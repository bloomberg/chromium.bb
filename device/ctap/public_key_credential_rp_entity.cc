// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/ctap/public_key_credential_rp_entity.h"

#include <utility>

namespace device {

PublicKeyCredentialRPEntity::PublicKeyCredentialRPEntity(std::string rp_id)
    : rp_id_(std::move(rp_id)) {}

PublicKeyCredentialRPEntity::PublicKeyCredentialRPEntity(
    PublicKeyCredentialRPEntity&& other) = default;

PublicKeyCredentialRPEntity& PublicKeyCredentialRPEntity::operator=(
    PublicKeyCredentialRPEntity&& other) = default;

PublicKeyCredentialRPEntity::~PublicKeyCredentialRPEntity() = default;

PublicKeyCredentialRPEntity& PublicKeyCredentialRPEntity::SetRPName(
    std::string rp_name) {
  rp_name_ = std::move(rp_name);
  return *this;
}

PublicKeyCredentialRPEntity& PublicKeyCredentialRPEntity::SetRPIconUrl(
    GURL icon_url) {
  rp_icon_url_ = std::move(icon_url);
  return *this;
}

cbor::CBORValue PublicKeyCredentialRPEntity::ConvertToCBOR() const {
  cbor::CBORValue::MapValue rp_map;
  rp_map[cbor::CBORValue("id")] = cbor::CBORValue(rp_id_);
  if (rp_name_)
    rp_map[cbor::CBORValue("name")] = cbor::CBORValue(*rp_name_);
  if (rp_icon_url_)
    rp_map[cbor::CBORValue("icon")] = cbor::CBORValue(rp_icon_url_->spec());
  return cbor::CBORValue(std::move(rp_map));
}

}  // namespace device
