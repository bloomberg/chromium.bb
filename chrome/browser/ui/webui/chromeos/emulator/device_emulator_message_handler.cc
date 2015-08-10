// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/emulator/device_emulator_message_handler.h"

#include "ash/shell.h"
#include "ash/system/tray/system_tray_delegate.h"
#include "base/bind.h"
#include "base/values.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_bluetooth_adapter_client.h"
#include "chromeos/dbus/fake_bluetooth_device_client.h"
#include "chromeos/dbus/fake_power_manager_client.h"
#include "content/public/browser/web_ui.h"
#include "device/bluetooth/bluetooth_device_chromeos.h"

namespace {

// Define the name of the callback functions that will be used by JavaScript.
const char kBluetoothDiscoverFunction[] = "requestBluetoothDiscover";
const char kBluetoothPairFunction[] = "requestBluetoothPair";
const char kRequestBluetoothInfo[] = "requestBluetoothInfo";
const char kRequestPowerInfo[] = "requestPowerInfo";

// Define update functions that will update the power properties to the
// variables defined in the web UI.
const char kRemoveBluetoothDevice[] = "removeBluetoothDevice";
const char kUpdateBatteryPercent[] = "updateBatteryPercent";
const char kUpdateBatteryState[] = "updateBatteryState";
const char kUpdateExternalPower[] = "updateExternalPower";
const char kUpdateTimeToEmpty[] = "updateTimeToEmpty";
const char kUpdateTimeToFull[] = "updateTimeToFull";

// Define callback functions that will update the JavaScript variable
// and the web UI.
const char kAddBluetoothDeviceJSCallback[] =
    "device_emulator.bluetoothSettings.addBluetoothDevice";
const char kDevicePairedFromTrayJSCallback[] =
    "device_emulator.bluetoothSettings.devicePairedFromTray";
const char kDeviceRemovedFromMainAdapterJSCallback[] =
    "device_emulator.bluetoothSettings.deviceRemovedFromMainAdapter";
const char kPairFailedJSCallback[] =
    "device_emulator.bluetoothSettings.pairFailed";
const char kUpdateBluetoothInfoJSCallback[] =
    "device_emulator.bluetoothSettings.updateBluetoothInfo";
const char kUpdatePowerPropertiesJSCallback[] =
    "device_emulator.batterySettings.updatePowerProperties";

const char kPairedPropertyName[] = "Paired";

}  // namespace

