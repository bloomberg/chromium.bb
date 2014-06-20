// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/fake_bluetooth_device_client.h"

#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <algorithm>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/rand_util.h"
#include "base/stl_util.h"
#include "base/threading/worker_pool.h"
#include "base/time/time.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_bluetooth_adapter_client.h"
#include "chromeos/dbus/fake_bluetooth_agent_manager_client.h"
#include "chromeos/dbus/fake_bluetooth_agent_service_provider.h"
#include "chromeos/dbus/fake_bluetooth_gatt_service_client.h"
#include "chromeos/dbus/fake_bluetooth_input_client.h"
#include "chromeos/dbus/fake_bluetooth_profile_manager_client.h"
#include "chromeos/dbus/fake_bluetooth_profile_service_provider.h"
#include "dbus/file_descriptor.h"
#include "dbus/object_path.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace {

// Default interval between simulated events.
const int kSimulationIntervalMs = 750;

// Minimum and maximum bounds for randomly generated RSSI values.
const int kMinRSSI = -90;
const int kMaxRSSI = -30;


void SimulatedProfileSocket(int fd) {
  // Simulate a server-side socket of a profile; read data from the socket,
  // write it back, and then close.
  char buf[1024];
  ssize_t len;
  ssize_t count;

  len = read(fd, buf, sizeof buf);
  if (len < 0) {
    close(fd);
    return;
  }

  count = len;
  len = write(fd, buf, count);
  if (len < 0) {
    close(fd);
    return;
  }

  close(fd);
}

void SimpleErrorCallback(const std::string& error_name,
                         const std::string& error_message) {
  VLOG(1) << "Bluetooth Error: " << error_name << ": " << error_message;
}

}  // namespace

