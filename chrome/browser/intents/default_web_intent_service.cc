// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/intents/default_web_intent_service.h"
#include "base/string_util.h"

DefaultWebIntentService::DefaultWebIntentService()
  : url_pattern(URLPattern::SCHEME_ALL, URLPattern::kAllUrlsPattern),
    user_date(-1),
    suppression(0) {}

DefaultWebIntentService::DefaultWebIntentService(
    const string16& srv_action,
    const string16& srv_type,
    const std::string& srv_service_url)
    : action(srv_action), type(srv_type),
      url_pattern(URLPattern::SCHEME_ALL, URLPattern::kAllUrlsPattern),
      user_date(-1), suppression(0), service_url(srv_service_url) {}

DefaultWebIntentService::DefaultWebIntentService(
    const string16& srv_scheme,
    const std::string& srv_service_url)
    : scheme(srv_scheme),
      url_pattern(URLPattern::SCHEME_ALL, URLPattern::kAllUrlsPattern),
      user_date(-1), suppression(0), service_url(srv_service_url) {}

DefaultWebIntentService::~DefaultWebIntentService() {}

std::string DefaultWebIntentService::ToString() const {
  return "{action=" + UTF16ToASCII(action)
      + ", type=" + UTF16ToASCII(type)
      + ", service_url=" + service_url
      + ", url_pattern=" + url_pattern.GetAsString()
      + "}";
}

bool DefaultWebIntentService::operator==(
    const DefaultWebIntentService& other) const {
  return action == other.action &&
         type == other.type &&
         scheme == other.scheme &&
         url_pattern == other.url_pattern &&
         user_date == other.user_date &&
         suppression == other.suppression &&
         service_url == other.service_url;
}
