// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_CTAP_PUBLIC_KEY_CREDENTIAL_RP_ENTITY_H_
#define DEVICE_CTAP_PUBLIC_KEY_CREDENTIAL_RP_ENTITY_H_

#include <string>
#include <vector>

#include "base/optional.h"
#include "components/cbor/cbor_values.h"
#include "url/gurl.h"

namespace device {

// Data structure containing information about relying party that invoked
// WebAuth API. Includes a relying party id, an optional relying party name,,
// and optional relying party display image url.
class PublicKeyCredentialRPEntity {
 public:
  explicit PublicKeyCredentialRPEntity(std::string rp_id);
  PublicKeyCredentialRPEntity(PublicKeyCredentialRPEntity&& other);
  PublicKeyCredentialRPEntity& operator=(PublicKeyCredentialRPEntity&& other);
  ~PublicKeyCredentialRPEntity();

  PublicKeyCredentialRPEntity& SetRPName(std::string rp_name);
  PublicKeyCredentialRPEntity& SetRPIconUrl(GURL icon_url);
  cbor::CBORValue ConvertToCBOR() const;

 private:
  std::string rp_id_;
  base::Optional<std::string> rp_name_;
  base::Optional<GURL> rp_icon_url_;

  DISALLOW_COPY_AND_ASSIGN(PublicKeyCredentialRPEntity);
};

}  // namespace device

#endif  // DEVICE_CTAP_PUBLIC_KEY_CREDENTIAL_RP_ENTITY_H_
