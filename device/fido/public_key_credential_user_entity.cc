// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/public_key_credential_user_entity.h"

#include <utility>

#include "device/fido/fido_constants.h"

namespace device {

// static
base::Optional<PublicKeyCredentialUserEntity>
PublicKeyCredentialUserEntity::CreateFromCBORValue(const cbor::Value& cbor) {
  if (!cbor.is_map())
    return base::nullopt;

  const cbor::Value::MapValue& cbor_map = cbor.GetMap();

  auto id_it = cbor_map.find(cbor::Value(kEntityIdMapKey));
  if (id_it == cbor_map.end() || !id_it->second.is_bytestring())
    return base::nullopt;

  PublicKeyCredentialUserEntity user(id_it->second.GetBytestring());

  auto name_it = cbor_map.find(cbor::Value(kEntityNameMapKey));
  if (name_it != cbor_map.end()) {
    if (!name_it->second.is_string()) {
      return base::nullopt;
    }
    user.name = name_it->second.GetString();
  }

  auto display_name_it = cbor_map.find(cbor::Value(kDisplayNameMapKey));
  if (display_name_it != cbor_map.end()) {
    if (!display_name_it->second.is_string()) {
      return base::nullopt;
    }
    user.display_name = display_name_it->second.GetString();
  }

  auto icon_it = cbor_map.find(cbor::Value(kIconUrlMapKey));
  if (icon_it != cbor_map.end()) {
    if (!icon_it->second.is_string()) {
      return base::nullopt;
    }
    user.icon_url = GURL(icon_it->second.GetString());
    if (!user.icon_url->is_valid()) {
      return base::nullopt;
    }
  }

  return user;
}

PublicKeyCredentialUserEntity::PublicKeyCredentialUserEntity(
    std::vector<uint8_t> in_id)
    : id(std::move(in_id)) {}

PublicKeyCredentialUserEntity::PublicKeyCredentialUserEntity(
    const PublicKeyCredentialUserEntity& other) = default;

PublicKeyCredentialUserEntity::PublicKeyCredentialUserEntity(
    PublicKeyCredentialUserEntity&& other) = default;

PublicKeyCredentialUserEntity& PublicKeyCredentialUserEntity::operator=(
    const PublicKeyCredentialUserEntity& other) = default;

PublicKeyCredentialUserEntity& PublicKeyCredentialUserEntity::operator=(
    PublicKeyCredentialUserEntity&& other) = default;

PublicKeyCredentialUserEntity::~PublicKeyCredentialUserEntity() = default;

cbor::Value AsCBOR(const PublicKeyCredentialUserEntity& user) {
  cbor::Value::MapValue user_map;
  user_map.emplace(kEntityIdMapKey, user.id);
  if (user.name)
    user_map.emplace(kEntityNameMapKey, *user.name);
  // Empty icon URLs result in CTAP1_ERR_INVALID_LENGTH on some security keys.
  if (user.icon_url && !user.icon_url->is_empty())
    user_map.emplace(kIconUrlMapKey, user.icon_url->spec());
  if (user.display_name)
    user_map.emplace(kDisplayNameMapKey, *user.display_name);
  return cbor::Value(std::move(user_map));
}

}  // namespace device
