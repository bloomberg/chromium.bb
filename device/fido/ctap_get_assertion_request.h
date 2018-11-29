// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_CTAP_GET_ASSERTION_REQUEST_H_
#define DEVICE_FIDO_CTAP_GET_ASSERTION_REQUEST_H_

#include <stdint.h>

#include <array>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/containers/span.h"
#include "base/macros.h"
#include "base/optional.h"
#include "crypto/sha2.h"
#include "device/fido/cable/cable_discovery_data.h"
#include "device/fido/fido_constants.h"
#include "device/fido/public_key_credential_descriptor.h"

namespace device {

// Object that encapsulates request parameters for AuthenticatorGetAssertion as
// specified in the CTAP spec.
// https://fidoalliance.org/specs/fido-v2.0-rd-20161004/fido-client-to-authenticator-protocol-v2.0-rd-20161004.html#authenticatorgetassertion
class COMPONENT_EXPORT(DEVICE_FIDO) CtapGetAssertionRequest {
 public:
  using ClientDataHash = std::array<uint8_t, kClientDataHashLength>;

  CtapGetAssertionRequest(std::string rp_id, std::string client_data_json);
  CtapGetAssertionRequest(const CtapGetAssertionRequest& that);
  CtapGetAssertionRequest(CtapGetAssertionRequest&& that);
  CtapGetAssertionRequest& operator=(const CtapGetAssertionRequest& other);
  CtapGetAssertionRequest& operator=(CtapGetAssertionRequest&& other);
  ~CtapGetAssertionRequest();

  // Serializes GetAssertion request parameter into CBOR encoded map with
  // integer keys and CBOR encoded values as defined by the CTAP spec.
  // https://drafts.fidoalliance.org/fido-2/latest/fido-client-to-authenticator-protocol-v2.0-wd-20180305.html#authenticatorGetAssertion
  std::vector<uint8_t> EncodeAsCBOR() const;

  CtapGetAssertionRequest& SetUserVerification(
      UserVerificationRequirement user_verfication);
  CtapGetAssertionRequest& SetUserPresenceRequired(bool user_presence_required);
  CtapGetAssertionRequest& SetAllowList(
      std::vector<PublicKeyCredentialDescriptor> allow_list);
  CtapGetAssertionRequest& SetPinAuth(std::vector<uint8_t> pin_auth);
  CtapGetAssertionRequest& SetPinProtocol(uint8_t pin_protocol);
  CtapGetAssertionRequest& SetCableExtension(
      std::vector<CableDiscoveryData> cable_extension);
  CtapGetAssertionRequest& SetAppId(std::string app_id);

  // Return true if the given RP ID hash from a response is valid for this
  // request.
  bool CheckResponseRpIdHash(
      const std::array<uint8_t, kRpIdHashLength>& response_rp_id_hash);

  const std::string& rp_id() const { return rp_id_; }
  const std::string& client_data_json() const { return client_data_json_; }
  const std::array<uint8_t, kClientDataHashLength>& client_data_hash() const {
    return client_data_hash_;
  }

  UserVerificationRequirement user_verification() const {
    return user_verification_;
  }

  bool user_presence_required() const { return user_presence_required_; }
  const base::Optional<std::vector<PublicKeyCredentialDescriptor>>& allow_list()
      const {
    return allow_list_;
  }

  const base::Optional<std::vector<uint8_t>>& pin_auth() const {
    return pin_auth_;
  }

  const base::Optional<uint8_t>& pin_protocol() const { return pin_protocol_; }
  const base::Optional<std::vector<CableDiscoveryData>>& cable_extension()
      const {
    return cable_extension_;
  }
  const base::Optional<std::array<uint8_t, kRpIdHashLength>>&
  alternative_application_parameter() const {
    return alternative_application_parameter_;
  }
  const base::Optional<std::string>& app_id() const { return app_id_; }

  bool is_incognito_mode() const { return is_incognito_mode_; }
  void set_is_incognito_mode(bool is_incognito_mode) {
    is_incognito_mode_ = is_incognito_mode;
  }

 private:
  std::string rp_id_;
  std::string client_data_json_;
  std::array<uint8_t, kClientDataHashLength> client_data_hash_;
  UserVerificationRequirement user_verification_ =
      UserVerificationRequirement::kPreferred;
  bool user_presence_required_ = true;

  base::Optional<std::vector<PublicKeyCredentialDescriptor>> allow_list_;
  base::Optional<std::vector<uint8_t>> pin_auth_;
  base::Optional<uint8_t> pin_protocol_;
  base::Optional<std::vector<CableDiscoveryData>> cable_extension_;
  base::Optional<std::string> app_id_;
  base::Optional<std::array<uint8_t, crypto::kSHA256Length>>
      alternative_application_parameter_;

  bool is_incognito_mode_ = false;
};

}  // namespace device

#endif  // DEVICE_FIDO_CTAP_GET_ASSERTION_REQUEST_H_