namespace chromeos {

const char FakeBluetoothDeviceClient::kPairedDevicePath[] =
    "/fake/hci0/dev0";
const char FakeBluetoothDeviceClient::kPairedDeviceAddress[] =
    "00:11:22:33:44:55";
const char FakeBluetoothDeviceClient::kPairedDeviceName[] =
    "Fake Device";
const uint32 FakeBluetoothDeviceClient::kPairedDeviceClass = 0x000104;

const char FakeBluetoothDeviceClient::kLegacyAutopairPath[] =
    "/fake/hci0/dev1";
const char FakeBluetoothDeviceClient::kLegacyAutopairAddress[] =
    "28:CF:DA:00:00:00";
const char FakeBluetoothDeviceClient::kLegacyAutopairName[] =
    "Bluetooth 2.0 Mouse";
const uint32 FakeBluetoothDeviceClient::kLegacyAutopairClass = 0x002580;

const char FakeBluetoothDeviceClient::kDisplayPinCodePath[] =
    "/fake/hci0/dev2";
const char FakeBluetoothDeviceClient::kDisplayPinCodeAddress[] =
    "28:37:37:00:00:00";
const char FakeBluetoothDeviceClient::kDisplayPinCodeName[] =
    "Bluetooth 2.0 Keyboard";
const uint32 FakeBluetoothDeviceClient::kDisplayPinCodeClass = 0x002540;

const char FakeBluetoothDeviceClient::kVanishingDevicePath[] =
    "/fake/hci0/dev3";
const char FakeBluetoothDeviceClient::kVanishingDeviceAddress[] =
    "01:02:03:04:05:06";
const char FakeBluetoothDeviceClient::kVanishingDeviceName[] =
    "Vanishing Device";
const uint32 FakeBluetoothDeviceClient::kVanishingDeviceClass = 0x000104;

const char FakeBluetoothDeviceClient::kConnectUnpairablePath[] =
    "/fake/hci0/dev4";
const char FakeBluetoothDeviceClient::kConnectUnpairableAddress[] =
    "7C:ED:8D:00:00:00";
const char FakeBluetoothDeviceClient::kConnectUnpairableName[] =
    "Unpairable Device";
const uint32 FakeBluetoothDeviceClient::kConnectUnpairableClass = 0x002580;

const char FakeBluetoothDeviceClient::kDisplayPasskeyPath[] =
    "/fake/hci0/dev5";
const char FakeBluetoothDeviceClient::kDisplayPasskeyAddress[] =
    "00:0F:F6:00:00:00";
const char FakeBluetoothDeviceClient::kDisplayPasskeyName[] =
    "Bluetooth 2.1+ Keyboard";
const uint32 FakeBluetoothDeviceClient::kDisplayPasskeyClass = 0x002540;

const char FakeBluetoothDeviceClient::kRequestPinCodePath[] =
    "/fake/hci0/dev6";
const char FakeBluetoothDeviceClient::kRequestPinCodeAddress[] =
    "00:24:BE:00:00:00";
const char FakeBluetoothDeviceClient::kRequestPinCodeName[] =
    "PIN Device";
const uint32 FakeBluetoothDeviceClient::kRequestPinCodeClass = 0x240408;

const char FakeBluetoothDeviceClient::kConfirmPasskeyPath[] =
    "/fake/hci0/dev7";
const char FakeBluetoothDeviceClient::kConfirmPasskeyAddress[] =
    "20:7D:74:00:00:00";
const char FakeBluetoothDeviceClient::kConfirmPasskeyName[] =
    "Phone";
const uint32 FakeBluetoothDeviceClient::kConfirmPasskeyClass = 0x7a020c;

const char FakeBluetoothDeviceClient::kRequestPasskeyPath[] =
    "/fake/hci0/dev8";
const char FakeBluetoothDeviceClient::kRequestPasskeyAddress[] =
    "20:7D:74:00:00:01";
const char FakeBluetoothDeviceClient::kRequestPasskeyName[] =
    "Passkey Device";
const uint32 FakeBluetoothDeviceClient::kRequestPasskeyClass = 0x7a020c;

const char FakeBluetoothDeviceClient::kUnconnectableDevicePath[] =
    "/fake/hci0/dev9";
const char FakeBluetoothDeviceClient::kUnconnectableDeviceAddress[] =
    "20:7D:74:00:00:02";
const char FakeBluetoothDeviceClient::kUnconnectableDeviceName[] =
    "Unconnectable Device";
const uint32 FakeBluetoothDeviceClient::kUnconnectableDeviceClass = 0x7a020c;

const char FakeBluetoothDeviceClient::kUnpairableDevicePath[] =
    "/fake/hci0/devA";
const char FakeBluetoothDeviceClient::kUnpairableDeviceAddress[] =
    "20:7D:74:00:00:03";
const char FakeBluetoothDeviceClient::kUnpairableDeviceName[] =
    "Unpairable Device";
const uint32 FakeBluetoothDeviceClient::kUnpairableDeviceClass = 0x002540;

const char FakeBluetoothDeviceClient::kJustWorksPath[] =
    "/fake/hci0/devB";
const char FakeBluetoothDeviceClient::kJustWorksAddress[] =
    "00:0C:8A:00:00:00";
const char FakeBluetoothDeviceClient::kJustWorksName[] =
    "Just-Works Device";
const uint32 FakeBluetoothDeviceClient::kJustWorksClass = 0x240428;

const char FakeBluetoothDeviceClient::kLowEnergyPath[] =
    "/fake/hci0/devC";
const char FakeBluetoothDeviceClient::kLowEnergyAddress[] =
    "00:1A:11:00:15:30";
const char FakeBluetoothDeviceClient::kLowEnergyName[] =
    "Bluetooth 4.0 Heart Rate Monitor";
const uint32 FakeBluetoothDeviceClient::kLowEnergyClass =
    0x000918;  // Major class "Health", Minor class "Heart/Pulse Rate Monitor."

FakeBluetoothDeviceClient::Properties::Properties(
    const PropertyChangedCallback& callback)
    : BluetoothDeviceClient::Properties(
          NULL,
          bluetooth_device::kBluetoothDeviceInterface,
          callback) {
}

FakeBluetoothDeviceClient::Properties::~Properties() {
}

void FakeBluetoothDeviceClient::Properties::Get(
    dbus::PropertyBase* property,
    dbus::PropertySet::GetCallback callback) {
  VLOG(1) << "Get " << property->name();
  callback.Run(false);
}

void FakeBluetoothDeviceClient::Properties::GetAll() {
  VLOG(1) << "GetAll";
}

void FakeBluetoothDeviceClient::Properties::Set(
    dbus::PropertyBase *property,
    dbus::PropertySet::SetCallback callback) {
  VLOG(1) << "Set " << property->name();
  if (property->name() == trusted.name()) {
    callback.Run(true);
    property->ReplaceValueWithSetValue();
  } else {
    callback.Run(false);
  }
}

FakeBluetoothDeviceClient::FakeBluetoothDeviceClient()
    : simulation_interval_ms_(kSimulationIntervalMs),
      discovery_simulation_step_(0),
      incoming_pairing_simulation_step_(0),
      pairing_cancelled_(false),
      connection_monitor_started_(false) {
  Properties* properties = new Properties(base::Bind(
      &FakeBluetoothDeviceClient::OnPropertyChanged,
      base::Unretained(this),
      dbus::ObjectPath(kPairedDevicePath)));
  properties->address.ReplaceValue(kPairedDeviceAddress);
  properties->bluetooth_class.ReplaceValue(kPairedDeviceClass);
  properties->name.ReplaceValue("Fake Device (Name)");
  properties->alias.ReplaceValue(kPairedDeviceName);
  properties->paired.ReplaceValue(true);
  properties->trusted.ReplaceValue(true);
  properties->adapter.ReplaceValue(
      dbus::ObjectPath(FakeBluetoothAdapterClient::kAdapterPath));

  std::vector<std::string> uuids;
  uuids.push_back("00001800-0000-1000-8000-00805f9b34fb");
  uuids.push_back("00001801-0000-1000-8000-00805f9b34fb");
  properties->uuids.ReplaceValue(uuids);

  properties->modalias.ReplaceValue("usb:v05ACp030Dd0306");

  properties_map_[dbus::ObjectPath(kPairedDevicePath)] = properties;
  device_list_.push_back(dbus::ObjectPath(kPairedDevicePath));
}

FakeBluetoothDeviceClient::~FakeBluetoothDeviceClient() {
  // Clean up Properties structures
  STLDeleteValues(&properties_map_);
}

void FakeBluetoothDeviceClient::Init(dbus::Bus* bus) {
}

void FakeBluetoothDeviceClient::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void FakeBluetoothDeviceClient::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

std::vector<dbus::ObjectPath> FakeBluetoothDeviceClient::GetDevicesForAdapter(
    const dbus::ObjectPath& adapter_path) {
  if (adapter_path ==
      dbus::ObjectPath(FakeBluetoothAdapterClient::kAdapterPath))
    return device_list_;
  else
    return std::vector<dbus::ObjectPath>();
}

FakeBluetoothDeviceClient::Properties*
FakeBluetoothDeviceClient::GetProperties(const dbus::ObjectPath& object_path) {
  PropertiesMap::iterator iter = properties_map_.find(object_path);
  if (iter != properties_map_.end())
    return iter->second;
  return NULL;
}

void FakeBluetoothDeviceClient::Connect(
    const dbus::ObjectPath& object_path,
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  VLOG(1) << "Connect: " << object_path.value();
  Properties* properties = GetProperties(object_path);

  if (properties->connected.value() == true) {
    // Already connected.
    callback.Run();
    return;
  }

  if (properties->paired.value() != true &&
      object_path != dbus::ObjectPath(kConnectUnpairablePath) &&
      object_path != dbus::ObjectPath(kLowEnergyPath)) {
    // Must be paired.
    error_callback.Run(bluetooth_device::kErrorFailed, "Not paired");
    return;
  } else if (properties->paired.value() == true &&
             object_path == dbus::ObjectPath(kUnconnectableDevicePath)) {
    // Must not be paired
    error_callback.Run(bluetooth_device::kErrorFailed,
                       "Connection fails while paired");
    return;
  }

  // The device can be connected.
  properties->connected.ReplaceValue(true);
  callback.Run();

  // Expose GATT services if connected to LE device.
  if (object_path == dbus::ObjectPath(kLowEnergyPath)) {
    FakeBluetoothGattServiceClient* gatt_service_client =
        static_cast<FakeBluetoothGattServiceClient*>(
            DBusThreadManager::Get()->GetBluetoothGattServiceClient());
    gatt_service_client->ExposeHeartRateService(
        dbus::ObjectPath(kLowEnergyPath));
  }

  AddInputDeviceIfNeeded(object_path, properties);
}

void FakeBluetoothDeviceClient::Disconnect(
    const dbus::ObjectPath& object_path,
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  VLOG(1) << "Disconnect: " << object_path.value();
  Properties* properties = GetProperties(object_path);

  if (!properties->connected.value()) {
    error_callback.Run("org.bluez.Error.NotConnected", "Not Connected");
    return;
  }

  // Hide the Heart Rate Service if disconnected from LE device.
  if (object_path == dbus::ObjectPath(kLowEnergyPath)) {
    FakeBluetoothGattServiceClient* gatt_service_client =
        static_cast<FakeBluetoothGattServiceClient*>(
            DBusThreadManager::Get()->GetBluetoothGattServiceClient());
    gatt_service_client->HideHeartRateService();
  }

  callback.Run();
  properties->connected.ReplaceValue(false);
}

void FakeBluetoothDeviceClient::ConnectProfile(
    const dbus::ObjectPath& object_path,
    const std::string& uuid,
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  VLOG(1) << "ConnectProfile: " << object_path.value() << " " << uuid;

  FakeBluetoothProfileManagerClient* fake_bluetooth_profile_manager_client =
      static_cast<FakeBluetoothProfileManagerClient*>(
          DBusThreadManager::Get()->GetBluetoothProfileManagerClient());
  FakeBluetoothProfileServiceProvider* profile_service_provider =
      fake_bluetooth_profile_manager_client->GetProfileServiceProvider(uuid);
  if (profile_service_provider == NULL) {
    error_callback.Run(kNoResponseError, "Missing profile");
    return;
  }

  // Make a socket pair of a compatible type with the type used by Bluetooth;
  // spin up a thread to simulate the server side and wrap the client side in
  // a D-Bus file descriptor object.
  int socket_type = SOCK_STREAM;
  if (uuid == FakeBluetoothProfileManagerClient::kL2capUuid)
    socket_type = SOCK_SEQPACKET;

  int fds[2];
  if (socketpair(AF_UNIX, socket_type, 0, fds) < 0) {
    error_callback.Run(kNoResponseError, "socketpair call failed");
    return;
  }

  int args;
  args = fcntl(fds[1], F_GETFL, NULL);
  if (args < 0) {
    error_callback.Run(kNoResponseError, "failed to get socket flags");
    return;
  }

  args |= O_NONBLOCK;
  if (fcntl(fds[1], F_SETFL, args) < 0) {
    error_callback.Run(kNoResponseError, "failed to set socket non-blocking");
    return;
  }

  base::WorkerPool::GetTaskRunner(false)->PostTask(
      FROM_HERE,
      base::Bind(&SimulatedProfileSocket,
                 fds[0]));

  scoped_ptr<dbus::FileDescriptor> fd(new dbus::FileDescriptor(fds[1]));

  // Post the new connection to the service provider.
  BluetoothProfileServiceProvider::Delegate::Options options;

  profile_service_provider->NewConnection(
      object_path,
      fd.Pass(),
      options,
      base::Bind(&FakeBluetoothDeviceClient::ConnectionCallback,
                 base::Unretained(this),
                 object_path,
                 callback,
                 error_callback));
}

void FakeBluetoothDeviceClient::DisconnectProfile(
    const dbus::ObjectPath& object_path,
    const std::string& uuid,
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  VLOG(1) << "DisconnectProfile: " << object_path.value() << " " << uuid;

  FakeBluetoothProfileManagerClient* fake_bluetooth_profile_manager_client =
      static_cast<FakeBluetoothProfileManagerClient*>(
          DBusThreadManager::Get()->GetBluetoothProfileManagerClient());
  FakeBluetoothProfileServiceProvider* profile_service_provider =
      fake_bluetooth_profile_manager_client->GetProfileServiceProvider(uuid);
  if (profile_service_provider == NULL) {
    error_callback.Run(kNoResponseError, "Missing profile");
    return;
  }

  profile_service_provider->RequestDisconnection(
      object_path,
      base::Bind(&FakeBluetoothDeviceClient::DisconnectionCallback,
                 base::Unretained(this),
                 object_path,
                 callback,
                 error_callback));
}

void FakeBluetoothDeviceClient::Pair(
    const dbus::ObjectPath& object_path,
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  VLOG(1) << "Pair: " << object_path.value();
  Properties* properties = GetProperties(object_path);

  if (properties->paired.value() == true) {
    // Already paired.
    callback.Run();
    return;
  }

  SimulatePairing(object_path, false, callback, error_callback);
}

void FakeBluetoothDeviceClient::CancelPairing(
    const dbus::ObjectPath& object_path,
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  VLOG(1) << "CancelPairing: " << object_path.value();
  pairing_cancelled_ = true;
  callback.Run();
}

void FakeBluetoothDeviceClient::StartConnectionMonitor(
    const dbus::ObjectPath& object_path,
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  VLOG(1) << "StartConnectionMonitor: " << object_path.value();
  connection_monitor_started_ = true;
  callback.Run();
}

void FakeBluetoothDeviceClient::StopConnectionMonitor(
    const dbus::ObjectPath& object_path,
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  connection_monitor_started_ = false;
  callback.Run();
}

void FakeBluetoothDeviceClient::BeginDiscoverySimulation(
    const dbus::ObjectPath& adapter_path) {
  VLOG(1) << "starting discovery simulation";

  discovery_simulation_step_ = 1;

  base::MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&FakeBluetoothDeviceClient::DiscoverySimulationTimer,
                 base::Unretained(this)),
      base::TimeDelta::FromMilliseconds(simulation_interval_ms_));
}

