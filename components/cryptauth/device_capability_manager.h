// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CRYPTAUTH_DEVICE_CAPABILITY_MANAGER_H_
#define COMPONENTS_CRYPTAUTH_DEVICE_CAPABILITY_MANAGER_H_

#include "base/bind.h"
#include "base/callback_forward.h"
#include "base/containers/queue.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "components/cryptauth/cryptauth_client.h"
#include "components/cryptauth/proto/cryptauth_api.pb.h"

namespace cryptauth {

// DeviceCapabilityManager sends requests to the back-end which enable/disable
// device capabilities and finds devices which contain those capabilities. Here,
// the term "capability" refers to the ability of a device to use a given
// feature (e.g. EasyUnlock or Magic Tether).
class DeviceCapabilityManager {
 public:
  // CAPABILITY_UNLOCK_KEY refers to EasyUnlock.
  enum class Capability { CAPABILITY_UNLOCK_KEY };

  DeviceCapabilityManager(CryptAuthClientFactory* cryptauth_client_factory);

  ~DeviceCapabilityManager();

  // Enables or disables |capability| for the device corresponding to
  // |public_key|. In error cases, |error_callback| is invoked with an error
  // string.
  void SetCapabilityEnabled(
      const std::string& public_key,
      Capability capability,
      bool enabled,
      const base::Closure& success_callback,
      const base::Callback<void(const std::string&)>& error_callback);

  // Fetches metadata about the device which are eligible for |capability|. In
  // error cases, |error_callback| is invoked with an error string.
  void FindEligibleDevicesForCapability(
      Capability capability,
      const base::Callback<void(const std::vector<ExternalDeviceInfo>&,
                                const std::vector<IneligibleDevice>&)>&
          success_callback,
      const base::Callback<void(const std::string&)>& error_callback);

  // Determines whether a device with |public_key| is promotable for
  // |capability|. In error cases, |error_callback| is invoked with an error
  // string.
  void IsCapabilityPromotable(
      const std::string& public_key,
      Capability capability,
      const base::Callback<void(bool)>& success_callback,
      const base::Callback<void(const std::string&)>& error_callback);

 private:
  enum class RequestType {
    SET_CAPABILITY_ENABLED,
    FIND_ELIGIBLE_DEVICES_FOR_CAPABILITY,
    IS_CAPABILITY_PROMOTABLE
  };

  struct Request {
    // Used for SET_CAPABILITY_ENABLED Requests.
    Request(RequestType request_type,
            Capability capability,
            std::string public_key,
            bool enabled,
            const base::Closure& set_capability_callback,
            const base::Callback<void(const std::string&)>& error_callback);

    // Used for FIND_ELIGIBLE_DEVICES_FOR_CAPABILITY Requests.
    Request(RequestType request_type,
            Capability capability,
            const base::Callback<void(const std::vector<ExternalDeviceInfo>&,
                                      const std::vector<IneligibleDevice>&)>&
                find_eligible_devices_callback,
            const base::Callback<void(const std::string&)>& error_callback);

    // Used for IS_CAPABILITY_PROMOTABLE Requests.
    Request(RequestType request_type,
            Capability capability,
            std::string public_key,
            const base::Callback<void(bool)> is_device_promotable_callback,
            const base::Callback<void(const std::string&)>& error_callback);

    ~Request();

    // Defined for every request.
    RequestType request_type;
    base::Callback<void(const std::string&)> error_callback;
    Capability capability;

    // Defined for SET_CAPABILITY_ENABLED and IS_CAPABILITY_PROMOTABLE;
    // otherwise, unused.
    std::string public_key;

    // Defined if |request_type_| is SET_CAPABILITY_ENABLED; otherwise, unused.
    base::Closure set_capability_callback;
    bool enabled;

    // Defined if |request_type_| is FIND_ELIGIBLE_DEVICES_FOR_CAPABILITY;
    // otherwise, unused.
    base::Callback<void(const std::vector<ExternalDeviceInfo>&,
                        const std::vector<IneligibleDevice>&)>
        find_eligible_devices_callback;

    // Defined if |request_type_| is IS_CAPABILITY_PROMOTABLE; otherwise,
    // unused.
    base::Callback<void(bool)> is_device_promotable_callback;
  };

  void CreateNewCryptAuthClient();

  void ProcessSetCapabilityEnabledRequest();
  void ProcessFindEligibleDevicesForCapability();
  void ProcessIsCapabilityPromotableRequest();

  void SetUnlockKeyCapability();
  void FindEligibleUnlockDevices();
  void IsDeviceUnlockPromotable();

  void OnToggleEasyUnlockResponse(const ToggleEasyUnlockResponse& response);
  void OnFindEligibleUnlockDevicesResponse(
      const FindEligibleUnlockDevicesResponse& response);
  void OnIsDeviceUnlockPromotableResponse(
      const FindEligibleForPromotionResponse& response);
  void OnErrorResponse(const std::string& response);
  void ProcessRequestQueue();

  std::unique_ptr<CryptAuthClient> current_cryptauth_client_;
  std::unique_ptr<Request> current_request_;
  base::queue<std::unique_ptr<Request>> pending_requests_;
  CryptAuthClientFactory* crypt_auth_client_factory_;
  base::WeakPtrFactory<DeviceCapabilityManager> weak_ptr_factory_;
};
}  // namespace cryptauth

#endif  // COMPONENTS_CRYPTAUTH_DEVICE_CAPABILITY_MANAGER_H_
