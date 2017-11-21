// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBAUTH_REGISTER_RESPONSE_DATA_H_
#define CONTENT_BROWSER_WEBAUTH_REGISTER_RESPONSE_DATA_H_

#include <stdint.h>
#include <memory>
#include <string>
#include <vector>

#include "base/base64url.h"
#include "base/macros.h"
#include "content/browser/webauth/attestation_object.h"
#include "content/browser/webauth/collected_client_data.h"
#include "content/common/content_export.h"

namespace content {

// See figure 2:
// https://fidoalliance.org/specs/fido-v2.0-rd-20170927/ \
// fido-client-to-authenticator-protocol-v2.0-rd-20170927.html#using-the- \
// ctap2-authenticatormakecredential-command-with-ctap1-u2f-authenticators
class CONTENT_EXPORT RegisterResponseData {
 public:
  RegisterResponseData(std::unique_ptr<CollectedClientData> client_data,
                       std::vector<uint8_t> credential_id,
                       std::unique_ptr<AttestationObject> object);

  static std::unique_ptr<RegisterResponseData> CreateFromU2fRegisterResponse(
      std::unique_ptr<CollectedClientData> client_data,
      std::vector<uint8_t> u2f_data);

  virtual ~RegisterResponseData();

  std::vector<uint8_t> GetClientDataJSONBytes();
  std::string GetId();
  std::vector<uint8_t> GetCBOREncodedAttestationObject();
  const std::vector<uint8_t>& raw_id() { return raw_id_; }

 private:
  const std::unique_ptr<CollectedClientData> client_data_;
  const std::vector<uint8_t> raw_id_;
  const std::unique_ptr<AttestationObject> attestation_object_;

  DISALLOW_COPY_AND_ASSIGN(RegisterResponseData);
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEBAUTH_REGISTER_RESPONSE_DATA_H_