void FakeBluetoothDeviceClient::EndDiscoverySimulation(
    const dbus::ObjectPath& adapter_path) {
  VLOG(1) << "stopping discovery simulation";
  discovery_simulation_step_ = 0;
}

void FakeBluetoothDeviceClient::BeginIncomingPairingSimulation(
    const dbus::ObjectPath& adapter_path) {
  VLOG(1) << "starting incoming pairing simulation";

  incoming_pairing_simulation_step_ = 1;

  base::MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&FakeBluetoothDeviceClient::IncomingPairingSimulationTimer,
                 base::Unretained(this)),
      base::TimeDelta::FromMilliseconds(30 * simulation_interval_ms_));
}

void FakeBluetoothDeviceClient::EndIncomingPairingSimulation(
    const dbus::ObjectPath& adapter_path) {
  VLOG(1) << "stopping incoming pairing simulation";
  incoming_pairing_simulation_step_ = 0;
}

void FakeBluetoothDeviceClient::SetSimulationIntervalMs(int interval_ms) {
  simulation_interval_ms_ = interval_ms;
}

void FakeBluetoothDeviceClient::CreateDevice(
    const dbus::ObjectPath& adapter_path,
    const dbus::ObjectPath& device_path) {
  if (std::find(device_list_.begin(),
                device_list_.end(), device_path) != device_list_.end())
    return;

  Properties* properties = new Properties(base::Bind(
      &FakeBluetoothDeviceClient::OnPropertyChanged,
      base::Unretained(this),
      device_path));
  properties->adapter.ReplaceValue(adapter_path);

  if (device_path == dbus::ObjectPath(kLegacyAutopairPath)) {
    properties->address.ReplaceValue(kLegacyAutopairAddress);
    properties->bluetooth_class.ReplaceValue(kLegacyAutopairClass);
    properties->name.ReplaceValue("LegacyAutopair");
    properties->alias.ReplaceValue(kLegacyAutopairName);

    std::vector<std::string> uuids;
    uuids.push_back("00001124-0000-1000-8000-00805f9b34fb");
    properties->uuids.ReplaceValue(uuids);

  } else if (device_path == dbus::ObjectPath(kDisplayPinCodePath)) {
    properties->address.ReplaceValue(kDisplayPinCodeAddress);
    properties->bluetooth_class.ReplaceValue(kDisplayPinCodeClass);
    properties->name.ReplaceValue("DisplayPinCode");
    properties->alias.ReplaceValue(kDisplayPinCodeName);

    std::vector<std::string> uuids;
    uuids.push_back("00001124-0000-1000-8000-00805f9b34fb");
    properties->uuids.ReplaceValue(uuids);

  } else if (device_path == dbus::ObjectPath(kVanishingDevicePath)) {
    properties->address.ReplaceValue(kVanishingDeviceAddress);
    properties->bluetooth_class.ReplaceValue(kVanishingDeviceClass);
    properties->name.ReplaceValue("VanishingDevice");
    properties->alias.ReplaceValue(kVanishingDeviceName);

  } else if (device_path == dbus::ObjectPath(kConnectUnpairablePath)) {
    properties->address.ReplaceValue(kConnectUnpairableAddress);
    properties->bluetooth_class.ReplaceValue(kConnectUnpairableClass);
    properties->name.ReplaceValue("ConnectUnpairable");
    properties->alias.ReplaceValue(kConnectUnpairableName);

    std::vector<std::string> uuids;
    uuids.push_back("00001124-0000-1000-8000-00805f9b34fb");
    properties->uuids.ReplaceValue(uuids);

  } else if (device_path == dbus::ObjectPath(kDisplayPasskeyPath)) {
    properties->address.ReplaceValue(kDisplayPasskeyAddress);
    properties->bluetooth_class.ReplaceValue(kDisplayPasskeyClass);
    properties->name.ReplaceValue("DisplayPasskey");
    properties->alias.ReplaceValue(kDisplayPasskeyName);

    std::vector<std::string> uuids;
    uuids.push_back("00001124-0000-1000-8000-00805f9b34fb");
    properties->uuids.ReplaceValue(uuids);

  } else if (device_path == dbus::ObjectPath(kRequestPinCodePath)) {
    properties->address.ReplaceValue(kRequestPinCodeAddress);
    properties->bluetooth_class.ReplaceValue(kRequestPinCodeClass);
    properties->name.ReplaceValue("RequestPinCode");
    properties->alias.ReplaceValue(kRequestPinCodeName);

  } else if (device_path == dbus::ObjectPath(kConfirmPasskeyPath)) {
    properties->address.ReplaceValue(kConfirmPasskeyAddress);
    properties->bluetooth_class.ReplaceValue(kConfirmPasskeyClass);
    properties->name.ReplaceValue("ConfirmPasskey");
    properties->alias.ReplaceValue(kConfirmPasskeyName);

  } else if (device_path == dbus::ObjectPath(kRequestPasskeyPath)) {
    properties->address.ReplaceValue(kRequestPasskeyAddress);
    properties->bluetooth_class.ReplaceValue(kRequestPasskeyClass);
    properties->name.ReplaceValue("RequestPasskey");
    properties->alias.ReplaceValue(kRequestPasskeyName);

  } else if (device_path == dbus::ObjectPath(kUnconnectableDevicePath)) {
    properties->address.ReplaceValue(kUnconnectableDeviceAddress);
    properties->bluetooth_class.ReplaceValue(kUnconnectableDeviceClass);
    properties->name.ReplaceValue("UnconnectableDevice");
    properties->alias.ReplaceValue(kUnconnectableDeviceName);

  } else if (device_path == dbus::ObjectPath(kUnpairableDevicePath)) {
    properties->address.ReplaceValue(kUnpairableDeviceAddress);
    properties->bluetooth_class.ReplaceValue(kUnpairableDeviceClass);
    properties->name.ReplaceValue("Fake Unpairable Device");
    properties->alias.ReplaceValue(kUnpairableDeviceName);

  } else if (device_path == dbus::ObjectPath(kJustWorksPath)) {
    properties->address.ReplaceValue(kJustWorksAddress);
    properties->bluetooth_class.ReplaceValue(kJustWorksClass);
    properties->name.ReplaceValue("JustWorks");
    properties->alias.ReplaceValue(kJustWorksName);

  } else if (device_path == dbus::ObjectPath(kLowEnergyPath)) {
    properties->address.ReplaceValue(kLowEnergyAddress);
    properties->bluetooth_class.ReplaceValue(kLowEnergyClass);
    properties->name.ReplaceValue("Heart Rate Monitor");
    properties->alias.ReplaceValue(kLowEnergyName);

    std::vector<std::string> uuids;
    uuids.push_back(FakeBluetoothGattServiceClient::kHeartRateServiceUUID);
    properties->uuids.ReplaceValue(uuids);

  } else {
    NOTREACHED();

  }

  properties_map_[device_path] = properties;
  device_list_.push_back(device_path);
  FOR_EACH_OBSERVER(BluetoothDeviceClient::Observer, observers_,
                    DeviceAdded(device_path));
}

