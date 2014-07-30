// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/hid_detection_screen_handler.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/metrics/histogram.h"
#include "base/prefs/pref_service.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/webui/chromeos/login/oobe_ui.h"
#include "chrome/common/pref_names.h"
#include "chromeos/chromeos_switches.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

const char kJsScreenPath[] = "login.HIDDetectionScreen";

// Variants of pairing state.
const char kRemotePinCode[] = "bluetoothRemotePinCode";
const char kRemotePasskey[] = "bluetoothRemotePasskey";

// Possible ui-states for device-blocks.
const char kSearchingState[] = "searching";
const char kUSBConnectedState[] = "connected";
const char kBTPairedState[] = "paired";
const char kBTPairingState[] = "pairing";
// Special state for notifications that don't switch ui-state, but add info.
const char kBTUpdateState[] = "update";

// Names of possible arguments used for ui update.
const char kPincodeArgName[] = "pincode";
const char kDeviceNameArgName[] = "name";
const char kLabelArgName[] = "keyboard-label";

// Standard length of pincode for pairing BT keyboards.
const int kPincodeLength = 6;

bool DeviceIsPointing(device::BluetoothDevice::DeviceType device_type) {
  return device_type == device::BluetoothDevice::DEVICE_MOUSE ||
         device_type == device::BluetoothDevice::DEVICE_KEYBOARD_MOUSE_COMBO ||
         device_type == device::BluetoothDevice::DEVICE_TABLET;
}

bool DeviceIsPointing(const device::InputServiceLinux::InputDeviceInfo& info) {
  return info.is_mouse || info.is_touchpad || info.is_touchscreen ||
         info.is_tablet;
}

bool DeviceIsKeyboard(device::BluetoothDevice::DeviceType device_type) {
  return device_type == device::BluetoothDevice::DEVICE_KEYBOARD ||
         device_type == device::BluetoothDevice::DEVICE_KEYBOARD_MOUSE_COMBO;
}

}  // namespace

