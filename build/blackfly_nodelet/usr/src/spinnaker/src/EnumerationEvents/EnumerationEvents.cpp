//=============================================================================
// Copyright (c) 2001-2019 FLIR Systems, Inc. All Rights Reserved.
//
// This software is the confidential and proprietary information of FLIR
// Integrated Imaging Solutions, Inc. ("Confidential Information"). You
// shall not disclose such Confidential Information and shall use it only in
// accordance with the terms of the license agreement you entered into
// with FLIR Integrated Imaging Solutions, Inc. (FLIR).
//
// FLIR MAKES NO REPRESENTATIONS OR WARRANTIES ABOUT THE SUITABILITY OF THE
// SOFTWARE, EITHER EXPRESSED OR IMPLIED, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
// PURPOSE, OR NON-INFRINGEMENT. FLIR SHALL NOT BE LIABLE FOR ANY DAMAGES
// SUFFERED BY LICENSEE AS A RESULT OF USING, MODIFYING OR DISTRIBUTING
// THIS SOFTWARE OR ITS DERIVATIVES.
//=============================================================================

/**
 *	@example EnumerationEvents.cpp
 *
 *	@brief EnumerationEvents.cpp explores arrival and removal events on
 *	interfaces and the system. It relies on information provided in the
 *	Enumeration, Acquisition, and NodeMapInfo examples.
 *
 *	It can also be helpful to familiarize yourself with the NodeMapCallback
 *	example, as nodemap callbacks follow the same general procedure as
 *	events, but with a few less steps.
 *
 *	This example creates two user-defined classes: InterfaceEventHandler and
 *	SystemEventHandler. These child classes allow the user to define properties,
 *	parameters, and the event itself while the parent classes - ArrivalEvent,
 *	RemovalEvent, and InterfaceEvent - allow the child classes to interface
 *	with Spinnaker.
 */

#include "Spinnaker.h"
#include "SpinGenApi/SpinnakerGenApi.h"
#include <iostream>
#include <sstream>

using namespace Spinnaker;
using namespace Spinnaker::GenApi;
using namespace Spinnaker::GenICam;
using namespace std;

// This class defines the properties and functions for device arrivals and removals
// on an interface. Take special note of the signatures of the OnDeviceArrival()
// and OnDeviceRemoval() methods. Also, enumeration events must inherit at least
// one of the three event types - ArrivalEvent, RemovalEvent, and InterfaceEvent
// if they are to be registered to interfaces, the system, or both.
class InterfaceEventHandler : public InterfaceEvent
{
  public:
    //
    // Set the constructor and destructor
    //
    // *** NOTES ***
    // When constructing a generic InterfaceEventHandler to be registered to the system,
    // the handler will not have knowledge of which interface triggered the event callbacks.
    // On the other hand, InterfaceEventHandler does not need knowledge about the system if we
    // are constructing it to be registered to a specific interface.
    //
    InterfaceEventHandler(SystemPtr system)
        : m_system(system), m_interface(nullptr), m_interfaceNum(-1), m_registerToSystem(true){};

    InterfaceEventHandler(InterfacePtr iface, unsigned int interfaceNum)
        : m_system(nullptr), m_interface(iface), m_interfaceNum(interfaceNum), m_registerToSystem(false){};

    ~InterfaceEventHandler(){};

    // This function defines the arrival event on an interface. It prints out
    // the device serial number of the camera arriving and the interface
    // number. The argument is the serial number of the camera that triggered
    // the arrival event.
    // If event was constructed to be registered to the system as a generic
    // InterfaceEventHandler, then we just retrieve the number of cameras
    // currently connected on the system and print it out.
    void OnDeviceArrival(uint64_t deviceSerialNumber)
    {
        if (m_registerToSystem)
        {
            unsigned int count = m_system->GetCameras().GetSize();

            cout << "Generic interface event handler:" << endl;
            cout << "\tThere " << (count == 1 ? "is " : "are ") << count << (count == 1 ? " device " : " devices ")
                 << "on the system." << endl
                 << endl;
        }
        else
        {
            cout << "Interface event handler:" << endl;
            cout << "\tDevice " << deviceSerialNumber << " has arrived on interface " << m_interfaceNum << "." << endl
                 << endl;
        }
    }