void FakeBluetoothDeviceClient::RemoveDevice(
    const dbus::ObjectPath& adapter_path,
    const dbus::ObjectPath& device_path) {
  std::vector<dbus::ObjectPath>::iterator listiter =
      std::find(device_list_.begin(), device_list_.end(), device_path);
  if (listiter == device_list_.end())
    return;

  PropertiesMap::iterator iter = properties_map_.find(device_path);
  Properties* properties = iter->second;

  VLOG(1) << "removing device: " << properties->alias.value();
  device_list_.erase(listiter);

  // Remove the Input interface if it exists. This should be called before the
  // BluetoothDeviceClient::Observer::DeviceRemoved because it deletes the
  // BluetoothDeviceChromeOS object, including the device_path referenced here.
  FakeBluetoothInputClient* fake_bluetooth_input_client =
      static_cast<FakeBluetoothInputClient*>(
          DBusThreadManager::Get()->GetBluetoothInputClient());
  fake_bluetooth_input_client->RemoveInputDevice(device_path);

  if (device_path == dbus::ObjectPath(kLowEnergyPath)) {
    FakeBluetoothGattServiceClient* gatt_service_client =
        static_cast<FakeBluetoothGattServiceClient*>(
            DBusThreadManager::Get()->GetBluetoothGattServiceClient());
    gatt_service_client->HideHeartRateService();
  }

  FOR_EACH_OBSERVER(BluetoothDeviceClient::Observer, observers_,
                    DeviceRemoved(device_path));

  delete properties;
  properties_map_.erase(iter);
}

