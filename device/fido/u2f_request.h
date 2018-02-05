// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_U2F_REQUEST_H_
#define DEVICE_FIDO_U2F_REQUEST_H_

#include <list>
#include <memory>
#include <string>
#include <vector>

#include "base/cancelable_callback.h"
#include "base/containers/flat_set.h"
#include "device/fido/u2f_device.h"
#include "device/fido/u2f_discovery.h"
#include "device/fido/u2f_transport_protocol.h"

namespace service_manager {
class Connector;
};

namespace device {

class U2fRequest : public U2fDiscovery::Observer {
 public:
  // U2fRequest will create a discovery instance and register itself as an
  // observer for each passed in transport protocol.
  // TODO(https://crbug.com/769631): Remove the dependency on Connector once U2F
  // is servicified.
  U2fRequest(std::string relying_party_id,
             service_manager::Connector* connector,
             const base::flat_set<U2fTransportProtocol>& protocols);
  ~U2fRequest() override;

  void Start();
  // Enables the overriding of discoveries for testing. Useful for fakes such as
  // MockU2fDiscovery.
  // Returns a raw pointer to the added |discovery| for convenience.
  U2fDiscovery* SetDiscoveryForTesting(std::unique_ptr<U2fDiscovery> discovery);

  // Supports two different discoveries for testing. For caller's convenience we
  // deliberately don't take a vector of discoveries. This is because such a
  // vector could not be constructed inline, since initializer lists don't
  // support move-only types.
  // Returns a reference to the underlying discoveries for convenience.
  std::vector<std::unique_ptr<U2fDiscovery>>& SetDiscoveriesForTesting(
      std::unique_ptr<U2fDiscovery> discovery_1,
      std::unique_ptr<U2fDiscovery> discovery_2);

  // Enables the overriding of discoveries for testing. Useful for fakes such as
  // MockU2fDiscovery.
  void SetDiscoveriesForTesting(
      std::vector<std::unique_ptr<U2fDiscovery>> discoveries);

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
  std::vector<std::unique_ptr<U2fDiscovery>> discoveries_;

 private:
  FRIEND_TEST_ALL_PREFIXES(U2fRequestTest, TestIterateDevice);
  FRIEND_TEST_ALL_PREFIXES(U2fRequestTest, TestBasicMachine);
  FRIEND_TEST_ALL_PREFIXES(U2fRequestTest, TestAlreadyPresentDevice);
  FRIEND_TEST_ALL_PREFIXES(U2fRequestTest, TestMultipleDiscoveries);

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

#endif  // DEVICE_FIDO_U2F_REQUEST_H_
