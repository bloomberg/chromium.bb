// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/browser/data_reduction_proxy_params.h"

#include "base/command_line.h"
#include "base/metrics/field_trial.h"
#include "base/time/time.h"
#include "components/data_reduction_proxy/common/data_reduction_proxy_switches.h"
#include "net/base/host_port_pair.h"
#include "net/proxy/proxy_info.h"
#include "net/proxy/proxy_retry_info.h"
#include "net/proxy/proxy_server.h"
#include "net/proxy/proxy_service.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "url/url_constants.h"

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
bool DataReductionProxyParams::IsIncludedInCriticalPathBypassFieldTrial() {
  return IsIncludedInFieldTrial() &&
      FieldTrialList::FindFullName(
          "DataCompressionProxyCriticalBypass") == kEnabled;
}

bool DataReductionProxyParams::IsIncludedInHoldbackFieldTrial() {
  return FieldTrialList::FindFullName(
      "DataCompressionProxyHoldback") == kEnabled;
}

DataReductionProxyParams::DataReductionProxyParams(int flags)
    : allowed_((flags & kAllowed) == kAllowed),
      fallback_allowed_((flags & kFallbackAllowed) == kFallbackAllowed),
      alt_allowed_((flags & kAlternativeAllowed) == kAlternativeAllowed),
      promo_allowed_((flags & kPromoAllowed) == kPromoAllowed),
      holdback_((flags & kHoldback) == kHoldback),
      configured_on_command_line_(false) {
  bool result = Init(allowed_, fallback_allowed_, alt_allowed_);
  DCHECK(result);
}

DataReductionProxyParams::~DataReductionProxyParams() {
}

DataReductionProxyParams::DataReductionProxyList
DataReductionProxyParams::GetAllowedProxies() const {
  DataReductionProxyList list;
  if (allowed_) {
    list.push_back(origin_);
    // TODO(bolian): revert this once the proxy PAC fix is ready.
    if (GURL(GetDefaultDevOrigin()) == origin()) {
      list.push_back(GURL(GetDefaultOrigin()));
    }
  }
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
      promo_allowed_((flags & kPromoAllowed) == kPromoAllowed),
      holdback_((flags & kHoldback) == kHoldback),
      configured_on_command_line_(false) {
  if (should_call_init) {
    bool result = Init(allowed_, fallback_allowed_, alt_allowed_);
    DCHECK(result);
  }
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
  std::string origin;
  if (!command_line.HasSwitch(switches::kDisableDataReductionProxyDev)) {
      origin = command_line.GetSwitchValueASCII(
          switches::kDataReductionProxyDev);
  }
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

  configured_on_command_line_ =
      !(origin.empty() && fallback_origin.empty() && ssl_origin.empty() &&
          alt_origin.empty() && alt_fallback_origin.empty());


  // Configuring the proxy on the command line overrides the values of
  // |allowed_| and |alt_allowed_|.
  if (configured_on_command_line_)
    allowed_ = true;
  if (!(ssl_origin.empty() &&
        alt_origin.empty() &&
        alt_fallback_origin.empty()))
    alt_allowed_ = true;

  std::string probe_url = command_line.GetSwitchValueASCII(
      switches::kDataReductionProxyProbeURL);
  std::string warmup_url = command_line.GetSwitchValueASCII(
      switches::kDataReductionProxyWarmupURL);

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
  if (warmup_url.empty())
    warmup_url = GetDefaultWarmupURL();

  origin_ = GURL(origin);
  fallback_origin_ = GURL(fallback_origin);
  ssl_origin_ = GURL(ssl_origin);
  alt_origin_ = GURL(alt_origin);
  alt_fallback_origin_ = GURL(alt_fallback_origin);
  probe_url_ = GURL(probe_url);
  warmup_url_ = GURL(warmup_url);

}

bool DataReductionProxyParams::WasDataReductionProxyUsed(
    const net::URLRequest* request,
    std::pair<GURL, GURL>* proxy_servers) const {
  DCHECK(request);
  return IsDataReductionProxy(request->proxy_server(), proxy_servers);
}

bool DataReductionProxyParams::IsDataReductionProxy(
    const net::HostPortPair& host_port_pair,
    std::pair<GURL, GURL>* proxy_servers) const {
  if (net::HostPortPair::FromURL(origin()).Equals(host_port_pair)) {
    if (proxy_servers) {
      (*proxy_servers).first = origin();
      if (fallback_allowed())
        (*proxy_servers).second = fallback_origin();
    }
    return true;
  }

  // TODO(bolian): revert this once the proxy PAC fix is ready.
  //
  // If dev host is configured as the primary proxy, we treat the default
  // origin as a valid data reduction proxy to workaround PAC script.
  if (GURL(GetDefaultDevOrigin()) == origin()) {
    const GURL& default_origin = GURL(GetDefaultOrigin());
    if (net::HostPortPair::FromURL(default_origin).Equals(host_port_pair)) {
      if (proxy_servers) {
        (*proxy_servers).first = default_origin;
        if (fallback_allowed())
          (*proxy_servers).second = fallback_origin();
      }
      return true;
    }
  }

  if (fallback_allowed() &&
      net::HostPortPair::FromURL(fallback_origin()).Equals(host_port_pair)) {
    if (proxy_servers) {
      (*proxy_servers).first = fallback_origin();
      (*proxy_servers).second = GURL();
    }
    return true;
  }
  if (net::HostPortPair::FromURL(alt_origin()).Equals(host_port_pair)) {
    if (proxy_servers) {
      (*proxy_servers).first = alt_origin();
      if (fallback_allowed())
        (*proxy_servers).second = alt_fallback_origin();
    }
    return true;
  }
  if (fallback_allowed() &&
      net::HostPortPair::FromURL(alt_fallback_origin()).Equals(
      host_port_pair)) {
    if (proxy_servers) {
      (*proxy_servers).first = alt_fallback_origin();
      (*proxy_servers).second = GURL();
    }
    return true;
  }
  if (net::HostPortPair::FromURL(ssl_origin()).Equals(host_port_pair)) {
    if (proxy_servers) {
      (*proxy_servers).first = ssl_origin();
      (*proxy_servers).second = GURL();
    }
    return true;
  }
  return false;
}

// TODO(kundaji): Check that the request will actually be sent through the
// proxy.
bool DataReductionProxyParams::IsDataReductionProxyEligible(
    const net::URLRequest* request) {
  DCHECK(request);
  DCHECK(request->context());
  DCHECK(request->context()->proxy_service());
  net::ProxyInfo result;
  request->context()->proxy_service()->config().proxy_rules().Apply(
      request->url(), &result);
  if (!result.proxy_server().is_valid())
    return false;
  if (result.proxy_server().is_direct())
    return false;
  return IsDataReductionProxy(result.proxy_server().host_port_pair(), NULL);
}

std::string DataReductionProxyParams::GetDefaultDevOrigin() const {
#if defined(DATA_REDUCTION_DEV_HOST)
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(switches::kDisableDataReductionProxyDev))
    return std::string();
  if (command_line.HasSwitch(switches::kEnableDataReductionProxyDev) ||
      (FieldTrialList::FindFullName("DataCompressionProxyDevRollout") ==
         kEnabled)) {
    return DATA_REDUCTION_DEV_HOST;
  }
#endif
  return std::string();
}

