// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/browser/data_reduction_proxy_params.h"

#include <vector>

#include "base/command_line.h"
#include "base/memory/scoped_ptr.h"
#include "base/metrics/field_trial.h"
#include "base/time/time.h"
#include "components/data_reduction_proxy/common/data_reduction_proxy_switches.h"
#include "net/base/host_port_pair.h"
#include "net/proxy/proxy_config.h"
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
const char kDefaultOrigin[] = "https://proxy.googlezip.net:443/";
const char kDevOrigin[] = "https://proxy-dev.googlezip.net:443/";
const char kDevFallbackOrigin[] = "http://proxy-dev.googlezip.net:80/";
const char kDefaultFallbackOrigin[] = "http://compress.googlezip.net:80/";
// This is for a proxy that supports HTTP CONNECT to tunnel SSL traffic.
// The proxy listens on port 443, but uses the HTTP protocol to set up
// the tunnel, not HTTPS.
const char kDefaultSslOrigin[] = "http://ssl.googlezip.net:443/";
const char kDefaultAltOrigin[] = "http://ssl.googlezip.net:80/";
const char kDefaultAltFallbackOrigin[] = "http://ssl.googlezip.net:80/";
const char kDefaultProbeUrl[] = "http://check.googlezip.net/connect";
const char kDefaultWarmupUrl[] = "http://www.gstatic.com/generate_204";

}  // namespace

