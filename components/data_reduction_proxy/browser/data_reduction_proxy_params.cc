// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/browser/data_reduction_proxy_params.h"

#include "base/command_line.h"
#include "base/metrics/field_trial.h"
#include "components/data_reduction_proxy/common/data_reduction_proxy_switches.h"

using base::FieldTrialList;

namespace {
const char kEnabled[] = "Enabled";
}

namespace data_reduction_proxy {

// static
bool DataReductionProxyParams::IsIncludedInFieldTrial() {
  return base::FieldTrialList::FindFullName(
      "DataCompressionProxyRollout") == kEnabled;
}

// static
bool DataReductionProxyParams::IsIncludedInAlternativeFieldTrial() {
  return base::FieldTrialList::FindFullName(
              "DataCompressionProxyAlternativeConfiguration") == kEnabled;
}

// static
bool DataReductionProxyParams::IsIncludedInPromoFieldTrial() {
  return FieldTrialList::FindFullName(
      "DataCompressionProxyPromoVisibility") == kEnabled;
}

// static
bool DataReductionProxyParams::IsIncludedInPreconnectHintingFieldTrial() {
  return IsIncludedInFieldTrial() &&
      FieldTrialList::FindFullName(
          "DataCompressionProxyPreconnectHints") == kEnabled;
}

// static
bool DataReductionProxyParams::IsKeySetOnCommandLine() {
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  return command_line.HasSwitch(
      data_reduction_proxy::switches::kEnableDataReductionProxy);
}

DataReductionProxyParams::DataReductionProxyParams(int flags)
    : allowed_((flags & kAllowed) == kAllowed),
      fallback_allowed_((flags & kFallbackAllowed) == kFallbackAllowed),
      alt_allowed_((flags & kAlternativeAllowed) == kAlternativeAllowed),
      promo_allowed_((flags & kPromoAllowed) == kPromoAllowed) {
  DCHECK(Init(allowed_, fallback_allowed_, alt_allowed_));
}

DataReductionProxyParams::~DataReductionProxyParams() {
}

DataReductionProxyParams::DataReductionProxyList
DataReductionProxyParams::GetAllowedProxies() const {
  DataReductionProxyList list;
  if (allowed_)
    list.push_back(origin_);
  if (allowed_ && fallback_allowed_)
    list.push_back(fallback_origin_);
  if (alt_allowed_) {
    list.push_back(alt_origin_);
    list.push_back(ssl_origin_);
  }
  if (alt_allowed_ && fallback_allowed_)
    list.push_back(alt_fallback_origin_);
  return list;
}

DataReductionProxyParams::DataReductionProxyParams(int flags,
                                                   bool should_call_init)
    : allowed_((flags & kAllowed) == kAllowed),
      fallback_allowed_((flags & kFallbackAllowed) == kFallbackAllowed),
      alt_allowed_((flags & kAlternativeAllowed) == kAlternativeAllowed),
      promo_allowed_((flags & kPromoAllowed) == kPromoAllowed) {
  if (should_call_init)
    DCHECK(Init(allowed_, fallback_allowed_, alt_allowed_));
}

bool DataReductionProxyParams::Init(
    bool allowed, bool fallback_allowed, bool alt_allowed) {
  InitWithoutChecks();
  // Verify that all necessary params are set.
  if (allowed) {
    if (!origin_.is_valid()) {
      DVLOG(1) << "Invalid data reduction proxy origin: " << origin_.spec();
      return false;
    }
  }

  if (allowed && fallback_allowed) {
    if (!fallback_origin_.is_valid()) {
      DVLOG(1) << "Invalid data reduction proxy fallback origin: "
          << fallback_origin_.spec();
      return false;
    }
  }

  if (alt_allowed) {
    if (!allowed) {
      DVLOG(1) << "Alternative data reduction proxy configuration cannot "
          << "be allowed if the regular configuration is not allowed";
      return false;
    }
    if (!alt_origin_.is_valid()) {
      DVLOG(1) << "Invalid alternative origin:" << alt_origin_.spec();
      return false;
    }
    if (!ssl_origin_.is_valid()) {
      DVLOG(1) << "Invalid ssl origin: " << ssl_origin_.spec();
      return false;
    }
  }

  if (alt_allowed && fallback_allowed) {
    if (!alt_fallback_origin_.is_valid()) {
      DVLOG(1) << "Invalid alternative fallback origin:"
          << alt_fallback_origin_.spec();
      return false;
    }
  }

  if (allowed && !probe_url_.is_valid()) {
    DVLOG(1) << "Invalid probe url: <null>";
    return false;
  }

  if (allowed || alt_allowed) {
    if (key_.empty()) {
      DVLOG(1) << "Invalid key: <empty>";
      return false;
    }
  }

  if (fallback_allowed_ && !allowed_) {
    DVLOG(1) << "The data reduction proxy fallback cannot be allowed if "
        << "the data reduction proxy is not allowed";
    return false;
  }
  if (promo_allowed_ && !allowed_) {
    DVLOG(1) << "The data reduction proxy promo cannot be allowed if the "
        << "data reduction proxy is not allowed";
    return false;
  }
  return true;

}


void DataReductionProxyParams::InitWithoutChecks() {
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  std::string origin =
      command_line.GetSwitchValueASCII(switches::kDataReductionProxyDev);
  if (origin.empty())
    origin = command_line.GetSwitchValueASCII(switches::kDataReductionProxy);
  std::string fallback_origin =
      command_line.GetSwitchValueASCII(switches::kDataReductionProxyFallback);
  std::string ssl_origin =
      command_line.GetSwitchValueASCII(switches::kDataReductionSSLProxy);
  std::string alt_origin =
      command_line.GetSwitchValueASCII(switches::kDataReductionProxyAlt);
  std::string alt_fallback_origin = command_line.GetSwitchValueASCII(
      switches::kDataReductionProxyAltFallback);
  key_ = command_line.GetSwitchValueASCII(switches::kDataReductionProxyKey);

  bool configured_on_command_line =
      !(origin.empty() && fallback_origin.empty() && ssl_origin.empty() &&
          alt_origin.empty() && alt_fallback_origin.empty());


  // Configuring the proxy on the command line overrides the values of
  // |allowed_| and |alt_allowed_|.
  if (configured_on_command_line)
    allowed_ = true;
  if (!(ssl_origin.empty() &&
        alt_origin.empty() &&
        alt_fallback_origin.empty()))
    alt_allowed_ = true;

  // Only use default key if non of the proxies are configured on the command
  // line.
  if (key_.empty() && !configured_on_command_line)
    key_ = GetDefaultKey();

  std::string probe_url = command_line.GetSwitchValueASCII(
      switches::kDataReductionProxyProbeURL);

  // Set from preprocessor constants those params that are not specified on the
  // command line.
  if (origin.empty())
    origin = GetDefaultDevOrigin();
  if (origin.empty())
    origin = GetDefaultOrigin();
  if (fallback_origin.empty())
    fallback_origin = GetDefaultFallbackOrigin();
  if (ssl_origin.empty())
    ssl_origin = GetDefaultSSLOrigin();
  if (alt_origin.empty())
    alt_origin = GetDefaultAltOrigin();
  if (alt_fallback_origin.empty())
    alt_fallback_origin = GetDefaultAltFallbackOrigin();
  if (probe_url.empty())
    probe_url = GetDefaultProbeURL();

  origin_ = GURL(origin);
  fallback_origin_ = GURL(fallback_origin);
  ssl_origin_ = GURL(ssl_origin);
  alt_origin_ = GURL(alt_origin);
  alt_fallback_origin_ = GURL(alt_fallback_origin);
  probe_url_ = GURL(probe_url);

}

std::string DataReductionProxyParams::GetDefaultKey() const {
#if defined(SPDY_PROXY_AUTH_VALUE)
  return SPDY_PROXY_AUTH_VALUE;
#endif
  return std::string();
}

std::string DataReductionProxyParams::GetDefaultDevOrigin() const {
#if defined(DATA_REDUCTION_DEV_HOST)
  if (FieldTrialList::FindFullName("DataCompressionProxyDevRollout") ==
      kEnabled) {
    return DATA_REDUCTION_DEV_HOST;
  }
#endif
  return std::string();
}

std::string DataReductionProxyParams::GetDefaultOrigin() const {
#if defined(SPDY_PROXY_AUTH_ORIGIN)
  return SPDY_PROXY_AUTH_ORIGIN;
#endif
  return std::string();
}

std::string DataReductionProxyParams::GetDefaultFallbackOrigin() const {
#if defined(DATA_REDUCTION_FALLBACK_HOST)
  return DATA_REDUCTION_FALLBACK_HOST;
#endif
  return std::string();
}

std::string DataReductionProxyParams::GetDefaultSSLOrigin() const {
#if defined(DATA_REDUCTION_PROXY_SSL_ORIGIN)
  return DATA_REDUCTION_PROXY_SSL_ORIGIN;
#endif
  return std::string();
}

std::string DataReductionProxyParams::GetDefaultAltOrigin() const {
#if defined(DATA_REDUCTION_PROXY_ALT_ORIGIN)
  return DATA_REDUCTION_PROXY_ALT_ORIGIN;
#endif
  return std::string();
}

std::string DataReductionProxyParams::GetDefaultAltFallbackOrigin() const {
#if defined(DATA_REDUCTION_PROXY_ALT_FALLBACK_ORIGIN)
  return DATA_REDUCTION_PROXY_ALT_FALLBACK_ORIGIN;
#endif
  return std::string();
}

std::string DataReductionProxyParams::GetDefaultProbeURL() const {
#if defined(DATA_REDUCTION_PROXY_PROBE_URL)
  return DATA_REDUCTION_PROXY_PROBE_URL;
#endif
  return std::string();
}

}  // namespace data_reduction_proxy