void FakeBluetoothDeviceClient::OnPropertyChanged(
    const dbus::ObjectPath& object_path,
    const std::string& property_name) {
  VLOG(2) << "Fake Bluetooth device property changed: " << object_path.value()
          << ": " << property_name;
  FOR_EACH_OBSERVER(BluetoothDeviceClient::Observer, observers_,
                    DevicePropertyChanged(object_path, property_name));
}

void FakeBluetoothDeviceClient::DiscoverySimulationTimer() {
  if (!discovery_simulation_step_)
    return;

  // Timer fires every .75s, the numbers below are arbitrary to give a feel
  // for a discovery process.
  VLOG(1) << "discovery simulation, step " << discovery_simulation_step_;
  if (discovery_simulation_step_ == 2) {
    CreateDevice(dbus::ObjectPath(FakeBluetoothAdapterClient::kAdapterPath),
                 dbus::ObjectPath(kLegacyAutopairPath));
    CreateDevice(dbus::ObjectPath(FakeBluetoothAdapterClient::kAdapterPath),
                 dbus::ObjectPath(kLowEnergyPath));

  } else if (discovery_simulation_step_ == 4) {
    UpdateDeviceRSSI(dbus::ObjectPath(kLowEnergyPath),
                     base::RandInt(kMinRSSI, kMaxRSSI));
    CreateDevice(dbus::ObjectPath(FakeBluetoothAdapterClient::kAdapterPath),
                 dbus::ObjectPath(kDisplayPinCodePath));
    CreateDevice(dbus::ObjectPath(FakeBluetoothAdapterClient::kAdapterPath),
                 dbus::ObjectPath(kVanishingDevicePath));

  } else if (discovery_simulation_step_ == 7) {
    CreateDevice(dbus::ObjectPath(FakeBluetoothAdapterClient::kAdapterPath),
                 dbus::ObjectPath(kConnectUnpairablePath));
    UpdateDeviceRSSI(dbus::ObjectPath(kLowEnergyPath),
                     base::RandInt(kMinRSSI, kMaxRSSI));

  } else if (discovery_simulation_step_ == 8) {
    CreateDevice(dbus::ObjectPath(FakeBluetoothAdapterClient::kAdapterPath),
                 dbus::ObjectPath(kDisplayPasskeyPath));
    CreateDevice(dbus::ObjectPath(FakeBluetoothAdapterClient::kAdapterPath),
                 dbus::ObjectPath(kRequestPinCodePath));
    UpdateDeviceRSSI(dbus::ObjectPath(kLowEnergyPath),
                     base::RandInt(kMinRSSI, kMaxRSSI));

  } else if (discovery_simulation_step_ == 10) {
    CreateDevice(dbus::ObjectPath(FakeBluetoothAdapterClient::kAdapterPath),
                 dbus::ObjectPath(kConfirmPasskeyPath));
    CreateDevice(dbus::ObjectPath(FakeBluetoothAdapterClient::kAdapterPath),
                 dbus::ObjectPath(kRequestPasskeyPath));
    CreateDevice(dbus::ObjectPath(FakeBluetoothAdapterClient::kAdapterPath),
                 dbus::ObjectPath(kUnconnectableDevicePath));
    CreateDevice(dbus::ObjectPath(FakeBluetoothAdapterClient::kAdapterPath),
                 dbus::ObjectPath(kUnpairableDevicePath));
    CreateDevice(dbus::ObjectPath(FakeBluetoothAdapterClient::kAdapterPath),
                 dbus::ObjectPath(kJustWorksPath));
    UpdateDeviceRSSI(dbus::ObjectPath(kLowEnergyPath),
                     base::RandInt(kMinRSSI, kMaxRSSI));

  } else if (discovery_simulation_step_ == 13) {
    UpdateDeviceRSSI(dbus::ObjectPath(kLowEnergyPath),
                     base::RandInt(kMinRSSI, kMaxRSSI));
    RemoveDevice(dbus::ObjectPath(FakeBluetoothAdapterClient::kAdapterPath),
                 dbus::ObjectPath(kVanishingDevicePath));

  } else if (discovery_simulation_step_ == 14) {
    UpdateDeviceRSSI(dbus::ObjectPath(kLowEnergyPath),
                     base::RandInt(kMinRSSI, kMaxRSSI));
    return;

  }

  ++discovery_simulation_step_;
  base::MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&FakeBluetoothDeviceClient::DiscoverySimulationTimer,
                 base::Unretained(this)),
      base::TimeDelta::FromMilliseconds(simulation_interval_ms_));
}