    // This function defines removal events on an interface. It prints out the
    // device serial number of the camera being removed and the interface
    // number. The argument is the serial number of the camera that triggered
    // the removal event.
    // If event was constructed to be registered to the system as a generic
    // InterfaceEventHandler, then we just retrieve the number of cameras
    // currently connected on the system and print it out.
    void OnDeviceRemoval(uint64_t deviceSerialNumber)
    {
        if (m_registerToSystem)
        {
            try
            {
                // At this point the interface that the camera is connected to
                // may have been removed as well, so we need to handle
                // potential exceptions when calling GetCameras().
                unsigned int count = m_system->GetCameras().GetSize();
                cout << "Generic interface event handler:" << endl;
                cout << "\tThere " << (count == 1 ? "is " : "are ") << count << (count == 1 ? " device " : " devices ")
                     << "on the system." << endl
                     << endl;
            }
            catch (Spinnaker::Exception& e)
            {
                cout << "Error updating cameras: " << e.what() << endl;
            }
        }
        else
        {
            cout << "Interface event handler:" << endl;
            cout << "\tDevice " << deviceSerialNumber << " was removed from interface " << m_interfaceNum << "." << endl
                 << endl;
        }
    }

  private:
    SystemPtr m_system;
    InterfacePtr m_interface;
    unsigned int m_interfaceNum;

    bool m_registerToSystem;
};

// This class defines the properties and methods for interface arrivals and removals
// on the system. Take special note of the signatures of the OnInterfaceArrival()
// and OnInterfaceRemoval() methods. Interface enumeration events must inherit from
// SystemEvent.
class SystemEventHandler : public InterfaceArrivalEvent, public InterfaceRemovalEvent
{
  public:
    SystemEventHandler(SystemPtr system) : m_system(system){};
    ~SystemEventHandler(){};

    // This method defines the interface arrival event on the system.
    // It first prints the ID of the arriving interface, then
    // re-registers the interface events on the updated interface list.
    //
    // *** NOTES ***
    // When calling RegisterInterfaceEvent() with the updateInterface
    // flag set to true, the system will initialize a new interface list
    // if there are changes to the list (such as an interface arrival/removal).
    // This means that the newly initialized list of interfaces will no
    // longer have events registered to them, and so here, we register
    // all of our interface events again.
    void OnInterfaceArrival(std::string interfaceID)
    {
        cout << "System event handler:" << endl;
        cout << "\tInterface " << interfaceID << " has arrived on the system." << endl << endl;

        m_system->UpdateInterfaceList();

        InterfaceList interfaceList = m_system->GetInterfaces();
        unsigned int numInterfaces = interfaceList.GetSize();

        // Here we go through the new interface list to find the arrived interface
        // and display the cameras connected to the new interface.
        for (unsigned int ifaceIdx = 0; ifaceIdx < numInterfaces; ifaceIdx++)
        {
            // Select interface
            InterfacePtr pInterface = interfaceList.GetByIndex(ifaceIdx);
            INodeMap& nodeMap = pInterface->GetTLNodeMap();

            const CStringPtr interfaceIDNode = nodeMap.GetNode("InterfaceID");
            // Ensure the node is valid
            if (!IsAvailable(interfaceIDNode) || !IsReadable(interfaceIDNode))
            {
                continue;
            }

            if (strcmp(interfaceID.c_str(), interfaceIDNode->GetValue().c_str()) != 0)
            {
                continue;
            }

            CameraList cameraList = pInterface->GetCameras();
            unsigned int numCameras = cameraList.GetSize();
            for (unsigned int camIdx = 0; camIdx < numCameras; camIdx++)
            {
                CameraPtr pCam = cameraList.GetByIndex(camIdx);
                INodeMap& nodeMapTLDevice = pCam->GetTLDeviceNodeMap();
                CStringPtr ptrStringSerial = nodeMapTLDevice.GetNode("DeviceSerialNumber");
                if (IsAvailable(ptrStringSerial) && IsReadable(ptrStringSerial))
                {
                    gcstring deviceSerialNumber = ptrStringSerial->GetValue();
                    cout << "\tDevice " << deviceSerialNumber << " is connected to interface " << interfaceID << "."
                         << endl
                         << endl;
                }
            }
        }

        RegisterInterfaceEventToSystem();
        RegisterInterfaceEvents();
    }

    // This method defines the interface removal event on the system.
    // It first prints the ID of the interface removed, then
    // re-registers the interface events on the updated interface list.
    //
    // *** NOTES ***
    // When calling RegisterInterfaceEvent() with the updateInterface
    // flag set to true, the system will initialize a new interface list
    // if there are changes to the list (such as an interface arrival/removal).
    // This means that the newly initialized list of interfaces will no
    // longer have events registered to them, and so here, we register
    // all of our interface events again.
    void OnInterfaceRemoval(std::string interfaceID)
    {
        cout << "System event handler:" << endl;
        cout << "\tInterface " << interfaceID << " was removed from the system." << endl << endl;

        m_system->UpdateInterfaceList();

        RegisterInterfaceEventToSystem();
        RegisterInterfaceEvents();
    }

