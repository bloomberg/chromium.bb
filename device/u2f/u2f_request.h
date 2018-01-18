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

class U2fRequest : public U2fDiscovery::Observer {
 public:
  // U2fRequest will register itself as an observer for each entry in
  // |discoveries|. Clients need to ensure that each discovery outlives this
  // request.
  U2fRequest(std::string relying_party_id,
             std::vector<U2fDiscovery*> discoveries);
  ~U2fRequest() override;

  void Start();

 protected:
  enum class State {
    INIT,
    BUSY,
    WINK,
    IDLE,
    OFF,
    COMPLETE,
  };

  // Returns bogus application parameter and challenge to be used to verify user
  // presence.
  static const std::vector<uint8_t>& GetBogusAppParam();
  static const std::vector<uint8_t>& GetBogusChallenge();

  void Transition();
  virtual void TryDevice() = 0;

  U2fDevice* current_device_ = nullptr;
  std::list<U2fDevice*> devices_;
  std::list<U2fDevice*> attempted_devices_;

  State state_;
  const std::string relying_party_id_;
  std::vector<U2fDiscovery*> discoveries_;

 private:
  FRIEND_TEST_ALL_PREFIXES(U2fRequestTest, TestIterateDevice);
  FRIEND_TEST_ALL_PREFIXES(U2fRequestTest, TestBasicMachine);
  FRIEND_TEST_ALL_PREFIXES(U2fRequestTest, TestAlreadyPresentDevice);

  // U2fDiscovery::Observer
  void DiscoveryStarted(U2fDiscovery* discovery, bool success) override;
  void DiscoveryStopped(U2fDiscovery* discovery, bool success) override;
  void DeviceAdded(U2fDiscovery* discovery, U2fDevice* device) override;
  void DeviceRemoved(U2fDiscovery* discovery, U2fDevice* device) override;

  void IterateDevice();
  void OnWaitComplete();

  base::CancelableClosure delay_callback_;
  size_t started_count_ = 0;

  base::WeakPtrFactory<U2fRequest> weak_factory_;
};

}  // namespace device

#endif  // DEVICE_U2F_U2F_REQUEST_H_
