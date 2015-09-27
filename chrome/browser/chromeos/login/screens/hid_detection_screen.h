// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_HID_DETECTION_SCREEN_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_HID_DETECTION_SCREEN_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/device/input_service_proxy.h"
#include "chrome/browser/chromeos/login/screens/base_screen.h"
#include "chrome/browser/chromeos/login/screens/hid_detection_model.h"
#include "components/login/screens/screen_context.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/bluetooth_discovery_session.h"
#include "device/hid/input_service_linux.h"

namespace chromeos {

class HIDDetectionView;

// Representation independent class that controls screen showing warning about
// HID absence to users.
class HIDDetectionScreen : public HIDDetectionModel,
                           public device::BluetoothAdapter::Observer,
                           public device::BluetoothDevice::PairingDelegate,
                           public InputServiceProxy::Observer {
 public:
  typedef device::InputServiceLinux::InputDeviceInfo InputDeviceInfo;

  class Delegate {
   public:
    virtual ~Delegate() {}
    virtual void OnHIDScreenNecessityCheck(bool screen_needed) = 0;
  };

  HIDDetectionScreen(BaseScreenDelegate* base_screen_delegate,
                     HIDDetectionView* view);
  ~HIDDetectionScreen() override;

  // HIDDetectionModel implementation:
  void PrepareToShow() override;
  void Show() override;
  void Hide() override;
  void Initialize(::login::ScreenContext* context) override;
  void OnContinueButtonClicked() override;
  void OnViewDestroyed(HIDDetectionView* view) override;
  void CheckIsScreenRequired(
      const base::Callback<void(bool)>& on_check_done) override;

  // device::BluetoothDevice::PairingDelegate implementation:
  void RequestPinCode(device::BluetoothDevice* device) override;
  void RequestPasskey(device::BluetoothDevice* device) override;
  void DisplayPinCode(device::BluetoothDevice* device,
                      const std::string& pincode) override;
  void DisplayPasskey(
      device::BluetoothDevice* device, uint32 passkey) override;
  void KeysEntered(device::BluetoothDevice* device, uint32 entered) override;
  void ConfirmPasskey(
      device::BluetoothDevice* device, uint32 passkey) override;
  void AuthorizePairing(device::BluetoothDevice* device) override;

  // device::BluetoothAdapter::Observer implementation.
  void AdapterPresentChanged(device::BluetoothAdapter* adapter,
                             bool present) override;
  void DeviceAdded(device::BluetoothAdapter* adapter,
                   device::BluetoothDevice* device) override;
  void DeviceChanged(device::BluetoothAdapter* adapter,
                     device::BluetoothDevice* device) override;
  void DeviceRemoved(device::BluetoothAdapter* adapter,
                     device::BluetoothDevice* device) override;

  // InputServiceProxy::Observer implementation.
  void OnInputDeviceAdded(const InputDeviceInfo& info) override;
  void OnInputDeviceRemoved(const std::string& id) override;

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

  void StartBTDiscoverySession();

  // Updates internal state and UI (if ready) using list of connected devices.
  void ProcessConnectedDevicesList(
      const std::vector<InputDeviceInfo>& devices);

  // Checks for lack of mouse or keyboard. If found starts BT devices update.
  // Initiates BTAdapter if it's not active and BT devices update required.
  void TryInitiateBTDevicesUpdate();

  // Processes list of input devices returned by InputServiceProxy on the check
  // request. Calls the callback that expects true if screen is required.
  void OnGetInputDevicesListForCheck(
      const base::Callback<void(bool)>& on_check_done,
      const std::vector<InputDeviceInfo>& devices);

  // Processes list of input devices returned by InputServiceProxy on regular
  // request.
  void OnGetInputDevicesList(const std::vector<InputDeviceInfo>& devices);

  // Called for revision of active devices. If current-placement is available
  // for mouse or keyboard device, sets one of active devices as current or
  // tries to connect some BT device if no appropriate devices are connected.
  void UpdateDevices();

  // Tries to connect some BT devices if no type-appropriate devices are
  // connected.
  void UpdateBTDevices();

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

  // Called by device::BluetoothAdapter in response to a failure to
  // power off BT adapter.
  void SetPoweredOffError();

  // Tries to connect given BT device as pointing one.
  void TryPairingAsPointingDevice(device::BluetoothDevice* device);

  // Tries to connect given BT device as keyboard.
  void TryPairingAsKeyboardDevice(device::BluetoothDevice* device);

  // Tries to connect given BT device.
  void ConnectBTDevice(device::BluetoothDevice* device);

  // Called by device::BluetoothDevice on a successful pairing and connection
  // to a device.
  void BTConnected(device::BluetoothDevice::DeviceType device_type);

  // Called by device::BluetoothDevice in response to a failure to
  // connect to the device with bluetooth address |address| due to an error
  // encoded in |error_code|.
  void BTConnectError(const std::string& address,
                      device::BluetoothDevice::DeviceType device_type,
                      device::BluetoothDevice::ConnectErrorCode error_code);

  // Sends a notification to the Web UI of the status of available Bluetooth/USB
  // pointing device.
  void SendPointingDeviceNotification();

  // Sends a notification to the Web UI of the status of available Bluetooth/USB
  // keyboard device.
  void SendKeyboardDeviceNotification();

  // Helper method. Sets device name or placeholder if the name is empty.
  void SetKeyboardDeviceName_(const std::string& name);

  HIDDetectionView* view_;

  // Default bluetooth adapter, used for all operations.
  scoped_refptr<device::BluetoothAdapter> adapter_;

  InputServiceProxy input_service_proxy_;

  // The current device discovery session. Only one active discovery session is
  // kept at a time and the instance that |discovery_session_| points to gets
  // replaced by a new one when a new discovery session is initiated.
  scoped_ptr<device::BluetoothDiscoverySession> discovery_session_;

  // Current pointing device, if any. Device name is kept in screen context.
  std::string pointing_device_id_;
  bool mouse_is_pairing_;
  InputDeviceInfo::Type pointing_device_connect_type_;

  // Current keyboard device, if any. Device name is kept in screen context.
  std::string keyboard_device_id_;
  bool keyboard_is_pairing_;
  InputDeviceInfo::Type keyboard_device_connect_type_;
  std::string keyboard_device_name_;

  // State of BT adapter before screen-initiated changes.
  scoped_ptr<bool> adapter_initially_powered_;

  bool switch_on_adapter_when_ready_;

  bool showing_;

  base::WeakPtrFactory<HIDDetectionScreen> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(HIDDetectionScreen);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_HID_DETECTION_SCREEN_H_