namespace data_reduction_proxy {

// static
bool DataReductionProxyParams::IsIncludedInAlternativeFieldTrial() {
  const std::string group_name = base::FieldTrialList::FindFullName(
      "DataCompressionProxyAlternativeConfiguration");
  if (CommandLine::ForCurrentProcess()->HasSwitch(
          data_reduction_proxy::switches::kEnableDataReductionProxyAlt)) {
    return true;
  }
  return group_name == kEnabled;
}

// static
bool DataReductionProxyParams::IsIncludedInPromoFieldTrial() {
  return FieldTrialList::FindFullName(
      "DataCompressionProxyPromoVisibility") == kEnabled;
}

// static
bool DataReductionProxyParams::IsIncludedInPreconnectHintingFieldTrial() {
  return FieldTrialList::FindFullName(
          "DataCompressionProxyPreconnectHints") == kEnabled;
}

// static
bool DataReductionProxyParams::IsIncludedInCriticalPathBypassFieldTrial() {
  return FieldTrialList::FindFullName(
          "DataCompressionProxyCriticalBypass") == kEnabled;
}

// static
bool DataReductionProxyParams::IsIncludedInHoldbackFieldTrial() {
  return FieldTrialList::FindFullName(
      "DataCompressionProxyHoldback") == kEnabled;
}

// static
bool DataReductionProxyParams::
    IsIncludedInRemoveMissingViaHeaderOtherBypassFieldTrial() {
  return FieldTrialList::FindFullName(
      "DataReductionProxyRemoveMissingViaHeaderOtherBypass") == kEnabled;
}

DataReductionProxyTypeInfo::DataReductionProxyTypeInfo()
    : proxy_servers(),
      is_fallback(false),
      is_alternative(false),
      is_ssl(false) {
}

DataReductionProxyTypeInfo::~DataReductionProxyTypeInfo(){
}

DataReductionProxyParams::DataReductionProxyParams(int flags)
    : allowed_((flags & kAllowed) == kAllowed),
      fallback_allowed_((flags & kFallbackAllowed) == kFallbackAllowed),
      alt_allowed_((flags & kAlternativeAllowed) == kAlternativeAllowed),
      alt_fallback_allowed_(
          (flags & kAlternativeFallbackAllowed) == kAlternativeFallbackAllowed),
      promo_allowed_((flags & kPromoAllowed) == kPromoAllowed),
      holdback_((flags & kHoldback) == kHoldback),
      configured_on_command_line_(false) {
  bool result = Init(
      allowed_, fallback_allowed_, alt_allowed_, alt_fallback_allowed_);
  DCHECK(result);
}

scoped_ptr<DataReductionProxyParams> DataReductionProxyParams::Clone() {
  return scoped_ptr<DataReductionProxyParams>(
      new DataReductionProxyParams(*this));
}

DataReductionProxyParams::DataReductionProxyParams(
    const DataReductionProxyParams& other)
    : origin_(other.origin_),
      fallback_origin_(other.fallback_origin_),
      ssl_origin_(other.ssl_origin_),
      alt_origin_(other.alt_origin_),
      alt_fallback_origin_(other.alt_fallback_origin_),
      probe_url_(other.probe_url_),
      warmup_url_(other.warmup_url_),
      allowed_(other.allowed_),
      fallback_allowed_(other.fallback_allowed_),
      alt_allowed_(other.alt_allowed_),
      alt_fallback_allowed_(other.alt_fallback_allowed_),
      promo_allowed_(other.promo_allowed_),
      holdback_(other.holdback_),
      configured_on_command_line_(other.configured_on_command_line_) {
}

DataReductionProxyParams::~DataReductionProxyParams() {
}

DataReductionProxyParams::DataReductionProxyList
DataReductionProxyParams::GetAllowedProxies() const {
  DataReductionProxyList list;
  if (allowed_) {
    list.push_back(origin_);
  }
  if (allowed_ && fallback_allowed_)
    list.push_back(fallback_origin_);
  if (alt_allowed_) {
    list.push_back(alt_origin_);
    list.push_back(ssl_origin_);
  }
  if (alt_allowed_ && alt_fallback_allowed_)
    list.push_back(alt_fallback_origin_);
  return list;
}

DataReductionProxyParams::DataReductionProxyParams(int flags,
                                                   bool should_call_init)
    : allowed_((flags & kAllowed) == kAllowed),
      fallback_allowed_((flags & kFallbackAllowed) == kFallbackAllowed),
      alt_allowed_((flags & kAlternativeAllowed) == kAlternativeAllowed),
      alt_fallback_allowed_(
          (flags & kAlternativeFallbackAllowed) == kAlternativeFallbackAllowed),
      promo_allowed_((flags & kPromoAllowed) == kPromoAllowed),
      holdback_((flags & kHoldback) == kHoldback),
      configured_on_command_line_(false) {
  if (should_call_init) {
    bool result = Init(
        allowed_, fallback_allowed_, alt_allowed_, alt_fallback_allowed_);
    DCHECK(result);
  }
}

bool DataReductionProxyParams::Init(bool allowed,
                                    bool fallback_allowed,
                                    bool alt_allowed,
                                    bool alt_fallback_allowed) {
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

  if (alt_allowed && alt_fallback_allowed) {
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
  if (alt_fallback_allowed_ && !alt_allowed_) {
    DVLOG(1) << "The data reduction proxy alternative fallback cannot be "
        << "allowed if the alternative data reduction proxy is not allowed";
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
        alt_origin.empty()))
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
    fallback_origin = GetDefaultDevFallbackOrigin();
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
    DataReductionProxyTypeInfo* proxy_info) const {
  DCHECK(request);
  return IsDataReductionProxy(request->proxy_server(), proxy_info);
}

bool DataReductionProxyParams::IsDataReductionProxy(
    const net::HostPortPair& host_port_pair,
    DataReductionProxyTypeInfo* proxy_info) const {
  if (net::HostPortPair::FromURL(origin()).Equals(host_port_pair)) {
    if (proxy_info) {
      proxy_info->proxy_servers.first = origin();
      if (fallback_allowed())
        proxy_info->proxy_servers.second = fallback_origin();
    }
    return true;
  }

  if (fallback_allowed() &&
      net::HostPortPair::FromURL(fallback_origin()).Equals(host_port_pair)) {
    if (proxy_info) {
      proxy_info->proxy_servers.first = fallback_origin();
      proxy_info->proxy_servers.second = GURL();
      proxy_info->is_fallback = true;
    }
    return true;
  }
  if (net::HostPortPair::FromURL(alt_origin()).Equals(host_port_pair)) {
    if (proxy_info) {
      proxy_info->proxy_servers.first = alt_origin();
      proxy_info->is_alternative = true;
      if (alternative_fallback_allowed())
        proxy_info->proxy_servers.second = alt_fallback_origin();
    }
    return true;
  }
  if (alternative_fallback_allowed() &&
      net::HostPortPair::FromURL(alt_fallback_origin()).Equals(
      host_port_pair)) {
    if (proxy_info) {
      proxy_info->proxy_servers.first = alt_fallback_origin();
      proxy_info->proxy_servers.second = GURL();
      proxy_info->is_fallback = true;
      proxy_info->is_alternative = true;
    }
    return true;
  }
  if (net::HostPortPair::FromURL(ssl_origin()).Equals(host_port_pair)) {
    if (proxy_info) {
      proxy_info->proxy_servers.first = ssl_origin();
      proxy_info->proxy_servers.second = GURL();
      proxy_info->is_ssl = true;
    }
    return true;
  }
  return false;
}

bool DataReductionProxyParams::IsBypassedByDataReductionProxyLocalRules(
    const net::URLRequest& request,
    const net::ProxyConfig& data_reduction_proxy_config) const {
  DCHECK(request.context());
  DCHECK(request.context()->proxy_service());
  net::ProxyInfo result;
  data_reduction_proxy_config.proxy_rules().Apply(
      request.url(), &result);
  if (!result.proxy_server().is_valid())
    return true;
  if (result.proxy_server().is_direct())
    return true;
  return !IsDataReductionProxy(result.proxy_server().host_port_pair(), NULL);
}

std::string DataReductionProxyParams::GetDefaultDevOrigin() const {
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(switches::kDisableDataReductionProxyDev))
    return std::string();
  if (command_line.HasSwitch(switches::kEnableDataReductionProxyDev) ||
      (FieldTrialList::FindFullName("DataCompressionProxyDevRollout") ==
         kEnabled)) {
    return kDevOrigin;
  }
  return std::string();
}