namespace chromeos {

HIDDetectionScreenHandler::HIDDetectionScreenHandler(
    CoreOobeActor* core_oobe_actor)
    : BaseScreenHandler(kJsScreenPath),
      delegate_(NULL),
      core_oobe_actor_(core_oobe_actor),
      show_on_init_(false),
      mouse_is_pairing_(false),
      keyboard_is_pairing_(false),
      switch_on_adapter_when_ready_(false),
      weak_ptr_factory_(this) {
}

HIDDetectionScreenHandler::~HIDDetectionScreenHandler() {
  if (adapter_.get())
    adapter_->RemoveObserver(this);
  input_service_proxy_.RemoveObserver(this);
  if (delegate_)
    delegate_->OnActorDestroyed(this);
}

void HIDDetectionScreenHandler::OnStartDiscoverySession(
    scoped_ptr<device::BluetoothDiscoverySession> discovery_session) {
  VLOG(1) << "BT Discovery session started";
  discovery_session_ = discovery_session.Pass();
  UpdateDevices();
}

void HIDDetectionScreenHandler::SetPoweredError() {
  LOG(ERROR) << "Failed to power BT adapter";
}

void HIDDetectionScreenHandler::FindDevicesError() {
  VLOG(1) << "Failed to start Bluetooth discovery.";
}

void HIDDetectionScreenHandler::Show() {
  if (!page_is_ready()) {
    show_on_init_ = true;
    return;
  }
  if (!CommandLine::ForCurrentProcess()->HasSwitch(switches::kDisableDemoMode))
    core_oobe_actor_->InitDemoModeDetection();
  input_service_proxy_.AddObserver(this);
  UpdateDevices();

  PrefService* local_state = g_browser_process->local_state();
  int num_of_times_dialog_was_shown = local_state->GetInteger(
      prefs::kTimesHIDDialogShown);
  local_state->SetInteger(prefs::kTimesHIDDialogShown,
                          num_of_times_dialog_was_shown + 1);

  ShowScreen(OobeUI::kScreenHIDDetection, NULL);
  if (!pointing_device_id_.empty())
    SendPointingDeviceNotification();
  if (!keyboard_device_id_.empty())
    SendKeyboardDeviceNotification(NULL);
}

void HIDDetectionScreenHandler::Hide() {
  if (adapter_.get())
    adapter_->RemoveObserver(this);
  input_service_proxy_.RemoveObserver(this);
}

void HIDDetectionScreenHandler::SetDelegate(Delegate* delegate) {
  delegate_ = delegate;
  if (page_is_ready())
    Initialize();
}

void HIDDetectionScreenHandler::CheckIsScreenRequired(
    const base::Callback<void(bool)>& on_check_done) {
  input_service_proxy_.GetDevices(
      base::Bind(&HIDDetectionScreenHandler::OnGetInputDevicesListForCheck,
                 weak_ptr_factory_.GetWeakPtr(),
                 on_check_done));
}

void HIDDetectionScreenHandler::DeclareLocalizedValues(
    LocalizedValuesBuilder* builder) {
  builder->Add("hidDetectionContinue", IDS_HID_DETECTION_CONTINUE_BUTTON);
  builder->Add("hidDetectionInvitation", IDS_HID_DETECTION_INVITATION_TEXT);
  builder->Add("hidDetectionPrerequisites",
      IDS_HID_DETECTION_PRECONDITION_TEXT);
  builder->Add("hidDetectionMouseSearching", IDS_HID_DETECTION_SEARCHING_MOUSE);
  builder->Add("hidDetectionKeyboardSearching",
      IDS_HID_DETECTION_SEARCHING_KEYBOARD);
  builder->Add("hidDetectionUSBMouseConnected",
      IDS_HID_DETECTION_CONNECTED_USB_MOUSE);
  builder->Add("hidDetectionUSBKeyboardConnected",
      IDS_HID_DETECTION_CONNECTED_USB_KEYBOARD);
  builder->Add("hidDetectionBTMousePaired",
      IDS_HID_DETECTION_PAIRED_BLUETOOTH_MOUSE);
  builder->Add("hidDetectionBTEnterKey", IDS_HID_DETECTION_BLUETOOTH_ENTER_KEY);
}

void HIDDetectionScreenHandler::Initialize() {
  if (!page_is_ready() || !delegate_)
    return;

  device::BluetoothAdapterFactory::GetAdapter(
      base::Bind(&HIDDetectionScreenHandler::InitializeAdapter,
                 weak_ptr_factory_.GetWeakPtr()));

  if (show_on_init_) {
    Show();
    show_on_init_ = false;
  }
}

void HIDDetectionScreenHandler::RegisterMessages() {
  AddCallback(
      "HIDDetectionOnContinue", &HIDDetectionScreenHandler::HandleOnContinue);
}

void HIDDetectionScreenHandler::HandleOnContinue() {
  // Continue button pressed.
  ContinueScenarioType scenario_type;
  if (!pointing_device_id_.empty() && !keyboard_device_id_.empty())
    scenario_type = All_DEVICES_DETECTED;
  else if (pointing_device_id_.empty())
    scenario_type = KEYBOARD_DEVICE_ONLY_DETECTED;
  else
    scenario_type = POINTING_DEVICE_ONLY_DETECTED;

  UMA_HISTOGRAM_ENUMERATION(
      "HIDDetection.OOBEDevicesDetectedOnContinuePressed",
      scenario_type,
      CONTINUE_SCENARIO_TYPE_SIZE);

  core_oobe_actor_->StopDemoModeDetection();
  if (delegate_)
    delegate_->OnExit();
}

void HIDDetectionScreenHandler::InitializeAdapter(
    scoped_refptr<device::BluetoothAdapter> adapter) {
  adapter_ = adapter;
  CHECK(adapter_.get());

  adapter_->AddObserver(this);
  UpdateDevices();
}

void HIDDetectionScreenHandler::StartBTDiscoverySession() {
  adapter_->StartDiscoverySession(
      base::Bind(&HIDDetectionScreenHandler::OnStartDiscoverySession,
                 weak_ptr_factory_.GetWeakPtr()),
      base::Bind(&HIDDetectionScreenHandler::FindDevicesError,
                 weak_ptr_factory_.GetWeakPtr()));
}

void HIDDetectionScreenHandler::RequestPinCode(
    device::BluetoothDevice* device) {
  VLOG(1) << "RequestPinCode id = " << device->GetDeviceID()
          << " name = " << device->GetName();
  device->CancelPairing();
}

void HIDDetectionScreenHandler::RequestPasskey(
    device::BluetoothDevice* device) {
  VLOG(1) << "RequestPassKey id = " << device->GetDeviceID()
          << " name = " << device->GetName();
  device->CancelPairing();
}

void HIDDetectionScreenHandler::DisplayPinCode(device::BluetoothDevice* device,
                                               const std::string& pincode) {
  VLOG(1) << "DisplayPinCode id = " << device->GetDeviceID()
          << " name = " << device->GetName();
  base::DictionaryValue params;
  params.SetString("state", kBTPairingState);
  params.SetString("pairing-state", kRemotePinCode);
  params.SetString("pincode", pincode);
  params.SetString(kDeviceNameArgName, device->GetName());
  SendKeyboardDeviceNotification(&params);
}

void HIDDetectionScreenHandler::DisplayPasskey(
    device::BluetoothDevice* device, uint32 passkey) {
  VLOG(1) << "DisplayPassKey id = " << device->GetDeviceID()
          << " name = " << device->GetName();
  base::DictionaryValue params;
  params.SetString("state", kBTPairingState);
  params.SetString("pairing-state", kRemotePasskey);
  params.SetInteger("passkey", passkey);
  std::string pincode = base::UintToString(passkey);
  pincode = std::string(kPincodeLength - pincode.length(), '0').append(pincode);
  params.SetString("pincode", pincode);
  params.SetString(kDeviceNameArgName, device->GetName());
  SendKeyboardDeviceNotification(&params);
}

void HIDDetectionScreenHandler::KeysEntered(
    device::BluetoothDevice* device, uint32 entered) {
  VLOG(1) << "Keys entered";
  base::DictionaryValue params;
  params.SetString("state", kBTUpdateState);
  params.SetInteger("keysEntered", entered);
  SendKeyboardDeviceNotification(&params);
}

void HIDDetectionScreenHandler::ConfirmPasskey(
    device::BluetoothDevice* device, uint32 passkey) {
  VLOG(1) << "Confirm Passkey";
  device->CancelPairing();
}

void HIDDetectionScreenHandler::AuthorizePairing(
    device::BluetoothDevice* device) {
  // There is never any circumstance where this will be called, since the
  // HID detection screen  handler will only be used for outgoing pairing
  // requests, but play it safe.
  VLOG(1) << "Authorize pairing";
  device->ConfirmPairing();
}

void HIDDetectionScreenHandler::AdapterPresentChanged(
    device::BluetoothAdapter* adapter, bool present) {
  if (present && switch_on_adapter_when_ready_) {
    adapter_->SetPowered(
        true,
        base::Bind(&HIDDetectionScreenHandler::StartBTDiscoverySession,
                   weak_ptr_factory_.GetWeakPtr()),
        base::Bind(&HIDDetectionScreenHandler::SetPoweredError,
                   weak_ptr_factory_.GetWeakPtr()));
  }
}

void HIDDetectionScreenHandler::TryPairingAsPointingDevice(
    device::BluetoothDevice* device) {
  if (pointing_device_id_.empty() &&
      DeviceIsPointing(device->GetDeviceType()) &&
      device->IsPairable() && !device->IsPaired() && !mouse_is_pairing_) {
    ConnectBTDevice(device);
  }
}

void HIDDetectionScreenHandler::TryPairingAsKeyboardDevice(
    device::BluetoothDevice* device) {
  if (keyboard_device_id_.empty() &&
      DeviceIsKeyboard(device->GetDeviceType()) &&
      device->IsPairable() && !device->IsPaired() && !keyboard_is_pairing_) {
    ConnectBTDevice(device);
  }
}

void HIDDetectionScreenHandler::DeviceAdded(
    device::BluetoothAdapter* adapter, device::BluetoothDevice* device) {
  VLOG(1) << "BT input device added id = " << device->GetDeviceID() <<
      " name = " << device->GetName();
  TryPairingAsPointingDevice(device);
  TryPairingAsKeyboardDevice(device);
}

void HIDDetectionScreenHandler::DeviceChanged(
    device::BluetoothAdapter* adapter, device::BluetoothDevice* device) {
  VLOG(1) << "BT device changed id = " << device->GetDeviceID() << " name = " <<
      device->GetName();
  TryPairingAsPointingDevice(device);
  TryPairingAsKeyboardDevice(device);
}

void HIDDetectionScreenHandler::DeviceRemoved(
    device::BluetoothAdapter* adapter, device::BluetoothDevice* device) {
  VLOG(1) << "BT device removed id = " << device->GetDeviceID() << " name = " <<
      device->GetName();
}

void HIDDetectionScreenHandler::OnInputDeviceAdded(
    const InputDeviceInfo& info) {
  VLOG(1) << "Input device added id = " << info.id << " name = " << info.name;
  // TODO(merkulova): deal with all available device types, e.g. joystick.
  if (!keyboard_device_id_.empty() && !pointing_device_id_.empty())
    return;

  if (pointing_device_id_.empty() && DeviceIsPointing(info)) {
    pointing_device_id_ = info.id;
    pointing_device_name_ = info.name;
    pointing_device_connect_type_ = info.type;
    SendPointingDeviceNotification();
  }
  if (keyboard_device_id_.empty() && info.is_keyboard) {
    keyboard_device_id_ = info.id;
    keyboard_device_name_ = info.name;
    keyboard_device_connect_type_ = info.type;
    SendKeyboardDeviceNotification(NULL);
  }
}

void HIDDetectionScreenHandler::OnInputDeviceRemoved(const std::string& id) {
  if (id == keyboard_device_id_) {
    keyboard_device_id_.clear();
    keyboard_device_name_.clear();
    keyboard_device_connect_type_ = InputDeviceInfo::TYPE_UNKNOWN;
    SendKeyboardDeviceNotification(NULL);
    UpdateDevices();
  } else if (id == pointing_device_id_) {
    pointing_device_id_.clear();
    pointing_device_name_.clear();
    pointing_device_connect_type_ = InputDeviceInfo::TYPE_UNKNOWN;
    SendPointingDeviceNotification();
    UpdateDevices();
  }
}

// static
void HIDDetectionScreenHandler::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(prefs::kTimesHIDDialogShown, 0);
}

