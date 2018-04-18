// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/response_data.h"

#include <utility>

#include "base/base64url.h"
#include "base/strings/string_piece.h"
#include "crypto/sha2.h"

namespace device {

ResponseData::~ResponseData() = default;

ResponseData::ResponseData() = default;

ResponseData::ResponseData(std::vector<uint8_t> raw_credential_id)
    : raw_credential_id_(std::move(raw_credential_id)) {}

ResponseData::ResponseData(ResponseData&& other) = default;

ResponseData& ResponseData::operator=(ResponseData&& other) = default;

std::string ResponseData::GetId() const {
  std::string id;
  base::Base64UrlEncode(base::StringPiece(reinterpret_cast<const char*>(
                                              raw_credential_id_.data()),
                                          raw_credential_id_.size()),
                        base::Base64UrlEncodePolicy::OMIT_PADDING, &id);
  return id;
}

bool ResponseData::CheckRpIdHash(const std::string& rp_id) const {
  const auto& response_rp_id_hash = GetRpIdHash();
  std::vector<uint8_t> request_rp_id_hash(crypto::kSHA256Length);
  crypto::SHA256HashString(rp_id, request_rp_id_hash.data(),
                           request_rp_id_hash.size());
  return response_rp_id_hash == request_rp_id_hash;
}

}  // namespace device
