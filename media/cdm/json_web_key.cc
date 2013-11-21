// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cdm/json_web_key.h"

#include "base/base64.h"
#include "base/json/json_string_value_serializer.h"
#include "base/memory/scoped_ptr.h"
#include "base/values.h"

namespace media {

// TODO(jrummell): Move JWK operations from aes_decryptor into this file.

const char kKeysTag[] = "keys";
const char kKeyTypeTag[] = "kty";
const char kSymmetricKeyValue[] = "oct";
const char kKeyTag[] = "k";
const char kKeyIdTag[] = "kid";

std::string GenerateJWKSet(const uint8* key, int key_length,
                           const uint8* key_id, int key_id_length) {
  // Both |key| and |key_id| need to be base64 encoded strings in the JWK.
  std::string key_base64;
  std::string key_id_base64;
  base::Base64Encode(
      std::string(reinterpret_cast<const char*>(key), key_length),
      &key_base64);
  base::Base64Encode(
      std::string(reinterpret_cast<const char*>(key_id), key_id_length),
      &key_id_base64);

  // Create the JWK, and wrap it into a JWK Set.
  scoped_ptr<base::DictionaryValue> jwk(new base::DictionaryValue());
  jwk->SetString(kKeyTypeTag, kSymmetricKeyValue);
  jwk->SetString(kKeyTag, key_base64);
  jwk->SetString(kKeyIdTag, key_id_base64);
  scoped_ptr<base::ListValue> list(new base::ListValue());
  list->Append(jwk.release());
  base::DictionaryValue jwk_set;
  jwk_set.Set(kKeysTag, list.release());

  // Finally serialize |jwk_set| into a string and return it.
  std::string serialized_jwk;
  JSONStringValueSerializer serializer(&serialized_jwk);
  serializer.Serialize(jwk_set);
  return serialized_jwk;
}

}  // namespace media