void HIDDetectionScreenHandler::UpdateDevices() {
  input_service_proxy_.GetDevices(
      base::Bind(&HIDDetectionScreenHandler::OnGetInputDevicesList,
                 weak_ptr_factory_.GetWeakPtr()));
}

void HIDDetectionScreenHandler::UpdateBTDevices() {
  if (!adapter_ || !adapter_->IsPresent() || !adapter_->IsPowered())
    return;

  // If no connected devices found as pointing device and keyboard, we try to
  // connect some type-suitable active bluetooth device.
  std::vector<device::BluetoothDevice*> bt_devices = adapter_->GetDevices();
  for (std::vector<device::BluetoothDevice*>::const_iterator it =
           bt_devices.begin();
       it != bt_devices.end() &&
           (keyboard_device_id_.empty() || pointing_device_id_.empty());
       ++it) {
    TryPairingAsPointingDevice(*it);
    TryPairingAsKeyboardDevice(*it);
  }
}

void HIDDetectionScreenHandler::ProcessConnectedDevicesList(
    const std::vector<InputDeviceInfo>& devices) {
  for (std::vector<InputDeviceInfo>::const_iterator it = devices.begin();
       it != devices.end() &&
       (pointing_device_id_.empty() || keyboard_device_id_.empty());
       ++it) {
    if (pointing_device_id_.empty() && DeviceIsPointing(*it)) {
      pointing_device_id_ = it->id;
      pointing_device_name_ = it->name;
      pointing_device_connect_type_ = it->type;
      if (page_is_ready())
        SendPointingDeviceNotification();
    }
    if (keyboard_device_id_.empty() && it->is_keyboard) {
      keyboard_device_id_ = it->id;
      keyboard_device_name_ = it->name;
      keyboard_device_connect_type_ = it->type;
      if (page_is_ready())
        SendKeyboardDeviceNotification(NULL);
    }
  }
}

