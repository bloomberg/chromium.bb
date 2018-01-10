// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_GAMEPAD_GAMEPAD_DEVICE_LINUX_
#define DEVICE_GAMEPAD_GAMEPAD_DEVICE_LINUX_

#include "device/gamepad/abstract_haptic_gamepad.h"
#include "device/gamepad/dualshock4_controller_linux.h"
#include "device/gamepad/udev_gamepad_linux.h"

extern "C" {
struct udev_device;
}

namespace device {

// GamepadDeviceLinux represents a single gamepad device which may be accessed
// through multiple host interfaces. Gamepad button and axis state are queried
// through the joydev interface, while haptics commands are routed through the
// evdev interface. A gamepad must be enumerated through joydev to be usable,
// but the evdev interface is only required for haptic effects.
//
// Dualshock4 haptics are not supported through evdev and are instead sent
// through the raw HID (hidraw) interface.
class GamepadDeviceLinux : public AbstractHapticGamepad {
 public:
  GamepadDeviceLinux(const std::string& syspath_prefix);
  ~GamepadDeviceLinux() override;

  // Delete any stored effect and close file descriptors.
  void DoShutdown() override;

  // Returns true if no device nodes are associated with this device.
  bool IsEmpty() const;

  int GetJoydevIndex() const { return joydev_index_; }
  std::string GetVendorId() const { return vendor_id_; }
  std::string GetProductId() const { return product_id_; }
  std::string GetVersionNumber() const { return version_number_; }
  std::string GetName() const { return name_; }
  std::string GetSyspathPrefix() const { return syspath_prefix_; }

  bool SupportsVibration() const;

  // Reads the current gamepad state into |pad|.
  void ReadPadState(Gamepad* pad) const;

  // Returns true if |pad_info| describes this device.
  bool IsSameDevice(const UdevGamepadLinux& pad_info);

  // Opens the joydev device node and queries device info.
  bool OpenJoydevNode(const UdevGamepadLinux& pad_info, udev_device* device);

  // Closes the joydev device node and clears device info.
  void CloseJoydevNode();

  // Opens the evdev device node and initializes haptics.
  bool OpenEvdevNode(const UdevGamepadLinux& pad_info);

  // Closes the evdev device node and shuts down haptics.
  void CloseEvdevNode();

  // Opens the hidraw device node and initializes haptics.
  bool OpenHidrawNode(const UdevGamepadLinux& pad_info);

  // Closes the hidraw device node and shuts down haptics.
  void CloseHidrawNode();

  // AbstractHapticGamepad
  void SetVibration(double strong_magnitude, double weak_magnitude) override;
  void SetZeroVibration() override;

 private:
  // The syspath prefix is used to identify device nodes that refer to the same
  // underlying gamepad through different interfaces.
  //
  // Joydev and evdev nodes that refer to the same device will share a parent
  // node that represents the physical device. We can compare the syspaths of
  // the parent nodes to determine when two nodes refer to the same device.
  //
  // The syspath for a hidraw node will match the parent syspath of a joydev or
  // evdev node up to the subsystem. To simplify this comparison, we only store
  // the syspath prefix up to the subsystem.
  std::string syspath_prefix_;

  // The file descriptor for the device's joydev node, or -1 if no joydev node
  // is associated with this device.
  int joydev_fd_;

  // The index of the device's joydev node, or -1 if unknown.
  // The joydev index is the integer at the end of the joydev node path and is
  // used to assign the gamepad to a slot. For example, a device with path
  // /dev/input/js2 has index 2 and will be assigned to the 3rd gamepad slot.
  int joydev_index_;

  // The vendor ID of the device.
  std::string vendor_id_;

  // The product ID of the device.
  std::string product_id_;

  // The version number of the device.
  std::string version_number_;

  // A string identifying the manufacturer and model of the device.
  std::string name_;

  // The file descriptor for the device's evdev node, or -1 if no evdev node is
  // associated with this device.
  int evdev_fd_;

  // The ID of the haptic effect stored on the device, or -1 if none is stored.
  int effect_id_;

  // True if the device supports rumble effects through the evdev device node.
  bool supports_force_feedback_;

  // The file descriptor for the device's hidraw node, or -1 if no hidraw node
  // is associated with this device.
  int hidraw_fd_;

  // True if the vendor and product IDs match any model of Dualshock4.
  bool is_dualshock4_;

  // Dualshock4 functionality, if available.
  std::unique_ptr<Dualshock4ControllerLinux> dualshock4_;
};

}  // namespace device

#endif  // DEVICE_GAMEPAD_GAMEPAD_DEVICE_LINUX_
