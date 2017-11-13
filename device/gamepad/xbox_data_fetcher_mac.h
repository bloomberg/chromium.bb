// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_GAMEPAD_XBOX_DATA_FETCHER_MAC_H_
#define DEVICE_GAMEPAD_XBOX_DATA_FETCHER_MAC_H_

#include <stdint.h>

#include <set>

#include "base/mac/scoped_ionotificationportref.h"
#include "base/mac/scoped_ioobject.h"
#include "base/macros.h"
#include "device/gamepad/gamepad_data_fetcher.h"
#include "device/gamepad/xbox_controller_mac.h"

namespace device {

class XboxDataFetcher : public GamepadDataFetcher,
                        public XboxControllerMac::Delegate {
 public:
  typedef GamepadDataFetcherFactoryImpl<XboxDataFetcher,
                                        GAMEPAD_SOURCE_MAC_XBOX>
      Factory;

  XboxDataFetcher();
  ~XboxDataFetcher() override;

  GamepadSource source() override;

  // GamepadDataFetcher overrides
  void GetGamepadData(bool devices_changed_hint) override;
  void PlayEffect(
      int source_id,
      mojom::GamepadHapticEffectType,
      mojom::GamepadEffectParametersPtr,
      mojom::GamepadHapticsManager::PlayVibrationEffectOnceCallback) override;
  void ResetVibration(
      int source_id,
      mojom::GamepadHapticsManager::ResetVibrationActuatorCallback) override;

  bool RegisterForNotifications();
  bool RegisterForDeviceNotifications(
      int vendor_id,
      int product_id,
      base::mac::ScopedIOObject<io_iterator_t>* added_iter,
      base::mac::ScopedIOObject<io_iterator_t>* removed_iter);
  void UnregisterFromNotifications();

  XboxControllerMac* ControllerForLocation(UInt32 location_id);

 private:
  static void DeviceAdded(void* context, io_iterator_t iterator);
  static void DeviceRemoved(void* context, io_iterator_t iterator);
  void OnAddedToProvider() override;
  void AddController(XboxControllerMac* controller);
  void RemoveController(XboxControllerMac* controller);
  void RemoveControllerByLocationID(uint32_t id);
  void XboxControllerGotData(XboxControllerMac* controller,
                             const XboxControllerMac::Data& data) override;
  void XboxControllerGotGuideData(XboxControllerMac* controller,
                                  bool guide) override;
  void XboxControllerError(XboxControllerMac* controller) override;

  std::set<XboxControllerMac*> controllers_;

  bool listening_;

  // port_ owns source_, so this doesn't need to be a ScopedCFTypeRef, but we
  // do need to maintain a reference to it so we can invalidate it.
  CFRunLoopSourceRef source_;
  base::mac::ScopedIONotificationPortRef port_;
  base::mac::ScopedIOObject<io_iterator_t> xbox_360_device_added_iter_;
  base::mac::ScopedIOObject<io_iterator_t> xbox_360_device_removed_iter_;
  base::mac::ScopedIOObject<io_iterator_t> xbox_one_2013_device_added_iter_;
  base::mac::ScopedIOObject<io_iterator_t> xbox_one_2013_device_removed_iter_;
  base::mac::ScopedIOObject<io_iterator_t> xbox_one_2015_device_added_iter_;
  base::mac::ScopedIOObject<io_iterator_t> xbox_one_2015_device_removed_iter_;
  base::mac::ScopedIOObject<io_iterator_t> xbox_one_elite_device_added_iter_;
  base::mac::ScopedIOObject<io_iterator_t> xbox_one_elite_device_removed_iter_;
  base::mac::ScopedIOObject<io_iterator_t> xbox_one_s_device_added_iter_;
  base::mac::ScopedIOObject<io_iterator_t> xbox_one_s_device_removed_iter_;

  DISALLOW_COPY_AND_ASSIGN(XboxDataFetcher);
};

}  // namespace device

#endif  // DEVICE_GAMEPAD_XBOX_DATA_FETCHER_MAC_H_