void HIDDetectionScreenHandler::TryInitiateBTDevicesUpdate() {
  if ((pointing_device_id_.empty() || keyboard_device_id_.empty()) &&
      adapter_) {
    if (!adapter_->IsPresent()) {
      // Switch on BT adapter later when it's available.
      switch_on_adapter_when_ready_ = true;
    } else if (!adapter_->IsPowered()) {
      adapter_->SetPowered(
          true,
          base::Bind(&HIDDetectionScreenHandler::StartBTDiscoverySession,
                     weak_ptr_factory_.GetWeakPtr()),
          base::Bind(&HIDDetectionScreenHandler::SetPoweredError,
                     weak_ptr_factory_.GetWeakPtr()));
    } else {
      UpdateBTDevices();
    }
  }
}

void HIDDetectionScreenHandler::OnGetInputDevicesListForCheck(
    const base::Callback<void(bool)>& on_check_done,
    const std::vector<InputDeviceInfo>& devices) {
  ProcessConnectedDevicesList(devices);

  // Screen is not required if both devices are present.
  bool all_devices_autodetected = !pointing_device_id_.empty() &&
                                  !keyboard_device_id_.empty();
  UMA_HISTOGRAM_BOOLEAN("HIDDetection.OOBEDialogShown",
                        !all_devices_autodetected);

  on_check_done.Run(!all_devices_autodetected);
}

