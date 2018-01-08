// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_GAMEPAD_GAMEPAD_DEVICE_LINUX_
#define DEVICE_GAMEPAD_GAMEPAD_DEVICE_LINUX_

#include "device/gamepad/abstract_haptic_gamepad.h"

extern "C" {
struct udev_device;
}

namespace device {

// GamepadDeviceLinux represents a single gamepad device which may be accessed
// through multiple host interfaces. Gamepad button and axis state are queried
// through the joydev interface, while haptics commands are routed through the
// evdev interface. A gamepad must be enumerated through joydev to be usable,
// but the evdev interface is only required for haptic effects.
class GamepadDeviceLinux : public AbstractHapticGamepad {
 public:
  GamepadDeviceLinux(const std::string& parent_syspath);
  ~GamepadDeviceLinux() override;

  // Delete any stored effect and close file descriptors.
  void DoShutdown() override;

  // Returns true if no device nodes are associated with this device.
  bool IsEmpty() const { return joydev_fd_ < 0 && evdev_fd_ < 0; }

  std::string GetParentSyspath() const { return parent_syspath_; }
  int GetJoydevIndex() const { return joydev_index_; }
  std::string GetVendorId() const { return vendor_id_; }
  std::string GetProductId() const { return product_id_; }
  std::string GetVersionNumber() const { return version_number_; }
  std::string GetName() const { return name_; }
  bool SupportsVibration() const { return supports_vibration_; }

  // Reads the current gamepad state into |pad|.
  void ReadPadState(Gamepad* pad) const;

  // Opens the joydev device node and queries device info.
  bool OpenJoydevNode(const std::string& path,
                      udev_device* device,
                      int joydev_index);

  // Closes the joydev device node and clears device info.
  void CloseJoydevNode();

  // Opens the evdev device node and initializes haptics.
  bool OpenEvdevNode(const std::string& path);

  // Closes the evdev device node and shuts down haptics.
  void CloseEvdevNode();

  // AbstractHapticGamepad
  void SetVibration(double strong_magnitude, double weak_magnitude) override;
  void SetZeroVibration() override;

 private:
  // Joydev and evdev nodes that refer to the same device will share a parent
  // node that represents the physical device. We compare the syspaths of the
  // parent nodes to determine when two nodes refer to the same device through
  // different interfaces.
  std::string parent_syspath_;

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

  // True if the device supports haptic vibration effects.
  bool supports_vibration_;
};

}  // namespace device

#endif  // DEVICE_GAMEPAD_GAMEPAD_DEVICE_LINUX_
