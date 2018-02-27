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
#include "base/optional.h"
#include "device/fido/u2f_apdu_command.h"
#include "device/fido/u2f_device.h"
#include "device/fido/u2f_discovery.h"
#include "device/fido/u2f_transport_protocol.h"

namespace service_manager {
class Connector;
};  // namespace service_manager

namespace device {

class U2fRequest : public U2fDiscovery::Observer {
 public:
  // U2fRequest will create a discovery instance and register itself as an
  // observer for each passed in transport protocol.
  // TODO(https://crbug.com/769631): Remove the dependency on Connector once U2F
  // is servicified.
  U2fRequest(service_manager::Connector* connector,
             const base::flat_set<U2fTransportProtocol>& protocols,
             std::vector<uint8_t> application_parameter,
             std::vector<uint8_t> challenge_digest,
             std::vector<std::vector<uint8_t>> registered_keys);
  ~U2fRequest() override;

  void Start();

  // Enables the overriding of discoveries for testing. Useful for fakes such as
  // MockU2fDiscovery.
  void SetDiscoveriesForTesting(
      std::vector<std::unique_ptr<U2fDiscovery>> discoveries);

  // Returns bogus application parameter and challenge to be used to verify user
  // presence.
  static const std::vector<uint8_t>& GetBogusApplicationParameter();
  static const std::vector<uint8_t>& GetBogusChallenge();
  // Returns APDU formatted U2F version request command. If |is_legacy_version|
  // is set to true, suffix {0x00, 0x00} is added at the end.
  static std::unique_ptr<U2fApduCommand> GetU2fVersionApduCommand(
      bool is_legacy_version);
  // Returns APDU U2F request commands. Nullptr is returned for
  // incorrectly formatted parameter.
  std::unique_ptr<U2fApduCommand> GetU2fSignApduCommand(
      const std::vector<uint8_t>& key_handle,
      bool is_check_only_sign = false) const;
  std::unique_ptr<U2fApduCommand> GetU2fRegisterApduCommand(
      bool is_individual_attestation) const;

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

  // Hold handles to the devices known to the system. Known devices are
  // partitioned into three parts:
  // [attempted_devices_), current_device_, [devices_)
  // During device iteration the |current_device_| gets pushed to
  // |attempted_devices_|, and, if possible, the first element of |devices_|
  // gets popped and becomes the new |current_device_|. Once all |devices_| are
  // exhausted, |attempted_devices_| get moved into |devices_| and
  // |current_device_| is reset.
  U2fDevice* current_device_ = nullptr;
  std::list<U2fDevice*> devices_;
  std::list<U2fDevice*> attempted_devices_;
  State state_;
  std::vector<std::unique_ptr<U2fDiscovery>> discoveries_;
  std::vector<uint8_t> application_parameter_;
  std::vector<uint8_t> challenge_digest_;
  std::vector<std::vector<uint8_t>> registered_keys_;

 private:
  FRIEND_TEST_ALL_PREFIXES(U2fRequestTest, TestIterateDevice);
  FRIEND_TEST_ALL_PREFIXES(U2fRequestTest, TestBasicMachine);
  FRIEND_TEST_ALL_PREFIXES(U2fRequestTest, TestAlreadyPresentDevice);
  FRIEND_TEST_ALL_PREFIXES(U2fRequestTest, TestMultipleDiscoveries);
  FRIEND_TEST_ALL_PREFIXES(U2fRequestTest, TestSlowDiscovery);
  FRIEND_TEST_ALL_PREFIXES(U2fRequestTest, TestMultipleDiscoveriesWithFailures);

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
