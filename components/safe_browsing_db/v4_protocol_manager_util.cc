// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing_db/v4_protocol_manager_util.h"

#include "base/base64.h"
#include "base/metrics/sparse_histogram.h"
#include "base/rand_util.h"
#include "base/strings/stringprintf.h"
#include "net/base/escape.h"

using base::Time;
using base::TimeDelta;

namespace safe_browsing {

// The Safe Browsing V4 server URL prefix.
const char kSbV4UrlPrefix[] = "https://safebrowsing.googleapis.com/v4";

bool UpdateListIdentifier::operator==(const UpdateListIdentifier& other) const {
  return platform_type == other.platform_type &&
         threat_entry_type == other.threat_entry_type &&
         threat_type == other.threat_type;
}

bool UpdateListIdentifier::operator!=(const UpdateListIdentifier& other) const {
  return !operator==(other);
}

size_t UpdateListIdentifier::hash() const {
  std::size_t first = std::hash<unsigned int>()(platform_type);
  std::size_t second = std::hash<unsigned int>()(threat_entry_type);
  std::size_t third = std::hash<unsigned int>()(threat_type);

  std::size_t interim = base::HashInts(first, second);
  return base::HashInts(interim, third);
}

V4ProtocolConfig::V4ProtocolConfig() : disable_auto_update(false) {}

V4ProtocolConfig::V4ProtocolConfig(const V4ProtocolConfig& other) = default;

V4ProtocolConfig::~V4ProtocolConfig() {}

// static
// Backoff interval is MIN(((2^(n-1))*15 minutes) * (RAND + 1), 24 hours) where
// n is the number of consecutive errors.
base::TimeDelta V4ProtocolManagerUtil::GetNextBackOffInterval(
    size_t* error_count,
    size_t* multiplier) {
  DCHECK(multiplier && error_count);
  (*error_count)++;
  if (*error_count > 1 && *error_count < 9) {
    // With error count 9 and above we will hit the 24 hour max interval.
    // Cap the multiplier here to prevent integer overflow errors.
    *multiplier *= 2;
  }
  base::TimeDelta next =
      base::TimeDelta::FromMinutes(*multiplier * (1 + base::RandDouble()) * 15);

  base::TimeDelta day = base::TimeDelta::FromHours(24);

  if (next < day)
    return next;
  else
    return day;
}

// static
void V4ProtocolManagerUtil::RecordHttpResponseOrErrorCode(
    const char* metric_name,
    const net::URLRequestStatus& status,
    int response_code) {
  UMA_HISTOGRAM_SPARSE_SLOWLY(
      metric_name, status.is_success() ? response_code : status.error());
}

// static
// The API hash call uses the pver4 Safe Browsing server.
GURL V4ProtocolManagerUtil::GetRequestUrl(const std::string& request_base64,
                                          const std::string& method_name,
                                          const V4ProtocolConfig& config) {
  std::string url =
      ComposeUrl(kSbV4UrlPrefix, method_name, request_base64,
                 config.client_name, config.version, config.key_param);
  return GURL(url);
}

// static
std::string V4ProtocolManagerUtil::ComposeUrl(const std::string& prefix,
                                              const std::string& method,
                                              const std::string& request_base64,
                                              const std::string& client_id,
                                              const std::string& version,
                                              const std::string& key_param) {
  DCHECK(!prefix.empty() && !method.empty() && !client_id.empty() &&
         !version.empty());
  std::string url =
      base::StringPrintf("%s/%s/%s?alt=proto&client_id=%s&client_version=%s",
                         prefix.c_str(), method.c_str(), request_base64.c_str(),
                         client_id.c_str(), version.c_str());
  if (!key_param.empty()) {
    base::StringAppendF(&url, "&key=%s",
                        net::EscapeQueryParamValue(key_param, true).c_str());
  }
  return url;
}

}  // namespace safe_browsing