namespace chromeos {

class DeviceEmulatorMessageHandler::BluetoothObserver
    : public BluetoothDeviceClient::Observer {
 public:
  explicit BluetoothObserver(DeviceEmulatorMessageHandler* owner)
      : owner_(owner) {
    owner_->fake_bluetooth_device_client_->AddObserver(this);
  }

  ~BluetoothObserver() override {
    owner_->fake_bluetooth_device_client_->RemoveObserver(this);
  }

  // chromeos::BluetoothDeviceClient::Observer.
  void DeviceAdded(const dbus::ObjectPath& object_path) override;

  // chromeos::BluetoothDeviceClient::Observer.
  void DevicePropertyChanged(const dbus::ObjectPath& object_path,
                             const std::string& property_name) override;

  // chromeos::BluetoothDeviceClient::Observer.
  void DeviceRemoved(const dbus::ObjectPath& object_path) override;

 private:
  DeviceEmulatorMessageHandler* owner_;

  DISALLOW_COPY_AND_ASSIGN(BluetoothObserver);
};

void DeviceEmulatorMessageHandler::BluetoothObserver::DeviceAdded(
    const dbus::ObjectPath& object_path) {
  scoped_ptr<base::DictionaryValue> device = owner_->GetDeviceInfo(
      object_path);

  // Request to add the device to the view's list of devices.
  owner_->web_ui()->CallJavascriptFunction(kAddBluetoothDeviceJSCallback,
      *device);
}

void DeviceEmulatorMessageHandler::BluetoothObserver::DevicePropertyChanged(
  const dbus::ObjectPath& object_path,
  const std::string& property_name) {
  if (property_name == kPairedPropertyName) {
    owner_->web_ui()->CallJavascriptFunction(kDevicePairedFromTrayJSCallback,
        base::StringValue(object_path.value()));
  }
}

void DeviceEmulatorMessageHandler::BluetoothObserver::DeviceRemoved(
    const dbus::ObjectPath& object_path) {
  owner_->web_ui()->CallJavascriptFunction(
    kDeviceRemovedFromMainAdapterJSCallback,
    base::StringValue(object_path.value()));
}

class DeviceEmulatorMessageHandler::PowerObserver
    : public PowerManagerClient::Observer {
 public:
  explicit PowerObserver(DeviceEmulatorMessageHandler* owner)
      : owner_(owner) {
    owner_->fake_power_manager_client_->AddObserver(this);
  }

  ~PowerObserver() override {
    owner_->fake_power_manager_client_->RemoveObserver(this);
  }

  void PowerChanged(
      const power_manager::PowerSupplyProperties& proto) override;

 private:
   DeviceEmulatorMessageHandler* owner_;

   DISALLOW_COPY_AND_ASSIGN(PowerObserver);
};

void DeviceEmulatorMessageHandler::PowerObserver::PowerChanged(
    const power_manager::PowerSupplyProperties& proto) {
  base::DictionaryValue power_properties;

  power_properties.SetInteger("battery_percent", proto.battery_percent());
  power_properties.SetInteger("battery_state", proto.battery_state());
  power_properties.SetInteger("external_power", proto.external_power());
  power_properties.SetInteger("battery_time_to_empty_sec",
                              proto.battery_time_to_empty_sec());
  power_properties.SetInteger("battery_time_to_full_sec",
                              proto.battery_time_to_full_sec());

  owner_->web_ui()->CallJavascriptFunction(kUpdatePowerPropertiesJSCallback,
                                   power_properties);
}

DeviceEmulatorMessageHandler::DeviceEmulatorMessageHandler()
    : fake_bluetooth_device_client_(
          static_cast<chromeos::FakeBluetoothDeviceClient*>(
              chromeos::DBusThreadManager::Get()
                  ->GetBluetoothDeviceClient())),
      fake_power_manager_client_(static_cast<chromeos::FakePowerManagerClient*>(
          chromeos::DBusThreadManager::Get()
              ->GetPowerManagerClient())) {}

DeviceEmulatorMessageHandler::~DeviceEmulatorMessageHandler() {
}

void DeviceEmulatorMessageHandler::Init() {
  bluetooth_observer_.reset(new BluetoothObserver(this));
  power_observer_.reset(new PowerObserver(this));
}

void DeviceEmulatorMessageHandler::RequestPowerInfo(
    const base::ListValue* args) {
  fake_power_manager_client_->RequestStatusUpdate();
}

void DeviceEmulatorMessageHandler::HandleRemoveBluetoothDevice(
    const base::ListValue* args) {
  std::string path;
  CHECK(args->GetString(0, &path));
  fake_bluetooth_device_client_->RemoveDevice(
      dbus::ObjectPath(FakeBluetoothAdapterClient::kAdapterPath),
      dbus::ObjectPath(path));
}

void DeviceEmulatorMessageHandler::HandleRequestBluetoothDiscover(
    const base::ListValue* args) {
  CreateBluetoothDeviceFromListValue(args);
}

void DeviceEmulatorMessageHandler::HandleRequestBluetoothInfo(
    const base::ListValue* args) {
  // Get a list containing paths of the devices which are connected to
  // the main adapter.
  std::vector<dbus::ObjectPath> paths =
      fake_bluetooth_device_client_->GetDevicesForAdapter(
          dbus::ObjectPath(chromeos::FakeBluetoothAdapterClient::kAdapterPath));

  base::ListValue devices;
  // Get each device's properties.
  for (const dbus::ObjectPath& path : paths) {
    scoped_ptr<base::DictionaryValue> device = GetDeviceInfo(path);
    devices.Append(device.Pass());
  }

  scoped_ptr<base::ListValue> predefined_devices =
      fake_bluetooth_device_client_->GetBluetoothDevicesAsDictionaries();

  base::ListValue pairing_method_options;
  pairing_method_options.AppendString(
      FakeBluetoothDeviceClient::kPairingMethodNone);
  pairing_method_options.AppendString(
      FakeBluetoothDeviceClient::kPairingMethodPinCode);
  pairing_method_options.AppendString(
      FakeBluetoothDeviceClient::kPairingMethodPassKey);

  base::ListValue pairing_action_options;
  pairing_action_options.AppendString(
      FakeBluetoothDeviceClient::kPairingActionDisplay);
  pairing_action_options.AppendString(
      FakeBluetoothDeviceClient::kPairingActionRequest);
  pairing_action_options.AppendString(
      FakeBluetoothDeviceClient::kPairingActionConfirmation);
  pairing_action_options.AppendString(
      FakeBluetoothDeviceClient::kPairingActionFail);

  // Send the list of devices to the view.
  web_ui()->CallJavascriptFunction(kUpdateBluetoothInfoJSCallback,
      *predefined_devices, devices, pairing_method_options,
      pairing_action_options);
}

void DeviceEmulatorMessageHandler::HandleRequestBluetoothPair(
    const base::ListValue* args) {
  // Create the device if it does not already exist.
  std::string path = CreateBluetoothDeviceFromListValue(args);
  chromeos::FakeBluetoothDeviceClient::Properties* props =
      fake_bluetooth_device_client_->GetProperties(dbus::ObjectPath(path));

  // Try to pair the device with the main adapter. The device is identified
  // by its device ID, which, in this case is the same as its address.
  ash::Shell::GetInstance()->system_tray_delegate()->ConnectToBluetoothDevice(
      props->address.value());
  if (!props->paired.value()) {
    web_ui()->CallJavascriptFunction(kPairFailedJSCallback,
        base::StringValue(path));
  }
}

void DeviceEmulatorMessageHandler::UpdateBatteryPercent(
    const base::ListValue* args) {
  int new_percent;
  if (args->GetInteger(0, &new_percent)) {
    power_manager::PowerSupplyProperties props =
        fake_power_manager_client_->props();
    props.set_battery_percent(new_percent);
    fake_power_manager_client_->UpdatePowerProperties(props);
  }
}

void DeviceEmulatorMessageHandler::UpdateBatteryState(
    const base::ListValue* args) {
  int battery_state;
  if (args->GetInteger(0, &battery_state)) {
    power_manager::PowerSupplyProperties props =
        fake_power_manager_client_->props();
    props.set_battery_state(
        static_cast<power_manager::PowerSupplyProperties_BatteryState>(
            battery_state));
    fake_power_manager_client_->UpdatePowerProperties(props);
  }
}

void DeviceEmulatorMessageHandler::UpdateExternalPower(
    const base::ListValue* args) {
  int power_source;
  if (args->GetInteger(0, &power_source)) {
    power_manager::PowerSupplyProperties props =
        fake_power_manager_client_->props();
    props.set_external_power(
        static_cast<power_manager::PowerSupplyProperties_ExternalPower>(
            power_source));
    fake_power_manager_client_->UpdatePowerProperties(props);
  }
}

void DeviceEmulatorMessageHandler::UpdateTimeToEmpty(
    const base::ListValue* args) {
  int new_time;
  if (args->GetInteger(0, &new_time)) {
    power_manager::PowerSupplyProperties props =
        fake_power_manager_client_->props();
    props.set_battery_time_to_empty_sec(new_time);
    fake_power_manager_client_->UpdatePowerProperties(props);
  }
}

void DeviceEmulatorMessageHandler::UpdateTimeToFull(
    const base::ListValue* args) {
  int new_time;
  if (args->GetInteger(0, &new_time)) {
    power_manager::PowerSupplyProperties props =
        fake_power_manager_client_->props();
    props.set_battery_time_to_full_sec(new_time);
    fake_power_manager_client_->UpdatePowerProperties(props);
  }
}

void DeviceEmulatorMessageHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      kRequestPowerInfo,
      base::Bind(&DeviceEmulatorMessageHandler::RequestPowerInfo,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      kUpdateBatteryPercent,
      base::Bind(&DeviceEmulatorMessageHandler::UpdateBatteryPercent,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      kUpdateBatteryState,
      base::Bind(&DeviceEmulatorMessageHandler::UpdateBatteryState,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      kUpdateExternalPower,
      base::Bind(&DeviceEmulatorMessageHandler::UpdateExternalPower,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      kUpdateTimeToEmpty,
      base::Bind(&DeviceEmulatorMessageHandler::UpdateTimeToEmpty,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      kUpdateTimeToFull,
      base::Bind(&DeviceEmulatorMessageHandler::UpdateTimeToFull,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      kBluetoothDiscoverFunction,
      base::Bind(&DeviceEmulatorMessageHandler::HandleRequestBluetoothDiscover,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      kBluetoothPairFunction,
      base::Bind(&DeviceEmulatorMessageHandler::HandleRequestBluetoothPair,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      kRequestBluetoothInfo,
      base::Bind(&DeviceEmulatorMessageHandler::HandleRequestBluetoothInfo,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      kRemoveBluetoothDevice,
      base::Bind(&DeviceEmulatorMessageHandler::HandleRemoveBluetoothDevice,
                 base::Unretained(this)));
}

std::string DeviceEmulatorMessageHandler::CreateBluetoothDeviceFromListValue(
    const base::ListValue* args) {
  const base::DictionaryValue* device_dict = nullptr;
  FakeBluetoothDeviceClient::IncomingDeviceProperties props;

  CHECK(args->GetDictionary(0, &device_dict));
  CHECK(device_dict->GetString("path", &props.device_path));
  CHECK(device_dict->GetString("name", &props.device_name));
  CHECK(device_dict->GetString("alias", &props.device_alias));
  CHECK(device_dict->GetString("address", &props.device_address));
  CHECK(device_dict->GetString("pairingMethod", &props.pairing_method));
  CHECK(device_dict->GetString("pairingAuthToken", &props.pairing_auth_token));
  CHECK(device_dict->GetString("pairingAction", &props.pairing_action));
  CHECK(device_dict->GetInteger("classValue", &props.device_class));
  CHECK(device_dict->GetBoolean("isTrusted", &props.is_trusted));
  CHECK(device_dict->GetBoolean("incoming", &props.incoming));

  // Create the device and store it in the FakeBluetoothDeviceClient's observed
  // list of devices.
  fake_bluetooth_device_client_->CreateDeviceWithProperties(
      dbus::ObjectPath(chromeos::FakeBluetoothAdapterClient::kAdapterPath),
      props);

  return props.device_path;
}

scoped_ptr<base::DictionaryValue> DeviceEmulatorMessageHandler::GetDeviceInfo(
    const dbus::ObjectPath& object_path) {
  // Get the device's properties.
  chromeos::FakeBluetoothDeviceClient::Properties* props =
      fake_bluetooth_device_client_->GetProperties(object_path);
  scoped_ptr<base::DictionaryValue> device(new base::DictionaryValue());
  scoped_ptr<base::ListValue> uuids(new base::ListValue);
  chromeos::FakeBluetoothDeviceClient::SimulatedPairingOptions* options =
      fake_bluetooth_device_client_->GetPairingOptions(object_path);

  device->SetString("path", object_path.value());
  device->SetString("name", props->name.value());
  device->SetString("alias", props->alias.value());
  device->SetString("address", props->address.value());
  if (options) {
    device->SetString("pairingMethod", options->pairing_method);
    device->SetString("pairingAuthToken", options->pairing_auth_token);
    device->SetString("pairingAction", options->pairing_action);
  } else {
    device->SetString("pairingMethod", "");
    device->SetString("pairingAuthToken", "");
    device->SetString("pairingAction", "");
  }
  device->SetInteger("classValue", props->bluetooth_class.value());
  device->SetBoolean("isTrusted", props->trusted.value());
  device->SetBoolean("incoming", false);

  for (const std::string& uuid : props->uuids.value()) {
    uuids->AppendString(uuid);
  }

  device->Set("uuids", uuids.Pass());

  return device.Pass();
}

}  // namespace chromeos
