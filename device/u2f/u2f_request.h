// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_U2F_U2F_REQUEST_H_
#define DEVICE_U2F_U2F_REQUEST_H_

#include <list>
#include <memory>
#include <vector>

#include "base/cancelable_callback.h"
#include "device/u2f/u2f_device.h"
#include "device/u2f/u2f_discovery.h"

namespace device {

class U2fRequest : public U2fDiscovery::Delegate {
 public:
  using ResponseCallback =
      base::Callback<void(U2fReturnCode status_code,
                          const std::vector<uint8_t>& response)>;

  U2fRequest(std::vector<std::unique_ptr<U2fDiscovery>> discoveries,
             ResponseCallback callback);
  virtual ~U2fRequest();

  void Start();

  std::vector<std::unique_ptr<U2fDiscovery>>& discoveries() {
    return discoveries_;
  }

  const std::vector<std::unique_ptr<U2fDiscovery>>& discoveries() const {
    return discoveries_;
  }

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
  std::vector<std::unique_ptr<U2fDiscovery>> discoveries_;
  ResponseCallback cb_;

 private:
  FRIEND_TEST_ALL_PREFIXES(U2fRequestTest, TestIterateDevice);
  FRIEND_TEST_ALL_PREFIXES(U2fRequestTest, TestBasicMachine);

  // U2fDiscovery::Delegate
  void OnStarted(bool success) override;
  void OnStopped(bool success) override;
  void OnDeviceAdded(std::unique_ptr<U2fDevice> device) override;
  void OnDeviceRemoved(base::StringPiece device_id) override;

  void IterateDevice();
  void OnWaitComplete();

  std::list<std::unique_ptr<U2fDevice>> devices_;
  std::list<std::unique_ptr<U2fDevice>> attempted_devices_;
  base::CancelableClosure delay_callback_;
  size_t started_count_ = 0;

  base::WeakPtrFactory<U2fRequest> weak_factory_;
};

}  // namespace device

#endif  // DEVICE_U2F_U2F_REQUEST_H_
