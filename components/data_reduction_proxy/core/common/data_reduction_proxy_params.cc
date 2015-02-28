// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params.h"

#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/memory/scoped_ptr.h"
#include "base/metrics/field_trial.h"
#include "base/strings/string_piece.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_switches.h"
#include "net/base/host_port_pair.h"
#include "net/proxy/proxy_config.h"
#include "net/proxy/proxy_info.h"
#include "net/proxy/proxy_list.h"
#include "net/proxy/proxy_retry_info.h"
#include "net/proxy/proxy_server.h"
#include "net/proxy/proxy_service.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "url/url_constants.h"

using base::FieldTrialList;

namespace {

const char kEnabled[] = "Enabled";
const char kDefaultSpdyOrigin[] = "https://proxy.googlezip.net:443";
const char kDefaultQuicOrigin[] = "quic://proxy.googlezip.net:443";
const char kDevOrigin[] = "https://proxy-dev.googlezip.net:443";
const char kDevFallbackOrigin[] = "proxy-dev.googlezip.net:80";
const char kDefaultFallbackOrigin[] = "compress.googlezip.net:80";
// This is for a proxy that supports HTTP CONNECT to tunnel SSL traffic.
// The proxy listens on port 443, but uses the HTTP protocol to set up
// the tunnel, not HTTPS.
const char kDefaultSslOrigin[] = "ssl.googlezip.net:443";
const char kDefaultAltOrigin[] = "ssl.googlezip.net:80";
const char kDefaultAltFallbackOrigin[] = "ssl.googlezip.net:80";
const char kDefaultSecureProxyCheckUrl[] = "http://check.googlezip.net/connect";
const char kDefaultWarmupUrl[] = "http://www.gstatic.com/generate_204";

const char kAndroidOneIdentifier[] = "sprout";

const char kQuicFieldTrial[] = "DataReductionProxyUseQuic";
}  // namespace

namespace data_reduction_proxy {

// static
bool DataReductionProxyParams::IsIncludedInAlternativeFieldTrial() {
  const std::string group_name = base::FieldTrialList::FindFullName(
      "DataCompressionProxyAlternativeConfiguration");
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
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

// static
bool DataReductionProxyParams::
    IsIncludedInRelaxMissingViaHeaderOtherBypassFieldTrial() {
  return FieldTrialList::FindFullName(
      "DataReductionProxyRemoveMissingViaHeaderOtherBypass") == "Relaxed";
}

// static
bool DataReductionProxyParams::IsIncludedInAndroidOnePromoFieldTrial(
    const char* build_fingerprint) {
  base::StringPiece fingerprint(build_fingerprint);
  return (fingerprint.find(kAndroidOneIdentifier) != std::string::npos);
}

// static
bool DataReductionProxyParams::IsLoFiEnabled() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      data_reduction_proxy::switches::kEnableDataReductionProxyLoFi);

}

//static
bool DataReductionProxyParams::WarnIfNoDataReductionProxy() {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          data_reduction_proxy::switches::
          kEnableDataReductionProxyBypassWarning)) {
    return true;
  }
  return false;
}

// static
bool DataReductionProxyParams::CanProxyURLScheme(const GURL& url) {
  return url.SchemeIs(url::kHttpScheme);
}

// static
bool DataReductionProxyParams::IsIncludedInQuicFieldTrial() {
  return FieldTrialList::FindFullName(kQuicFieldTrial) == kEnabled;
}

// static
std::string DataReductionProxyParams::GetQuicFieldTrialName() {
  return kQuicFieldTrial;
}