void FakeBluetoothDeviceClient::IncomingPairingSimulationTimer() {
  if (!incoming_pairing_simulation_step_)
    return;

  VLOG(1) << "incoming pairing simulation, step "
          << incoming_pairing_simulation_step_;
  switch (incoming_pairing_simulation_step_) {
    case 1:
      CreateDevice(dbus::ObjectPath(FakeBluetoothAdapterClient::kAdapterPath),
                   dbus::ObjectPath(kConfirmPasskeyPath));
      SimulatePairing(dbus::ObjectPath(kConfirmPasskeyPath), true,
                      base::Bind(&base::DoNothing),
                      base::Bind(&SimpleErrorCallback));
      break;
    case 2:
      CreateDevice(dbus::ObjectPath(FakeBluetoothAdapterClient::kAdapterPath),
                   dbus::ObjectPath(kJustWorksPath));
      SimulatePairing(dbus::ObjectPath(kJustWorksPath), true,
                      base::Bind(&base::DoNothing),
                      base::Bind(&SimpleErrorCallback));
      break;
    case 3:
      CreateDevice(dbus::ObjectPath(FakeBluetoothAdapterClient::kAdapterPath),
                   dbus::ObjectPath(kDisplayPinCodePath));
      SimulatePairing(dbus::ObjectPath(kDisplayPinCodePath), true,
                      base::Bind(&base::DoNothing),
                      base::Bind(&SimpleErrorCallback));
      break;
    case 4:
      CreateDevice(dbus::ObjectPath(FakeBluetoothAdapterClient::kAdapterPath),
                   dbus::ObjectPath(kDisplayPasskeyPath));
      SimulatePairing(dbus::ObjectPath(kDisplayPasskeyPath), true,
                      base::Bind(&base::DoNothing),
                      base::Bind(&SimpleErrorCallback));
      break;
    case 5:
      CreateDevice(dbus::ObjectPath(FakeBluetoothAdapterClient::kAdapterPath),
                   dbus::ObjectPath(kRequestPinCodePath));
      SimulatePairing(dbus::ObjectPath(kRequestPinCodePath), true,
                      base::Bind(&base::DoNothing),
                      base::Bind(&SimpleErrorCallback));
      break;
    case 6:
      CreateDevice(dbus::ObjectPath(FakeBluetoothAdapterClient::kAdapterPath),
                   dbus::ObjectPath(kRequestPasskeyPath));
      SimulatePairing(dbus::ObjectPath(kRequestPasskeyPath), true,
                      base::Bind(&base::DoNothing),
                      base::Bind(&SimpleErrorCallback));
      break;
    default:
      return;
  }

  ++incoming_pairing_simulation_step_;
  base::MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&FakeBluetoothDeviceClient::IncomingPairingSimulationTimer,
                 base::Unretained(this)),
      base::TimeDelta::FromMilliseconds(45 * simulation_interval_ms_));
}

void FakeBluetoothDeviceClient::SimulatePairing(
    const dbus::ObjectPath& object_path,
    bool incoming_request,
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  pairing_cancelled_ = false;

  FakeBluetoothAgentManagerClient* fake_bluetooth_agent_manager_client =
      static_cast<FakeBluetoothAgentManagerClient*>(
          DBusThreadManager::Get()->GetBluetoothAgentManagerClient());
  FakeBluetoothAgentServiceProvider* agent_service_provider =
      fake_bluetooth_agent_manager_client->GetAgentServiceProvider();
  CHECK(agent_service_provider != NULL);

  if (object_path == dbus::ObjectPath(kLegacyAutopairPath) ||
      object_path == dbus::ObjectPath(kConnectUnpairablePath) ||
      object_path == dbus::ObjectPath(kUnconnectableDevicePath) ||
      object_path == dbus::ObjectPath(kLowEnergyPath)) {
    // No need to call anything on the pairing delegate, just wait 3 times
    // the interval before acting as if the other end accepted it.
    base::MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&FakeBluetoothDeviceClient::CompleteSimulatedPairing,
                   base::Unretained(this),
                   object_path, callback, error_callback),
        base::TimeDelta::FromMilliseconds(3 * simulation_interval_ms_));

  } else if (object_path == dbus::ObjectPath(kDisplayPinCodePath)) {
    // Display a Pincode, and wait 7 times the interval before acting as
    // if the other end accepted it.
    agent_service_provider->DisplayPinCode(object_path, "123456");

    base::MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&FakeBluetoothDeviceClient::CompleteSimulatedPairing,
                   base::Unretained(this),
                   object_path, callback, error_callback),
        base::TimeDelta::FromMilliseconds(7 * simulation_interval_ms_));

  } else if (object_path == dbus::ObjectPath(kVanishingDevicePath)) {
    // The vanishing device simulates being too far away, and thus times out.
    base::MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&FakeBluetoothDeviceClient::TimeoutSimulatedPairing,
                   base::Unretained(this),
                   object_path, error_callback),
        base::TimeDelta::FromMilliseconds(4 * simulation_interval_ms_));

  } else if (object_path == dbus::ObjectPath(kDisplayPasskeyPath)) {
    // Display a passkey, and each interval act as if another key was entered
    // for it.
    agent_service_provider->DisplayPasskey(object_path, 123456, 0);

    base::MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&FakeBluetoothDeviceClient::SimulateKeypress,
                   base::Unretained(this),
                   1, object_path, callback, error_callback),
        base::TimeDelta::FromMilliseconds(simulation_interval_ms_));

  } else if (object_path == dbus::ObjectPath(kRequestPinCodePath)) {
    // Request a Pincode.
    agent_service_provider->RequestPinCode(
        object_path,
        base::Bind(&FakeBluetoothDeviceClient::PinCodeCallback,
                   base::Unretained(this),
                   object_path,
                   callback,
                   error_callback));

  } else if (object_path == dbus::ObjectPath(kConfirmPasskeyPath)) {
    // Request confirmation of a Passkey.
    agent_service_provider->RequestConfirmation(
        object_path, 123456,
        base::Bind(&FakeBluetoothDeviceClient::ConfirmationCallback,
                   base::Unretained(this),
                   object_path,
                   callback,
                   error_callback));

  } else if (object_path == dbus::ObjectPath(kRequestPasskeyPath)) {
    // Request a Passkey from the user.
    agent_service_provider->RequestPasskey(
        object_path,
        base::Bind(&FakeBluetoothDeviceClient::PasskeyCallback,
                   base::Unretained(this),
                   object_path,
                   callback,
                   error_callback));

  } else if (object_path == dbus::ObjectPath(kUnpairableDevicePath)) {
    // Fails the pairing with an org.bluez.Error.Failed error.
    base::MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&FakeBluetoothDeviceClient::FailSimulatedPairing,
                   base::Unretained(this),
                   object_path, error_callback),
        base::TimeDelta::FromMilliseconds(simulation_interval_ms_));

  } else if (object_path == dbus::ObjectPath(kJustWorksPath)) {
    if (incoming_request) {
      agent_service_provider->RequestAuthorization(
          object_path,
          base::Bind(&FakeBluetoothDeviceClient::ConfirmationCallback,
                     base::Unretained(this),
                     object_path,
                     callback,
                     error_callback));

    } else {
      // No need to call anything on the pairing delegate, just wait 3 times
      // the interval before acting as if the other end accepted it.
      base::MessageLoop::current()->PostDelayedTask(
          FROM_HERE,
          base::Bind(&FakeBluetoothDeviceClient::CompleteSimulatedPairing,
                     base::Unretained(this),
                     object_path, callback, error_callback),
          base::TimeDelta::FromMilliseconds(3 * simulation_interval_ms_));

    }

  } else {
    error_callback.Run(kNoResponseError, "No pairing fake");
  }
}

