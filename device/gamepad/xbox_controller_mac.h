// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_GAMEPAD_XBOX_CONTROLLER_MAC_H_
#define DEVICE_GAMEPAD_XBOX_CONTROLLER_MAC_H_

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <stddef.h>
#include <stdint.h>

#include <memory>

#include "base/mac/scoped_cftyperef.h"
#include "base/mac/scoped_ioplugininterface.h"
#include "base/macros.h"
#include "base/sequenced_task_runner.h"
#include "device/gamepad/public/interfaces/gamepad.mojom.h"

struct IOUSBDeviceStruct320;
struct IOUSBInterfaceStruct300;

namespace device {

class XboxControllerMac {
 public:
  static const int kVendorMicrosoft = 0x045e;
  static const int kProductXbox360Controller = 0x028e;
  static const int kProductXboxOneController2013 = 0x02d1;
  static const int kProductXboxOneController2015 = 0x02dd;
  static const int kProductXboxOneEliteController = 0x02e3;
  static const int kProductXboxOneSController = 0x02ea;

  enum ControllerType {
    UNKNOWN_CONTROLLER,
    XBOX_360_CONTROLLER,
    XBOX_ONE_CONTROLLER_2013,
    XBOX_ONE_CONTROLLER_2015,
    XBOX_ONE_ELITE_CONTROLLER,
    XBOX_ONE_S_CONTROLLER
  };

  enum LEDPattern {
    LED_OFF = 0,

    // 2 quick flashes, then a series of slow flashes (about 1 per second).
    LED_FLASH = 1,

    // Flash three times then hold the LED on. This is the standard way to tell
    // the player which player number they are.
    LED_FLASH_TOP_LEFT = 2,
    LED_FLASH_TOP_RIGHT = 3,
    LED_FLASH_BOTTOM_LEFT = 4,
    LED_FLASH_BOTTOM_RIGHT = 5,

    // Simply turn on the specified LED and turn all other LEDs off.
    LED_HOLD_TOP_LEFT = 6,
    LED_HOLD_TOP_RIGHT = 7,
    LED_HOLD_BOTTOM_LEFT = 8,
    LED_HOLD_BOTTOM_RIGHT = 9,

    LED_ROTATE = 10,

    LED_FLASH_FAST = 11,
    LED_FLASH_SLOW = 12,  // Flash about once per 3 seconds

    // Flash alternating LEDs for a few seconds, then flash all LEDs about once
    // per second
    LED_ALTERNATE_PATTERN = 13,

    // 14 is just another boring flashing speed.

    // Flash all LEDs once then go black.
    LED_FLASH_ONCE = 15,

    LED_NUM_PATTERNS
  };

  struct Data {
    bool buttons[15];
    float triggers[2];
    float axes[4];
  };

  class Delegate {
   public:
    virtual void XboxControllerGotData(XboxControllerMac* controller,
                                       const Data& data) = 0;
    virtual void XboxControllerGotGuideData(XboxControllerMac* controller,
                                            bool guide) = 0;
    virtual void XboxControllerError(XboxControllerMac* controller) = 0;
  };

  explicit XboxControllerMac(Delegate* delegate);
  virtual ~XboxControllerMac();

  bool OpenDevice(io_service_t service);

  void SetLEDPattern(LEDPattern pattern);

  void PlayEffect(
      mojom::GamepadHapticEffectType type,
      mojom::GamepadEffectParametersPtr params,
      mojom::GamepadHapticsManager::PlayVibrationEffectOnceCallback callback);

  void ResetVibration(
      mojom::GamepadHapticsManager::ResetVibrationActuatorCallback callback);

  UInt32 location_id() { return location_id_; }
  int GetVendorId() const;
  int GetProductId() const;
  ControllerType GetControllerType() const;
  std::string GetControllerTypeString() const;
  std::string GetIdString() const;

 private:
  static void WriteComplete(void* context, IOReturn result, void* arg0);
  static void GotData(void* context, IOReturn result, void* arg0);
  static void DoRunCallback(
      mojom::GamepadHapticsManager::PlayVibrationEffectOnceCallback callback,
      mojom::GamepadHapticsResult result);

  void ProcessXbox360Packet(size_t length);
  void ProcessXboxOnePacket(size_t length);
  void QueueRead();

  void IOError();

  void PlayDualRumbleEffect(int sequence_id,
                            double duration,
                            double start_delay,
                            double strong_magnitude,
                            double weak_magnitude);
  void StartVibration(int sequence_id,
                      double duration,
                      double strong_magnitude,
                      double weak_magnitude);
  void StopVibration(int sequence_id);
  void SetVibration(double strong_magnitude, double weak_magnitude);

  void RunCallbackOnMojoThread(mojom::GamepadHapticsResult result);

  void WriteXbox360Rumble(uint8_t strong_magnitude, uint8_t weak_magnitude);
  void WriteXboxOneInit();
  void WriteXboxOneRumble(uint8_t strong_magnitude, uint8_t weak_magnitude);

  // Handle for the USB device. IOUSBDeviceStruct320 is the latest version of
  // the device API that is supported on Mac OS 10.6.
  base::mac::ScopedIOPluginInterface<IOUSBDeviceStruct320> device_;

  // Handle for the interface on the device which sends button and analog data.
  // The other interfaces (for the ChatPad and headset) are ignored.
  base::mac::ScopedIOPluginInterface<IOUSBInterfaceStruct300> interface_;

  bool device_is_open_;
  bool interface_is_open_;

  base::ScopedCFTypeRef<CFRunLoopSourceRef> source_;

  // This will be set to the max packet size reported by the interface, which
  // is 32 bytes. I would have expected USB to do message framing itself, but
  // somehow we still sometimes (rarely!) get packets off the interface which
  // aren't correctly framed. The 360 controller frames its packets with a 2
  // byte header (type, total length) so we can reframe the packet data
  // ourselves.
  uint16_t read_buffer_size_;
  std::unique_ptr<uint8_t[]> read_buffer_;

  // The pattern that the LEDs on the device are currently displaying, or
  // LED_NUM_PATTERNS if unknown.
  LEDPattern led_pattern_;

  UInt32 location_id_;

  Delegate* delegate_;

  ControllerType controller_type_;
  int read_endpoint_;
  int control_endpoint_;

  uint8_t counter_;

  int sequence_id_;
  scoped_refptr<base::SequencedTaskRunner> playing_effect_task_runner_;
  mojom::GamepadHapticsManager::PlayVibrationEffectOnceCallback
      playing_effect_callback_;

  DISALLOW_COPY_AND_ASSIGN(XboxControllerMac);
};

}  // namespace device

#endif  // DEVICE_GAMEPAD_XBOX_CONTROLLER_MAC_H_
