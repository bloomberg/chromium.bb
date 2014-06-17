// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_HID_DETECTION_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_HID_DETECTION_SCREEN_HANDLER_H_

#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/prefs/pref_registry_simple.h"
#include "base/values.h"
#include "chrome/browser/chromeos/device/input_service_proxy.h"
#include "chrome/browser/chromeos/login/screens/hid_detection_screen_actor.h"
#include "chrome/browser/ui/webui/chromeos/login/base_screen_handler.h"
#include "content/public/browser/web_ui.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/bluetooth_discovery_session.h"

namespace base {
class DictionaryValue;
}

namespace chromeos {

class CoreOobeActor;

// WebUI implementation of HIDDetectionScreenActor.
class HIDDetectionScreenHandler
    : public HIDDetectionScreenActor,
      public BaseScreenHandler,
      public device::BluetoothAdapter::Observer,
      public device::BluetoothDevice::PairingDelegate,
      public InputServiceProxy::Observer {
 public:
  typedef device::InputServiceLinux::InputDeviceInfo InputDeviceInfo;

  explicit HIDDetectionScreenHandler(CoreOobeActor* core_oobe_actor);
  virtual ~HIDDetectionScreenHandler();

  // HIDDetectionScreenActor implementation:
  virtual void Show() OVERRIDE;
  virtual void Hide() OVERRIDE;
  virtual void SetDelegate(Delegate* delegate) OVERRIDE;

  // BaseScreenHandler implementation:
  virtual void DeclareLocalizedValues(LocalizedValuesBuilder* builder) OVERRIDE;
  virtual void Initialize() OVERRIDE;

  // WebUIMessageHandler implementation:
  virtual void RegisterMessages() OVERRIDE;

  // device::BluetoothDevice::PairingDelegate implementation:
  virtual void RequestPinCode(device::BluetoothDevice* device) OVERRIDE;
  virtual void RequestPasskey(device::BluetoothDevice* device) OVERRIDE;
  virtual void DisplayPinCode(device::BluetoothDevice* device,
                              const std::string& pincode) OVERRIDE;
  virtual void DisplayPasskey(
      device::BluetoothDevice* device, uint32 passkey) OVERRIDE;
  virtual void KeysEntered(device::BluetoothDevice* device,
                           uint32 entered) OVERRIDE;
  virtual void ConfirmPasskey(
      device::BluetoothDevice* device, uint32 passkey) OVERRIDE;
  virtual void AuthorizePairing(device::BluetoothDevice* device) OVERRIDE;

  // device::BluetoothAdapter::Observer implementation.
  virtual void AdapterPresentChanged(device::BluetoothAdapter* adapter,
                                     bool present) OVERRIDE;
  virtual void DeviceAdded(device::BluetoothAdapter* adapter,
                           device::BluetoothDevice* device) OVERRIDE;
  virtual void DeviceChanged(device::BluetoothAdapter* adapter,
                             device::BluetoothDevice* device) OVERRIDE;
  virtual void DeviceRemoved(device::BluetoothAdapter* adapter,
                             device::BluetoothDevice* device) OVERRIDE;

  // InputServiceProxy::Observer implementation.
  virtual void OnInputDeviceAdded(const InputDeviceInfo& info) OVERRIDE;
  virtual void OnInputDeviceRemoved(const std::string& id) OVERRIDE;

  // Registers the preference for derelict state.
  static void RegisterPrefs(PrefRegistrySimple* registry);

 private:
  // Types of dialog leaving scenarios for UMA metric.
  enum ContinueScenarioType {
    // Only pointing device detected, user pressed 'Continue'.
    POINTING_DEVICE_ONLY_DETECTED,

    // Only keyboard detected, user pressed 'Continue'.
    KEYBOARD_DEVICE_ONLY_DETECTED,

    // All devices detected.
    All_DEVICES_DETECTED,

    // Must be last enum element.
    CONTINUE_SCENARIO_TYPE_SIZE
  };

  void InitializeAdapter(scoped_refptr<device::BluetoothAdapter> adapter);

  // Sends a notification to the Web UI of the status of available Bluetooth/USB
  // pointing device.
  void SendPointingDeviceNotification();

  // Sends a notification to the Web UI of the status of available Bluetooth/USB
  // keyboard device.
  void SendKeyboardDeviceNotification(base::DictionaryValue* params);

  // Updates internal state and UI using list of connected devices.
  void ProcessConnectedDevicesList(const std::vector<InputDeviceInfo>& devices);

  // Checks for lack of mouse or keyboard. If found starts BT devices update.
  // Initiates BTAdapter if it's not active and BT devices update required.
  void TryInitiateBTDevicesUpdate();

  // Processes list of input devices returned by InputServiceProxy on the first
  // time the screen is initiated. Skips the screen if all required devices are
  // present.
  void OnGetInputDevicesListFirstTime(
      const std::vector<InputDeviceInfo>& devices);

  // Processes list of input devices returned by InputServiceProxy on regular
  // request.
  void OnGetInputDevicesList(const std::vector<InputDeviceInfo>& devices);

  void StartBTDiscoverySession();

  // Called by device::BluetoothDevice on a successful pairing and connection
  // to a device.
  void BTConnected(device::BluetoothDevice::DeviceType device_type);

  // Called by device::BluetoothDevice in response to a failure to
  // connect to the device with bluetooth address |address| due to an error
  // encoded in |error_code|.
  void BTConnectError(const std::string& address,
                      device::BluetoothDevice::DeviceType device_type,
                      device::BluetoothDevice::ConnectErrorCode error_code);

  // JS messages handlers.
  void HandleOnContinue();

  Delegate* delegate_;

  CoreOobeActor* core_oobe_actor_;

  // Keeps whether screen should be shown right after initialization.
  bool show_on_init_;

  // Displays in the UI a connecting to the device |device| message.
  void DeviceConnecting(device::BluetoothDevice* device);

  // Called by device::BluetoothAdapter in response to a successful request
  // to initiate a discovery session.
  void OnStartDiscoverySession(
      scoped_ptr<device::BluetoothDiscoverySession> discovery_session);

  // Called by device::BluetoothAdapter in response to a failure to
  // initiate a discovery session.
  void FindDevicesError();

  // Called by device::BluetoothAdapter in response to a failure to
  // power BT adapter.
  void SetPoweredError();

  // Special case uf UpdateDevice. Called on first show, skips the dialog if
  // all necessary devices (mouse and keyboard) already connected.
  void GetDevicesFirstTime();

  // Called for revision of active devices. If current-placement is available
  // for mouse or keyboard device, sets one of active devices as current or
  // tries to connect some BT device if no appropriate devices are connected.
  void UpdateDevices();

  // Tries to connect some BT devices if no type-appropriate devices are
  // connected.
  void UpdateBTDevices();

  // Tries to connect given BT device.
  void ConnectBTDevice(device::BluetoothDevice* device);

  // Tries to connect given BT device as pointing one.
  void TryPairingAsPointingDevice(device::BluetoothDevice* device);

  // Tries to connect given BT device as keyboard.
  void TryPairingAsKeyboardDevice(device::BluetoothDevice* device);

  // Default bluetooth adapter, used for all operations.
  scoped_refptr<device::BluetoothAdapter> adapter_;

  InputServiceProxy input_service_proxy_;

  // The current device discovery session. Only one active discovery session is
  // kept at a time and the instance that |discovery_session_| points to gets
  // replaced by a new one when a new discovery session is initiated.
  scoped_ptr<device::BluetoothDiscoverySession> discovery_session_;

  // Current pointing device, if any.
  std::string pointing_device_name_;
  std::string pointing_device_id_;
  bool mouse_is_pairing_;
  InputDeviceInfo::Type pointing_device_connect_type_;

  // Current keyboard device, if any.
  std::string keyboard_device_name_;
  std::string keyboard_device_id_;
  bool keyboard_is_pairing_;
  InputDeviceInfo::Type keyboard_device_connect_type_;

  bool switch_on_adapter_when_ready_;

  bool first_time_screen_show_;

  base::WeakPtrFactory<HIDDetectionScreenHandler> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(HIDDetectionScreenHandler);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_HID_DETECTION_SCREEN_HANDLER_H_