void DataReductionProxyParams::EnableQuic(bool enable) {
  quic_enabled_ = enable;
  DCHECK(!quic_enabled_ || IsIncludedInQuicFieldTrial());
  if (override_quic_origin_.empty() && quic_enabled_)
    origin_ = net::ProxyServer::FromURI(kDefaultQuicOrigin,
                                        net::ProxyServer::SCHEME_HTTP);
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
      quic_enabled_(false),
      configured_on_command_line_(false) {
  bool result = Init(
      allowed_, fallback_allowed_, alt_allowed_, alt_fallback_allowed_);
  DCHECK(result);
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
      quic_enabled_(false),
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
      DVLOG(1) << "Invalid data reduction proxy origin: " << origin_.ToURI();
      return false;
    }
  }

  if (allowed && fallback_allowed) {
    if (!fallback_origin_.is_valid()) {
      DVLOG(1) << "Invalid data reduction proxy fallback origin: "
          << fallback_origin_.ToURI();
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
      DVLOG(1) << "Invalid alternative origin:" << alt_origin_.ToURI();
      return false;
    }
    if (!ssl_origin_.is_valid()) {
      DVLOG(1) << "Invalid ssl origin: " << ssl_origin_.ToURI();
      return false;
    }
  }

  if (alt_allowed && alt_fallback_allowed) {
    if (!alt_fallback_origin_.is_valid()) {
      DVLOG(1) << "Invalid alternative fallback origin:"
          << alt_fallback_origin_.ToURI();
      return false;
    }
  }

  if (allowed && !secure_proxy_check_url_.is_valid()) {
    DVLOG(1) << "Invalid secure proxy check url: <null>";
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
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
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

  std::string secure_proxy_check_url = command_line.GetSwitchValueASCII(
      switches::kDataReductionProxySecureProxyCheckURL);
  std::string warmup_url = command_line.GetSwitchValueASCII(
      switches::kDataReductionProxyWarmupURL);

  // Set from preprocessor constants those params that are not specified on the
  // command line.
  if (origin.empty())
    origin = GetDefaultDevOrigin();
  override_quic_origin_ = origin;
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
  if (secure_proxy_check_url.empty())
    secure_proxy_check_url = GetDefaultSecureProxyCheckURL();
  if (warmup_url.empty())
    warmup_url = GetDefaultWarmupURL();

  origin_ = net::ProxyServer::FromURI(origin, net::ProxyServer::SCHEME_HTTP);
  fallback_origin_ =
      net::ProxyServer::FromURI(fallback_origin, net::ProxyServer::SCHEME_HTTP);
  ssl_origin_ =
      net::ProxyServer::FromURI(ssl_origin, net::ProxyServer::SCHEME_HTTP);
  alt_origin_ =
      net::ProxyServer::FromURI(alt_origin, net::ProxyServer::SCHEME_HTTP);
  alt_fallback_origin_ =
      net::ProxyServer::FromURI(alt_fallback_origin,
                                net::ProxyServer::SCHEME_HTTP);
  secure_proxy_check_url_ = GURL(secure_proxy_check_url);
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
  if (allowed() && origin().is_valid() &&
      origin().host_port_pair().Equals(host_port_pair)) {
    if (proxy_info) {
      proxy_info->proxy_servers.first = origin();
      if (fallback_allowed())
        proxy_info->proxy_servers.second = fallback_origin();
    }
    return true;
  }

  if (fallback_allowed() && fallback_origin().is_valid() &&
      fallback_origin().host_port_pair().Equals(host_port_pair)) {
    if (proxy_info) {
      proxy_info->proxy_servers.first = fallback_origin();
      proxy_info->proxy_servers.second =
          net::ProxyServer::FromURI(std::string(),
                                    net::ProxyServer::SCHEME_HTTP);
      proxy_info->is_fallback = true;
    }
    return true;
  }
  if (alternative_allowed() && alt_origin().is_valid() &&
      alt_origin().host_port_pair().Equals(host_port_pair)) {
    if (proxy_info) {
      proxy_info->proxy_servers.first = alt_origin();
      proxy_info->is_alternative = true;
      if (alternative_fallback_allowed())
        proxy_info->proxy_servers.second = alt_fallback_origin();
    }
    return true;
  }
  if (alternative_fallback_allowed() && alt_fallback_origin().is_valid() &&
      alt_fallback_origin().host_port_pair().Equals(
      host_port_pair)) {
    if (proxy_info) {
      proxy_info->proxy_servers.first = alt_fallback_origin();
      proxy_info->proxy_servers.second =
          net::ProxyServer::FromURI(std::string(),
                                    net::ProxyServer::SCHEME_HTTP);
      proxy_info->is_fallback = true;
      proxy_info->is_alternative = true;
    }
    return true;
  }
  if (ssl_origin().is_valid() &&
      ssl_origin().host_port_pair().Equals(host_port_pair)) {
    if (proxy_info) {
      proxy_info->proxy_servers.first = ssl_origin();
      proxy_info->proxy_servers.second =
          net::ProxyServer::FromURI(std::string(),
                                    net::ProxyServer::SCHEME_HTTP);
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
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
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
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
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
    const net::URLRequest& request,
    const net::ProxyConfig& data_reduction_proxy_config,
    base::TimeDelta* min_retry_delay) const {
  if (request.context() != NULL &&
      request.context()->proxy_service() != NULL) {
    return AreProxiesBypassed(
        request.context()->proxy_service()->proxy_retry_info(),
        data_reduction_proxy_config.proxy_rules(),
        request.url().SchemeIs(url::kHttpsScheme),
        min_retry_delay);
  }

  return false;
}

bool DataReductionProxyParams::AreProxiesBypassed(
    const net::ProxyRetryInfoMap& retry_map,
    const net::ProxyConfig::ProxyRules& proxy_rules,
    bool is_https,
    base::TimeDelta* min_retry_delay) const {
  // Data reduction proxy config is TYPE_PROXY_PER_SCHEME.
  if (proxy_rules.type != net::ProxyConfig::ProxyRules::TYPE_PROXY_PER_SCHEME)
    return false;

  const net::ProxyList* proxies = is_https ?
      proxy_rules.MapUrlSchemeToProxyList(url::kHttpsScheme) :
      proxy_rules.MapUrlSchemeToProxyList(url::kHttpScheme);

  if (!proxies)
    return false;

  scoped_ptr<base::ListValue> proxy_list =
      scoped_ptr<base::ListValue>(proxies->ToValue());

  base::TimeDelta min_delay = base::TimeDelta::Max();
  base::TimeDelta delay;
  bool bypassed = false;
  std::string proxy;
  net::HostPortPair host_port_pair;

  for (size_t i = 0; i < proxy_list->GetSize(); ++i) {
    proxy_list->GetString(i, &proxy);
    host_port_pair =  net::HostPortPair::FromString(std::string());
    net::ProxyServer proxy_server =
        net::ProxyServer::FromURI(proxy, net::ProxyServer::SCHEME_HTTP);
    if (proxy_server.is_valid() && !proxy_server.is_direct())
      host_port_pair = proxy_server.host_port_pair();

    if (IsDataReductionProxy(host_port_pair, NULL)) {
      if (!IsProxyBypassed(
          retry_map,
          net::ProxyServer::FromURI(proxy, net::ProxyServer::SCHEME_HTTP),
          &delay))
        return false;
      if (delay < min_delay)
        min_delay = delay;
      bypassed = true;
    }
  }

  if (min_retry_delay && bypassed)
    *min_retry_delay = min_delay;

  return bypassed;
}

bool DataReductionProxyParams::IsProxyBypassed(
    const net::ProxyRetryInfoMap& retry_map,
    const net::ProxyServer& proxy_server,
    base::TimeDelta* retry_delay) const {
  net::ProxyRetryInfoMap::const_iterator found =
      retry_map.find(proxy_server.ToURI());

  if (found == retry_map.end() ||
      found->second.bad_until < base::TimeTicks::Now()) {
    return false;
  }

  if (retry_delay)
     *retry_delay = found->second.current_delay;

  return true;
}

// TODO(kundaji): Remove tests for macro definitions.
std::string DataReductionProxyParams::GetDefaultOrigin() const {
  return quic_enabled_ ?
      kDefaultQuicOrigin : kDefaultSpdyOrigin;
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

std::string DataReductionProxyParams::GetDefaultSecureProxyCheckURL() const {
  return kDefaultSecureProxyCheckUrl;
}

std::string DataReductionProxyParams::GetDefaultWarmupURL() const {
  return kDefaultWarmupUrl;
}

}  // namespace data_reduction_proxy