void FakeBluetoothDeviceClient::CompleteSimulatedPairing(
    const dbus::ObjectPath& object_path,
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  VLOG(1) << "CompleteSimulatedPairing: " << object_path.value();
  if (pairing_cancelled_) {
    pairing_cancelled_ = false;

    error_callback.Run(bluetooth_device::kErrorAuthenticationCanceled,
                       "Cancelled");
  } else {
    Properties* properties = GetProperties(object_path);

    properties->paired.ReplaceValue(true);
    callback.Run();

    AddInputDeviceIfNeeded(object_path, properties);
  }
}

void FakeBluetoothDeviceClient::TimeoutSimulatedPairing(
    const dbus::ObjectPath& object_path,
    const ErrorCallback& error_callback) {
  VLOG(1) << "TimeoutSimulatedPairing: " << object_path.value();

  error_callback.Run(bluetooth_device::kErrorAuthenticationTimeout,
                     "Timed out");
}

void FakeBluetoothDeviceClient::CancelSimulatedPairing(
    const dbus::ObjectPath& object_path,
    const ErrorCallback& error_callback) {
  VLOG(1) << "CancelSimulatedPairing: " << object_path.value();

  error_callback.Run(bluetooth_device::kErrorAuthenticationCanceled,
                     "Canceled");
}

void FakeBluetoothDeviceClient::RejectSimulatedPairing(
    const dbus::ObjectPath& object_path,
    const ErrorCallback& error_callback) {
  VLOG(1) << "RejectSimulatedPairing: " << object_path.value();

  error_callback.Run(bluetooth_device::kErrorAuthenticationRejected,
                     "Rejected");
}

void FakeBluetoothDeviceClient::FailSimulatedPairing(
    const dbus::ObjectPath& object_path,
    const ErrorCallback& error_callback) {
  VLOG(1) << "FailSimulatedPairing: " << object_path.value();

  error_callback.Run(bluetooth_device::kErrorFailed, "Failed");
}

void FakeBluetoothDeviceClient::AddInputDeviceIfNeeded(
    const dbus::ObjectPath& object_path,
    Properties* properties) {
  // If the paired device is a HID device based on it's bluetooth class,
  // simulate the Input interface.
  FakeBluetoothInputClient* fake_bluetooth_input_client =
      static_cast<FakeBluetoothInputClient*>(
          DBusThreadManager::Get()->GetBluetoothInputClient());

  if ((properties->bluetooth_class.value() & 0x001f03) == 0x000500)
    fake_bluetooth_input_client->AddInputDevice(object_path);
}

void FakeBluetoothDeviceClient::UpdateDeviceRSSI(
    const dbus::ObjectPath& object_path,
    int16 rssi) {
  PropertiesMap::iterator iter = properties_map_.find(object_path);
  if (iter == properties_map_.end()) {
    VLOG(2) << "Fake device does not exist: " << object_path.value();
    return;
  }
  Properties* properties = iter->second;
  DCHECK(properties);
  properties->rssi.ReplaceValue(rssi);
}

void FakeBluetoothDeviceClient::PinCodeCallback(
    const dbus::ObjectPath& object_path,
    const base::Closure& callback,
    const ErrorCallback& error_callback,
    BluetoothAgentServiceProvider::Delegate::Status status,
    const std::string& pincode) {
  VLOG(1) << "PinCodeCallback: " << object_path.value();

  if (status == BluetoothAgentServiceProvider::Delegate::SUCCESS) {
    base::MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&FakeBluetoothDeviceClient::CompleteSimulatedPairing,
                   base::Unretained(this),
                   object_path, callback, error_callback),
        base::TimeDelta::FromMilliseconds(3 * simulation_interval_ms_));

  } else if (status == BluetoothAgentServiceProvider::Delegate::CANCELLED) {
    base::MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&FakeBluetoothDeviceClient::CancelSimulatedPairing,
                   base::Unretained(this),
                   object_path, error_callback),
        base::TimeDelta::FromMilliseconds(simulation_interval_ms_));

  } else if (status == BluetoothAgentServiceProvider::Delegate::REJECTED) {
    base::MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&FakeBluetoothDeviceClient::RejectSimulatedPairing,
                   base::Unretained(this),
                   object_path, error_callback),
        base::TimeDelta::FromMilliseconds(simulation_interval_ms_));

  }
}

