// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/gaia/oauth_multilogin_result.h"
#include "base/json/json_reader.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece_forward.h"

OAuthMultiloginResult::OAuthMultiloginResult() {}

OAuthMultiloginResult::OAuthMultiloginResult(
    const OAuthMultiloginResult& other) {
  cookies_ = other.cookies();
}

// static
base::StringPiece OAuthMultiloginResult::StripXSSICharacters(
    const std::string& raw_data) {
  base::StringPiece body(raw_data);
  return body.substr(body.find('\n'));
}

void OAuthMultiloginResult::TryParseCookiesFromValue(
    base::DictionaryValue* dictionary_value) {
  base::ListValue* cookie_list = nullptr;
  dictionary_value->GetList("cookies", &cookie_list);
  if (cookie_list == nullptr) {
    VLOG(1) << "No cookies found in the response.";
    return;
  }
  for (size_t i = 0; i < cookie_list->GetSize(); ++i) {
    base::DictionaryValue* cookie_value = nullptr;
    cookie_list->GetDictionary(i, &cookie_value);
    std::string name;
    std::string value;
    std::string domain;
    std::string host;
    std::string path;
    bool is_secure;
    bool is_http_only;
    std::string priority;
    std::string max_age;
    double expiration_delta;
    cookie_value->GetString("name", &name);
    cookie_value->GetString("value", &value);
    cookie_value->GetString("domain", &domain);
    cookie_value->GetString("host", &host);
    cookie_value->GetString("path", &path);
    cookie_value->GetBoolean("isSecure", &is_secure);
    cookie_value->GetBoolean("isHttpOnly", &is_http_only);
    cookie_value->GetString("priority", &priority);
    cookie_value->GetDouble("maxAge", &expiration_delta);

    base::TimeDelta before_expiration =
        base::TimeDelta::FromSecondsD(expiration_delta);
    if (domain.empty() && !host.empty() && host[0] != '.') {
      // Host cookie case. If domain is empty but other conditions are not met,
      // there must be something wrong with the received cookie.
      domain = host;
    }
    net::CanonicalCookie new_cookie(
        name, value, domain, path, base::Time::Now(),
        base::Time::Now() + before_expiration, base::Time::Now(), is_secure,
        is_http_only, net::CookieSameSite::NO_RESTRICTION,
        net::StringToCookiePriority(priority));
    if (new_cookie.IsCanonical()) {
      cookies_.push_back(std::move(new_cookie));
    } else {
      LOG(ERROR) << "Non-canonical cookie found.";
    }
  }
}

// static
bool OAuthMultiloginResult::CreateOAuthMultiloginResultFromString(
    const std::string& raw_data,
    OAuthMultiloginResult* result) {
  base::StringPiece data = StripXSSICharacters(raw_data);
  std::unique_ptr<base::DictionaryValue> dictionary_value =
      base::DictionaryValue::From(base::JSONReader::Read(data));
  if (!dictionary_value) {
    return false;
  }
  result->TryParseCookiesFromValue(dictionary_value.get());
  return true;
}

OAuthMultiloginResult::~OAuthMultiloginResult() = default;