bool DataReductionProxyParams::AreDataReductionProxiesBypassed(
    const net::URLRequest& request, base::TimeDelta* min_retry_delay) const {
  if (request.context() != NULL &&
      request.context()->proxy_service() != NULL) {
    return AreProxiesBypassed(
        request.context()->proxy_service()->proxy_retry_info(),
        request.url().SchemeIs(url::kHttpsScheme),
        min_retry_delay);
  }

  return false;
}

bool DataReductionProxyParams::AreProxiesBypassed(
    const net::ProxyRetryInfoMap& retry_map,
    bool is_https,
    base::TimeDelta* min_retry_delay) const {
  if (retry_map.size() == 0)
    return false;

  if (is_https && alt_allowed_) {
    return ArePrimaryAndFallbackBypassed(
        retry_map, ssl_origin_, GURL(), min_retry_delay);
  }

  if (allowed_ && ArePrimaryAndFallbackBypassed(
      retry_map, origin_, fallback_origin_, min_retry_delay)) {
    return true;
  }

  if (alt_allowed_ && ArePrimaryAndFallbackBypassed(
      retry_map, alt_origin_, alt_fallback_origin_, min_retry_delay)) {
    return true;
  }

  return false;
}

bool DataReductionProxyParams::ArePrimaryAndFallbackBypassed(
    const net::ProxyRetryInfoMap& retry_map,
    const GURL& primary,
    const GURL& fallback,
    base::TimeDelta* min_retry_delay) const {
  net::ProxyRetryInfoMap::const_iterator found = retry_map.find(
      net::ProxyServer(primary.SchemeIs(url::kHttpsScheme) ?
          net::ProxyServer::SCHEME_HTTPS : net::ProxyServer::SCHEME_HTTP,
          net::HostPortPair::FromURL(primary)).ToURI());

  if (found == retry_map.end())
    return false;

  base::TimeDelta min_delay = found->second.current_delay;
  if (!fallback_allowed_ || !fallback.is_valid()) {
    if (min_retry_delay != NULL)
      *min_retry_delay = min_delay;
    return true;
  }

  found = retry_map.find(
      net::ProxyServer(fallback.SchemeIs(url::kHttpsScheme) ?
          net::ProxyServer::SCHEME_HTTPS : net::ProxyServer::SCHEME_HTTP,
          net::HostPortPair::FromURL(fallback)).ToURI());

  if (found == retry_map.end())
    return false;

  if (min_delay > found->second.current_delay)
    min_delay = found->second.current_delay;
  if (min_retry_delay != NULL)
    *min_retry_delay = min_delay;
  return true;
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

std::string DataReductionProxyParams::GetDefaultWarmupURL() const {
#if defined(DATA_REDUCTION_PROXY_WARMUP_URL)
  return DATA_REDUCTION_PROXY_WARMUP_URL;
#endif
  return std::string();
}

}  // namespace data_reduction_proxy