void FakeBluetoothDeviceClient::PasskeyCallback(
    const dbus::ObjectPath& object_path,
    const base::Closure& callback,
    const ErrorCallback& error_callback,
    BluetoothAgentServiceProvider::Delegate::Status status,
    uint32 passkey) {
  VLOG(1) << "PasskeyCallback: " << object_path.value();

  if (status == BluetoothAgentServiceProvider::Delegate::SUCCESS) {
    base::MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&FakeBluetoothDeviceClient::CompleteSimulatedPairing,
                   base::Unretained(this),
                   object_path, callback, error_callback),
        base::TimeDelta::FromMilliseconds(3 * simulation_interval_ms_));

  } else if (status == BluetoothAgentServiceProvider::Delegate::CANCELLED) {
    base::MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&FakeBluetoothDeviceClient::CancelSimulatedPairing,
                   base::Unretained(this),
                   object_path, error_callback),
        base::TimeDelta::FromMilliseconds(simulation_interval_ms_));

  } else if (status == BluetoothAgentServiceProvider::Delegate::REJECTED) {
    base::MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&FakeBluetoothDeviceClient::RejectSimulatedPairing,
                   base::Unretained(this),
                   object_path, error_callback),
        base::TimeDelta::FromMilliseconds(simulation_interval_ms_));

  }
}

void FakeBluetoothDeviceClient::ConfirmationCallback(
    const dbus::ObjectPath& object_path,
    const base::Closure& callback,
    const ErrorCallback& error_callback,
    BluetoothAgentServiceProvider::Delegate::Status status) {
  VLOG(1) << "ConfirmationCallback: " << object_path.value();

  if (status == BluetoothAgentServiceProvider::Delegate::SUCCESS) {
    base::MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&FakeBluetoothDeviceClient::CompleteSimulatedPairing,
                   base::Unretained(this),
                   object_path, callback, error_callback),
        base::TimeDelta::FromMilliseconds(3 * simulation_interval_ms_));

  } else if (status == BluetoothAgentServiceProvider::Delegate::CANCELLED) {
    base::MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&FakeBluetoothDeviceClient::CancelSimulatedPairing,
                   base::Unretained(this),
                   object_path, error_callback),
        base::TimeDelta::FromMilliseconds(simulation_interval_ms_));

  } else if (status == BluetoothAgentServiceProvider::Delegate::REJECTED) {
    base::MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&FakeBluetoothDeviceClient::RejectSimulatedPairing,
                   base::Unretained(this),
                   object_path, error_callback),
        base::TimeDelta::FromMilliseconds(simulation_interval_ms_));

  }
}

void FakeBluetoothDeviceClient::SimulateKeypress(
    uint16 entered,
    const dbus::ObjectPath& object_path,
    const base::Closure& callback,
    const ErrorCallback& error_callback) {
  VLOG(1) << "SimulateKeypress " << entered << ": " << object_path.value();

  FakeBluetoothAgentManagerClient* fake_bluetooth_agent_manager_client =
      static_cast<FakeBluetoothAgentManagerClient*>(
          DBusThreadManager::Get()->GetBluetoothAgentManagerClient());
  FakeBluetoothAgentServiceProvider* agent_service_provider =
      fake_bluetooth_agent_manager_client->GetAgentServiceProvider();

  // The agent service provider object could have been destroyed after the
  // pairing is canceled.
  if (!agent_service_provider)
    return;

  agent_service_provider->DisplayPasskey(object_path, 123456, entered);

  if (entered < 7) {
    base::MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&FakeBluetoothDeviceClient::SimulateKeypress,
                   base::Unretained(this),
                   entered + 1, object_path, callback, error_callback),
        base::TimeDelta::FromMilliseconds(simulation_interval_ms_));

  } else {
    base::MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&FakeBluetoothDeviceClient::CompleteSimulatedPairing,
                   base::Unretained(this),
                   object_path, callback, error_callback),
        base::TimeDelta::FromMilliseconds(simulation_interval_ms_));

  }
}

void FakeBluetoothDeviceClient::ConnectionCallback(
    const dbus::ObjectPath& object_path,
    const base::Closure& callback,
    const ErrorCallback& error_callback,
    BluetoothProfileServiceProvider::Delegate::Status status) {
  VLOG(1) << "ConnectionCallback: " << object_path.value();

  if (status == BluetoothProfileServiceProvider::Delegate::SUCCESS) {
    callback.Run();
  } else if (status == BluetoothProfileServiceProvider::Delegate::CANCELLED) {
    // TODO(keybuk): tear down this side of the connection
    error_callback.Run(bluetooth_device::kErrorFailed, "Canceled");
  } else if (status == BluetoothProfileServiceProvider::Delegate::REJECTED) {
    // TODO(keybuk): tear down this side of the connection
    error_callback.Run(bluetooth_device::kErrorFailed, "Rejected");
  }
}

void FakeBluetoothDeviceClient::DisconnectionCallback(
    const dbus::ObjectPath& object_path,
    const base::Closure& callback,
    const ErrorCallback& error_callback,
    BluetoothProfileServiceProvider::Delegate::Status status) {
  VLOG(1) << "DisconnectionCallback: " << object_path.value();

  if (status == BluetoothProfileServiceProvider::Delegate::SUCCESS) {
    // TODO(keybuk): tear down this side of the connection
    callback.Run();
  } else if (status == BluetoothProfileServiceProvider::Delegate::CANCELLED) {
    error_callback.Run(bluetooth_device::kErrorFailed, "Canceled");
  } else if (status == BluetoothProfileServiceProvider::Delegate::REJECTED) {
    error_callback.Run(bluetooth_device::kErrorFailed, "Rejected");
  }
}

}  // namespace chromeos