void HIDDetectionScreenHandler::OnGetInputDevicesList(
    const std::vector<InputDeviceInfo>& devices) {
  ProcessConnectedDevicesList(devices);
  TryInitiateBTDevicesUpdate();
}

void HIDDetectionScreenHandler::ConnectBTDevice(
    device::BluetoothDevice* device) {
  if (!device->IsPairable() || device->IsPaired())
    return;
  device::BluetoothDevice::DeviceType device_type = device->GetDeviceType();

  if (device_type == device::BluetoothDevice::DEVICE_MOUSE ||
      device_type == device::BluetoothDevice::DEVICE_TABLET) {
    if (mouse_is_pairing_)
      return;
    mouse_is_pairing_ = true;
  } else if (device_type == device::BluetoothDevice::DEVICE_KEYBOARD) {
    if (keyboard_is_pairing_)
      return;
    keyboard_is_pairing_ = true;
  } else if (device_type ==
      device::BluetoothDevice::DEVICE_KEYBOARD_MOUSE_COMBO) {
    if (mouse_is_pairing_ || keyboard_is_pairing_)
      return;
    mouse_is_pairing_ = true;
    keyboard_is_pairing_ = true;
  }
  device->Connect(this,
            base::Bind(&HIDDetectionScreenHandler::BTConnected,
                       weak_ptr_factory_.GetWeakPtr(), device_type),
            base::Bind(&HIDDetectionScreenHandler::BTConnectError,
                       weak_ptr_factory_.GetWeakPtr(),
                       device->GetAddress(), device_type));
}

void HIDDetectionScreenHandler::BTConnected(
    device::BluetoothDevice::DeviceType device_type) {
  if (DeviceIsPointing(device_type))
    mouse_is_pairing_ = false;
  if (DeviceIsKeyboard(device_type))
    keyboard_is_pairing_ = false;
}

void HIDDetectionScreenHandler::BTConnectError(
    const std::string& address,
    device::BluetoothDevice::DeviceType device_type,
    device::BluetoothDevice::ConnectErrorCode error_code) {
  LOG(WARNING) << "BTConnectError while connecting " << address
               << " error code = " << error_code;
  if (DeviceIsPointing(device_type))
    mouse_is_pairing_ = false;
  if (DeviceIsKeyboard(device_type)) {
    keyboard_is_pairing_ = false;
    SendKeyboardDeviceNotification(NULL);
  }

  if (pointing_device_id_.empty() || keyboard_device_id_.empty())
    UpdateDevices();
}


void HIDDetectionScreenHandler::SendPointingDeviceNotification() {
  std::string state;
  if (pointing_device_id_.empty())
    state = kSearchingState;
  else if (pointing_device_connect_type_ == InputDeviceInfo::TYPE_BLUETOOTH)
    state = kBTPairedState;
  else
    state = kUSBConnectedState;
  CallJS("setPointingDeviceState", state);
}

void HIDDetectionScreenHandler::SendKeyboardDeviceNotification(
    base::DictionaryValue* params) {
  base::DictionaryValue state_info;
  if (params)
    state_info.MergeDictionary(params);

  base::string16 device_name;
  if (!state_info.GetString(kDeviceNameArgName, &device_name)) {
    device_name = l10n_util::GetStringUTF16(
        IDS_HID_DETECTION_DEFAULT_KEYBOARD_NAME);
  }

  if (keyboard_device_id_.empty()) {
    if (!state_info.HasKey("state")) {
      state_info.SetString("state", kSearchingState);
    } else if (state_info.HasKey(kPincodeArgName)) {
      state_info.SetString(
          kLabelArgName,
          l10n_util::GetStringFUTF16(
              IDS_HID_DETECTION_BLUETOOTH_REMOTE_PIN_CODE_REQUEST,
              device_name));
    }
  } else if (keyboard_device_connect_type_ == InputDeviceInfo::TYPE_BLUETOOTH) {
    state_info.SetString("state", kBTPairedState);
    state_info.SetString(
        kLabelArgName,
        l10n_util::GetStringFUTF16(
            IDS_HID_DETECTION_PAIRED_BLUETOOTH_KEYBOARD,
            base::UTF8ToUTF16(keyboard_device_name_)));
  } else {
    state_info.SetString("state", kUSBConnectedState);
  }
  CallJS("setKeyboardDeviceState", state_info);
}

}  // namespace chromeos
