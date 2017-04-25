// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_U2F_U2F_REQUEST_H_
#define DEVICE_U2F_U2F_REQUEST_H_

#include "base/cancelable_callback.h"
#include "base/scoped_observer.h"
#include "device/hid/hid_device_filter.h"
#include "device/hid/hid_service.h"
#include "u2f_device.h"

namespace device {
class U2fRequest : HidService::Observer {
 public:
  using ResponseCallback = base::Callback<void(U2fReturnCode status_code,
                                               std::vector<uint8_t> response)>;

  U2fRequest(const ResponseCallback& callback);
  virtual ~U2fRequest();

  void Start();
  void AddDeviceForTesting(std::unique_ptr<U2fDevice> device);

 protected:
  enum class State {
    INIT,
    BUSY,
    WINK,
    IDLE,
    OFF,
    COMPLETE,
  };

  void Transition();
  virtual void TryDevice() = 0;

  std::unique_ptr<U2fDevice> current_device_;
  State state_;
  ResponseCallback cb_;

 private:
  FRIEND_TEST_ALL_PREFIXES(U2fRequestTest, TestAddRemoveDevice);
  FRIEND_TEST_ALL_PREFIXES(U2fRequestTest, TestIterateDevice);
  FRIEND_TEST_ALL_PREFIXES(U2fRequestTest, TestBasicMachine);

  void Enumerate();
  void IterateDevice();
  void OnWaitComplete();
  void AddDevice(std::unique_ptr<U2fDevice> device);
  void OnDeviceAdded(scoped_refptr<HidDeviceInfo> device_info) override;
  void OnDeviceRemoved(scoped_refptr<HidDeviceInfo> device_info) override;
  void OnEnumerate(const std::vector<scoped_refptr<HidDeviceInfo>>& devices);

  std::list<std::unique_ptr<U2fDevice>> devices_;
  std::list<std::unique_ptr<U2fDevice>> attempted_devices_;
  base::CancelableClosure delay_callback_;
  HidDeviceFilter filter_;
  ScopedObserver<HidService, HidService::Observer> hid_service_observer_;
  base::WeakPtrFactory<U2fRequest> weak_factory_;
};
}  // namespace device

#endif  // DEVICE_U2F_U2F_REQUEST_H_
