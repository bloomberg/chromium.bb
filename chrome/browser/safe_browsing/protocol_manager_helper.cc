// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/protocol_manager_helper.h"

#ifndef NDEBUG
#include "base/base64.h"
#endif
#include "base/environment.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "components/version_info/version_info.h"
#include "google_apis/google_api_keys.h"
#include "net/base/escape.h"

namespace safe_browsing {

SafeBrowsingProtocolConfig::SafeBrowsingProtocolConfig()
    : disable_auto_update(false) {}

SafeBrowsingProtocolConfig::~SafeBrowsingProtocolConfig() {}

// static
std::string SafeBrowsingProtocolManagerHelper::Version() {
  if (version_info::GetVersionNumber().empty())
    return "0.1";
  else
    return version_info::GetVersionNumber();
}

// static
std::string SafeBrowsingProtocolManagerHelper::ComposeUrl(
    const std::string& prefix,
    const std::string& method,
    const std::string& client_name,
    const std::string& version,
    const std::string& additional_query) {
  DCHECK(!prefix.empty() && !method.empty() &&
         !client_name.empty() && !version.empty());
  std::string url = base::StringPrintf("%s/%s?client=%s&appver=%s&pver=3.0",
                                       prefix.c_str(), method.c_str(),
                                       client_name.c_str(), version.c_str());
  std::string api_key = google_apis::GetAPIKey();
  if (!api_key.empty()) {
    base::StringAppendF(&url, "&key=%s",
                        net::EscapeQueryParamValue(api_key, true).c_str());
  }
  if (!additional_query.empty()) {
    DCHECK(url.find("?") != std::string::npos);
    url.append("&");
    url.append(additional_query);
  }
  return url;
}

// static
std::string SafeBrowsingProtocolManagerHelper::ComposeUrl(
    const std::string& prefix,
    const std::string& method,
    const std::string& client_name,
    const std::string& version,
    const std::string& additional_query,
    bool is_extended_reporting) {
  std::string url =
      ComposeUrl(prefix, method, client_name, version, additional_query);
  if (is_extended_reporting) {
    url.append("&ext=1");
  } else {
    url.append("&ext=0");
  }
  return url;
}

}  // namespace safe_browsing
