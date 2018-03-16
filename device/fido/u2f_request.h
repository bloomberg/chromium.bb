// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_U2F_REQUEST_H_
#define DEVICE_FIDO_U2F_REQUEST_H_

#include <stdint.h>

#include <list>
#include <memory>
#include <string>
#include <vector>

#include "base/cancelable_callback.h"
#include "base/component_export.h"
#include "base/containers/flat_set.h"
#include "base/macros.h"
#include "base/optional.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_device.h"
#include "device/fido/fido_discovery.h"
#include "device/fido/u2f_transport_protocol.h"

namespace service_manager {
class Connector;
}

namespace device {

class COMPONENT_EXPORT(DEVICE_FIDO) U2fRequest
    : public FidoDiscovery::Observer {
 public:
  using VersionCallback = base::OnceCallback<void(ProtocolVersion version)>;

  // U2fRequest will create a discovery instance and register itself as an
  // observer for each passed in transport protocol.
  // TODO(https://crbug.com/769631): Remove the dependency on Connector once U2F
  // is servicified.
  U2fRequest(service_manager::Connector* connector,
             const base::flat_set<U2fTransportProtocol>& transports,
             std::vector<uint8_t> application_parameter,
             std::vector<uint8_t> challenge_digest,
             std::vector<std::vector<uint8_t>> registered_keys);
  ~U2fRequest() override;

  void Start();

  // Returns bogus register command to be used to verify user presence.
  static std::vector<uint8_t> GetBogusRegisterCommand();
  // Returns APDU formatted U2F version request command. If |is_legacy_version|
  // is set to true, suffix {0x00, 0x00} is added at the end.
  static std::vector<uint8_t> GetU2fVersionApduCommand(
      bool is_legacy_version = false);
  // Returns APDU U2F request commands. Null optional is returned for
  // incorrectly formatted parameter.
  base::Optional<std::vector<uint8_t>> GetU2fSignApduCommand(
      const std::vector<uint8_t>& application_parameter,
      const std::vector<uint8_t>& key_handle,
      bool is_check_only_sign = false) const;
  base::Optional<std::vector<uint8_t>> GetU2fRegisterApduCommand(
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

  // Starts sign, register, and version request transaction on
  // |current_device_|.
  void InitiateDeviceTransaction(base::Optional<std::vector<uint8_t>> cmd,
                                 FidoDevice::DeviceCallback callback);
  // Callback function to U2F version request. If non-legacy version request
  // fails, retry with legacy version request.
  void OnDeviceVersionRequest(VersionCallback callback,
                              base::WeakPtr<FidoDevice> device,
                              bool legacy,
                              base::Optional<std::vector<uint8_t>> response);

  virtual void TryDevice() = 0;

  // Hold handles to the devices known to the system. Known devices are
  // partitioned into three parts:
  // [attempted_devices_), current_device_, [devices_)
  // During device iteration the |current_device_| gets pushed to
  // |attempted_devices_|, and, if possible, the first element of |devices_|
  // gets popped and becomes the new |current_device_|. Once all |devices_| are
  // exhausted, |attempted_devices_| get moved into |devices_| and
  // |current_device_| is reset.
  FidoDevice* current_device_ = nullptr;
  std::list<FidoDevice*> devices_;
  std::list<FidoDevice*> attempted_devices_;
  State state_;
  std::vector<std::unique_ptr<FidoDiscovery>> discoveries_;
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
  FRIEND_TEST_ALL_PREFIXES(U2fRequestTest, TestLegacyVersionRequest);

  // FidoDiscovery::Observer
  void DiscoveryStarted(FidoDiscovery* discovery, bool success) override;
  void DiscoveryStopped(FidoDiscovery* discovery, bool success) override;
  void DeviceAdded(FidoDiscovery* discovery, FidoDevice* device) override;
  void DeviceRemoved(FidoDiscovery* discovery, FidoDevice* device) override;

  void IterateDevice();
  void OnWaitComplete();

  base::CancelableClosure delay_callback_;
  size_t started_count_ = 0;

  base::WeakPtrFactory<U2fRequest> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(U2fRequest);
};

}  // namespace device

#endif  // DEVICE_FIDO_U2F_REQUEST_H_