    void RegisterInterfaceEventToSystem()
    {
        if (!m_interfaceEventHandlerOnSystem)
        {
            //
            // Create interface event for the system
            //
            // *** NOTES ***
            // The InterfaceEventHandler has been constructed to accept a system object in
            // order to print the number of cameras on the system.
            //
            m_interfaceEventHandlerOnSystem = shared_ptr<InterfaceEventHandler>(new InterfaceEventHandler(m_system));
        }

        //
        // Register interface event for the system
        //
        // *** NOTES ***
        // Arrival, removal, and interface events can all be registered to
        // interfaces or the system. Do not think that interface events can only be
        // registered to an interface. An interface event is merely a combination
        // of an arrival and a removal event.
        //
        // *** LATER ***
        // Arrival, removal, and interface events must all be unregistered manually.
        // This must be done prior to releasing the system and while they are still
        // in scope.
        //
        m_system->RegisterInterfaceEvent(*m_interfaceEventHandlerOnSystem);
    }

    void UnregisterInterfaceEventFromSystem()
    {
        //
        // Unregister interface event from system object
        //
        // *** NOTES ***
        // It is important to unregister all arrival, removal, and interface events
        // registered to the system.
        //
        if (m_interfaceEventHandlerOnSystem)
        {
            m_system->UnregisterInterfaceEvent(*m_interfaceEventHandlerOnSystem);

            cout << "Interface event handler unregistered from system..." << endl;

            m_interfaceEventHandlerOnSystem = shared_ptr<InterfaceEventHandler>();
        }
    }

    void RegisterInterfaceEvents()
    {
        if (!m_interfaceEventHandlers.empty())
        {
            m_interfaceEventHandlers.clear();
        }

        InterfaceList interfaceList = m_system->GetInterfaces();
        unsigned int numInterfaces = interfaceList.GetSize();

        //
        // Create and register interface event to each interface
        //
        // *** NOTES ***
        // The process of event creation and registration on interfaces is similar
        // to the process of event creation and registration on the system. The
        // class for interfaces has been constructed to accept an interface and an
        // interface number (this is just to separate the interfaces).
        //
        // *** LATER ***
        // Arrival, removal, and interface events must all be unregistered manually.
        // This must be done prior to releasing the system and while they are still
        // in scope.
        //
        for (unsigned int i = 0; i < numInterfaces; i++)
        {
            // Select interface
            InterfacePtr pInterface = interfaceList.GetByIndex(i);

            // Create interface event
            shared_ptr<InterfaceEventHandler> interfaceEventHandler =
                shared_ptr<InterfaceEventHandler>(new InterfaceEventHandler(pInterface, i));
            m_interfaceEventHandlers.push_back(interfaceEventHandler);

            // Register interface event
            pInterface->RegisterEvent(*m_interfaceEventHandlers[i]);

            cout << "Event handler registered to interface " << i << "..." << endl;
        }
        cout << endl;
    }

    void UnregisterInterfaceEvents()
    {
        InterfaceList interfaceList = m_system->GetInterfaces(false);
        unsigned int numInterfaces = interfaceList.GetSize();

        //
        // Unregister interface event from each interface
        //
        // *** NOTES ***
        // It is important to unregister all arrival, removal, and interface events
        // from all interfaces that they may be registered to.
        //
        for (unsigned int i = 0; i < numInterfaces; i++)
        {
            interfaceList.GetByIndex(i)->UnregisterEvent(*m_interfaceEventHandlers[i]);
        }
        m_interfaceEventHandlers.clear();

        cout << "Event handler unregistered from interfaces..." << endl;
    }

  private:
    SystemPtr m_system;
    shared_ptr<InterfaceEventHandler> m_interfaceEventHandlerOnSystem;
    vector<shared_ptr<InterfaceEventHandler>> m_interfaceEventHandlers;
};