std::string DataReductionProxyParams::GetDefaultDevFallbackOrigin() const {
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(switches::kDisableDataReductionProxyDev))
    return std::string();
  if (command_line.HasSwitch(switches::kEnableDataReductionProxyDev) ||
      (FieldTrialList::FindFullName("DataCompressionProxyDevRollout") ==
           kEnabled)) {
    return kDevFallbackOrigin;
  }
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

  // If the request is https, consider only the ssl proxy.
  if (is_https) {
    if (alt_allowed_) {
      return ArePrimaryAndFallbackBypassed(
          retry_map, ssl_origin_, GURL(), min_retry_delay);
    }
    NOTREACHED();
    return false;
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
  net::ProxyRetryInfoMap::const_iterator found = retry_map.end();
  if (min_retry_delay)
    *min_retry_delay = base::TimeDelta::Max();

  // Look for the primary proxy in the retry map. This must be done before
  // looking for the fallback in order to assign |min_retry_delay| if the
  // primary proxy has a shorter delay.
  if (!fallback_allowed_ || !fallback.is_valid() || min_retry_delay) {
    found = retry_map.find(
        net::ProxyServer(primary.SchemeIs(url::kHttpsScheme) ?
            net::ProxyServer::SCHEME_HTTPS :
            net::ProxyServer::SCHEME_HTTP,
        net::HostPortPair::FromURL(primary)).ToURI());
    if (found != retry_map.end() && min_retry_delay) {
      *min_retry_delay = found->second.current_delay;
    }
  }

  if (fallback_allowed_ && fallback.is_valid()) {
    // If fallback is allowed, only the fallback proxy needs to be on the retry
    // map to know if there was a bypass. We can reset found and forget if the
    // primary was on the retry map.
    found = retry_map.find(
        net::ProxyServer(fallback.SchemeIs(url::kHttpsScheme) ?
                             net::ProxyServer::SCHEME_HTTPS :
                             net::ProxyServer::SCHEME_HTTP,
                         net::HostPortPair::FromURL(fallback)).ToURI());
    if (found != retry_map.end() &&
        min_retry_delay &&
        *min_retry_delay > found->second.current_delay) {
      *min_retry_delay = found->second.current_delay;
    }
  }

  return found != retry_map.end();
}

// TODO(kundaji): Remove tests for macro definitions.
std::string DataReductionProxyParams::GetDefaultOrigin() const {
  return kDefaultOrigin;
}

std::string DataReductionProxyParams::GetDefaultFallbackOrigin() const {
  return kDefaultFallbackOrigin;
}

std::string DataReductionProxyParams::GetDefaultSSLOrigin() const {
  return kDefaultSslOrigin;
}

std::string DataReductionProxyParams::GetDefaultAltOrigin() const {
  return kDefaultAltOrigin;
}

std::string DataReductionProxyParams::GetDefaultAltFallbackOrigin() const {
  return kDefaultAltFallbackOrigin;
}

std::string DataReductionProxyParams::GetDefaultProbeURL() const {
  return kDefaultProbeUrl;
}

std::string DataReductionProxyParams::GetDefaultWarmupURL() const {
  return kDefaultWarmupUrl;
}

}  // namespace data_reduction_proxy
