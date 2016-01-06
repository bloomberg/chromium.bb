// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/experiments/api_key.h"

#include <vector>

#include "base/base64.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "url/origin.h"

namespace content {

namespace {

const char* kApiKeyFieldSeparator = "|";
}

ApiKey::~ApiKey() {}

scoped_ptr<ApiKey> ApiKey::Parse(const std::string& key_text) {
  if (key_text.empty()) {
    return nullptr;
  }

  // API Key should resemble:
  // signature|origin|api_name|expiry_timestamp
  std::vector<std::string> parts =
      SplitString(key_text, kApiKeyFieldSeparator, base::KEEP_WHITESPACE,
                  base::SPLIT_WANT_ALL);
  if (parts.size() != 4) {
    return nullptr;
  }

  const std::string& signature = parts[0];
  const std::string& origin_string = parts[1];
  const std::string& api_name = parts[2];
  const std::string& expiry_string = parts[3];

  uint64_t expiry_timestamp;
  if (!base::StringToUint64(expiry_string, &expiry_timestamp)) {
    return nullptr;
  }

  // Ensure that the origin is a valid (non-unique) origin URL
  GURL origin_url(origin_string);
  if (url::Origin(origin_url).unique()) {
    return nullptr;
  }

  // signed data is (origin + "|" + api_name + "|" + expiry).
  std::string data = key_text.substr(signature.length() + 1);

  return make_scoped_ptr(
      new ApiKey(signature, data, origin_url, api_name, expiry_timestamp));
}

ApiKey::ApiKey(const std::string& signature,
               const std::string& data,
               const GURL& origin,
               const std::string& api_name,
               uint64_t expiry_timestamp)
    : signature_(signature),
      data_(data),
      origin_(origin),
      api_name_(api_name),
      expiry_timestamp_(expiry_timestamp) {}

bool ApiKey::IsAppropriate(const std::string& origin,
                           const std::string& api_name) const {
  return ValidateOrigin(origin) && ValidateApiName(api_name);
}

bool ApiKey::IsValid(const base::Time& now) const {
  // TODO(iclelland): Validate signature on key data here as well.
  // https://crbug.com/543215
  return ValidateDate(now);
}

bool ApiKey::ValidateOrigin(const std::string& origin) const {
  return GURL(origin) == origin_;
}

bool ApiKey::ValidateApiName(const std::string& api_name) const {
  return base::EqualsCaseInsensitiveASCII(api_name, api_name_);
}

bool ApiKey::ValidateDate(const base::Time& now) const {
  base::Time expiry_time = base::Time::FromDoubleT((double)expiry_timestamp_);
  return expiry_time > now;
}

}  // namespace content