// This function checks if GEV enumeration is enabled on the system.
void CheckGevEnabled(SystemPtr& pSystem)
{
    // Retrieve the System TL Nodemap and EnumerateGEVInterfaces node
    INodeMap& nodeMap = pSystem->GetTLNodeMap();
    const CBooleanPtr enumerateGevInterfacesNode = nodeMap.GetNode("EnumerateGEVInterfaces");

    // Ensure the node is valid
    if (IsAvailable(enumerateGevInterfacesNode) && IsReadable(enumerateGevInterfacesNode))
    {
        const bool gevEnabled = enumerateGevInterfacesNode->GetValue();

        // Check if node is enabled
        if (!gevEnabled)
        {
            cout << endl
                 << "WARNING: GEV Enumeration is disabled." << endl
                 << "If you intend to use GigE cameras please run the EnableGEVInterfaces shortcut" << endl
                 << "or set EnumerateGEVInterfaces to true and relaunch your application." << endl
                 << endl;
        }
        else
        {
            cout << "EnumerateGEVInterfaces is enabled. Continuing.." << endl;
        }
    }
    else
    {
        cout << "EnumerateGEVInterfaces node is unavailable" << endl;
    }
}

// Example entry point; this function sets up the example to act appropriately
// upon arrival and removal events; please see Enumeration example for more
// in-depth comments on preparing and cleaning up the system.
int main(int /*argc*/, char** /*argv*/)
{
    // Print application build information
    cout << "Application build date: " << __DATE__ << " " << __TIME__ << endl << endl;

    // Retrieve singleton reference to system object
    SystemPtr system = System::GetInstance();

    // Print out current library version
    const LibraryVersion spinnakerLibraryVersion = system->GetLibraryVersion();
    cout << "Spinnaker library version: " << spinnakerLibraryVersion.major << "." << spinnakerLibraryVersion.minor
         << "." << spinnakerLibraryVersion.type << "." << spinnakerLibraryVersion.build << endl
         << endl;

    // Check if GEV enumeration is enabled.
    CheckGevEnabled(system);

    // Retrieve list of cameras from the system
    CameraList camList = system->GetCameras();

    unsigned int numCameras = camList.GetSize();

    cout << "Number of cameras detected: " << numCameras << endl << endl;

    //
    // Retrieve list of interfaces from the system
    //
    // *** NOTES ***
    // MacOS interfaces are only registered if they are active.
    // This example will have varied outcomes depending on the order
    // in which we receive the interface and the device arrival events.
    // In most cases on MacOS, since the interface and the device arrive
    // at the same time, we will see the interface arrival event and
    // the interface will already have one device connected to it.
    // This means we will not see another device arrival event.
    // Device removal events however, are guaranteed to be fired before
    // interface removal events.
    //
    InterfaceList interfaceList = system->GetInterfaces();

    unsigned int numInterfaces = interfaceList.GetSize();

    cout << "Number of interfaces detected: " << numInterfaces << endl << endl;

    cout << endl << "*** CONFIGURING ENUMERATION EVENTS ***" << endl << endl;

    //
    // Create system event
    //
    // *** NOTES ***
    // The SystemEventHandler has been constructed to accept a system object in
    // order to register/unregister events to/from the system object
    //
    SystemEventHandler systemEventHandler(system);

    //
    // Register system event to the system
    //
    // *** NOTES ***
    // A system event is merely a combination of an interface arrival and an
    // interface removal event.
    // This feature is currently only implemented for Linux/macOS platforms.
    //
    // *** LATER ***
    // Interface arrival and removal events must all be unregistered manually.
    // This must be done prior to releasing the system and while they are still
    // in scope.
    //
#if !defined(_WIN64) && !defined(_WIN32)
    system->RegisterEvent(systemEventHandler);
#endif

    systemEventHandler.RegisterInterfaceEventToSystem();
    systemEventHandler.RegisterInterfaceEvents();

    // Wait for user to plug in and/or remove camera devices
    cout << endl << "Ready! Remove/Plug in cameras to test or press Enter to exit..." << endl << endl;
    getchar();

    systemEventHandler.UnregisterInterfaceEvents();
    systemEventHandler.UnregisterInterfaceEventFromSystem();

    //
    // Unregister system event from system object
    //
    // *** NOTES ***
    // It is important to unregister all interface arrival and removal events
    // registered to the system.
    //
#if !defined(_WIN64) && !defined(_WIN32)
    system->UnregisterEvent(systemEventHandler);
#endif

    cout << "System event handler unregistered from system..." << endl;

    // Clear camera list before releasing system
    camList.Clear();

    // Clear interface list before releasing system
    interfaceList.Clear();

    // Release system
    system->ReleaseInstance();

    cout << endl << "Done! Press Enter to exit..." << endl;
    getchar();

    return 0;
}
