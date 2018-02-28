// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/authenticator_data.h"

#include <utility>

#include "device/fido/u2f_parsing_utils.h"

namespace device {

AuthenticatorData::AuthenticatorData(
    std::vector<uint8_t> application_parameter,
    uint8_t flags,
    std::vector<uint8_t> counter,
    base::Optional<AttestedCredentialData> data)
    : application_parameter_(std::move(application_parameter)),
      flags_(flags),
      counter_(std::move(counter)),
      attested_data_(std::move(data)) {
  // TODO(kpaulhamus): use std::array for these small, fixed-sized vectors.
  CHECK_EQ(counter_.size(), 4u);
}

AuthenticatorData::AuthenticatorData(AuthenticatorData&& other) = default;
AuthenticatorData& AuthenticatorData::operator=(AuthenticatorData&& other) =
    default;

AuthenticatorData::~AuthenticatorData() = default;

std::vector<uint8_t> AuthenticatorData::SerializeToByteArray() const {
  std::vector<uint8_t> authenticator_data;
  u2f_parsing_utils::Append(&authenticator_data, application_parameter_);
  authenticator_data.insert(authenticator_data.end(), flags_);
  u2f_parsing_utils::Append(&authenticator_data, counter_);
  if (attested_data_) {
    // Attestations are returned in registration responses but not in assertion
    // responses.
    u2f_parsing_utils::Append(&authenticator_data,
                              attested_data_->SerializeAsBytes());
  }
  return authenticator_data;
}

}  // namespace device
