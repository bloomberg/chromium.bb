// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefs/pref_hash_calculator.h"

#include <vector>

#include "base/json/json_string_value_serializer.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "crypto/hmac.h"

namespace {

// Renders |value| as a string. |value| may be NULL, in which case the result
// is an empty string.
std::string ValueAsString(const base::Value* value) {
  // Dictionary values may contain empty lists and sub-dictionaries. Make a
  // deep copy with those removed to make the hash more stable.
  const base::DictionaryValue* dict_value;
  scoped_ptr<base::DictionaryValue> canonical_dict_value;
  if (value && value->GetAsDictionary(&dict_value)) {
    canonical_dict_value.reset(dict_value->DeepCopyWithoutEmptyChildren());
    value = canonical_dict_value.get();
  }

  std::string value_as_string;
  if (value) {
    JSONStringValueSerializer serializer(&value_as_string);
    serializer.Serialize(*value);
  }

  return value_as_string;
}

// Common helper for all hash algorithms.
std::string CalculateFromValueAndComponents(
    const std::string& seed,
    const base::Value* value,
    const std::vector<std::string>& extra_components) {
  static const size_t kSHA256DigestSize = 32;

  std::string message = JoinString(extra_components, "") + ValueAsString(value);

  crypto::HMAC hmac(crypto::HMAC::SHA256);
  unsigned char digest[kSHA256DigestSize];
  if (!hmac.Init(seed) || !hmac.Sign(message, digest, arraysize(digest))) {
    NOTREACHED();
    return std::string();
  }

  return base::HexEncode(digest, arraysize(digest));
}

}  // namespace

PrefHashCalculator::PrefHashCalculator(const std::string& seed,
                                       const std::string& device_id)
    : seed_(seed), device_id_(device_id) {}

std::string PrefHashCalculator::Calculate(const std::string& path,
                                          const base::Value* value) const {
  std::vector<std::string> components;
  if (!device_id_.empty())
    components.push_back(device_id_);
  components.push_back(path);
  return CalculateFromValueAndComponents(seed_, value, components);
}

PrefHashCalculator::ValidationResult PrefHashCalculator::Validate(
    const std::string& path,
    const base::Value* value,
    const std::string& hash) const {
  if (hash == Calculate(path, value))
    return VALID;
  if (hash == CalculateLegacyHash(path, value))
    return VALID_LEGACY;
  return INVALID;
}

std::string PrefHashCalculator::CalculateLegacyHash(
    const std::string& path, const base::Value* value) const {
  return CalculateFromValueAndComponents(seed_,
                                         value,
                                         std::vector<std::string>());
}
