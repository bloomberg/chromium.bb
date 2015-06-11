// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params.h"

#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/memory/scoped_ptr.h"
#include "base/metrics/field_trial.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/values.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_client_config_parser.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_switches.h"
#include "components/variations/variations_associated_data.h"
#include "net/base/host_port_pair.h"
#include "net/proxy/proxy_server.h"
#include "url/url_constants.h"

using base::FieldTrialList;

namespace {

const char kEnabled[] = "Enabled";
const char kDefaultSpdyOrigin[] = "https://proxy.googlezip.net:443";
const char kDefaultQuicOrigin[] = "quic://proxy.googlezip.net:443";
// A one-off change, until the Data Reduction Proxy configuration service is
// available.
const char kCarrierTestOrigin[] =
    "http://o-o.preferred.nttdocomodcp-hnd1.proxy-dev.googlezip.net:80";
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

const char kLoFiFieldTrial[] = "DataReductionProxyLoFi";

const char kConfigServiceFieldTrial[] = "DataReductionProxyConfigService";
const char kConfigServiceURLParam[] = "url";

// Default URL for retrieving the Data Reduction Proxy configuration.
const char kClientConfigURL[] = "";

const char kConfigScheme[] = "scheme";
const char kConfigHost[] = "host";
const char kConfigPort[] = "port";

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
std::string DataReductionProxyParams::GetLoFiFieldTrialName() {
  return kLoFiFieldTrial;
}

// static
bool DataReductionProxyParams::IsLoFiAlwaysOnViaFlags() {
  const std::string& lo_fi_value =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          data_reduction_proxy::switches::kDataReductionProxyLoFi);
  return lo_fi_value ==
         data_reduction_proxy::switches::kDataReductionProxyLoFiValueAlwaysOn;
}

// static
bool DataReductionProxyParams::IsLoFiCellularOnlyViaFlags() {
  const std::string& lo_fi_value =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          data_reduction_proxy::switches::kDataReductionProxyLoFi);
  return lo_fi_value == data_reduction_proxy::switches::
                            kDataReductionProxyLoFiValueCellularOnly;
}

// static
bool DataReductionProxyParams::IsLoFiDisabledViaFlags() {
  const std::string& lo_fi_value =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          data_reduction_proxy::switches::kDataReductionProxyLoFi);
  return lo_fi_value ==
         data_reduction_proxy::switches::kDataReductionProxyLoFiValueDisabled;
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

// static
bool DataReductionProxyParams::IsIncludedInUseDataSaverOnVPNFieldTrial() {
  return FieldTrialList::FindFullName("DataReductionProxyUseDataSaverOnVPN") ==
         kEnabled;
}

// static
bool DataReductionProxyParams::IsConfigClientEnabled() {
  std::string group_value =
      base::FieldTrialList::FindFullName(kConfigServiceFieldTrial);
  base::StringPiece group = group_value;
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
             data_reduction_proxy::switches::
                 kEnableDataReductionProxyConfigClient) ||
         group.starts_with(kEnabled);
}

// static
GURL DataReductionProxyParams::GetConfigServiceURL() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  std::string url;
  if (command_line->HasSwitch(switches::kDataReductionProxyConfigURL)) {
    url = command_line->GetSwitchValueASCII(
        switches::kDataReductionProxyConfigURL);
  }

  if (url.empty()) {
    url = variations::GetVariationParamValue(kConfigServiceFieldTrial,
                                             kConfigServiceURLParam);
  }

  if (url.empty())
    return GURL(kClientConfigURL);

  GURL result(url);
  if (result.is_valid())
    return result;

  LOG(WARNING) << "The following client config URL specified at the "
               << "command-line or variation is invalid: " << url;
  return GURL(kClientConfigURL);
}

// static
bool DataReductionProxyParams::ShouldForceEnableDataReductionProxy() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      data_reduction_proxy::switches::kEnableDataReductionProxy);
}

// static
bool DataReductionProxyParams::ShouldUseSecureProxyByDefault() {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          data_reduction_proxy::switches::
              kDataReductionProxyStartSecureDisabled))
    return false;

  if (FieldTrialList::FindFullName("DataReductionProxySecureProxyAfterCheck") ==
      kEnabled)
    return false;

  return true;
}

// static
int DataReductionProxyParams::GetFieldTrialParameterAsInteger(
    const std::string& group,
    const std::string& param_name,
    int default_value,
    int min_value) {
  DCHECK(default_value >= min_value);
  std::string param_value =
      variations::GetVariationParamValue(group, param_name);
  int value;
  if (param_value.empty() || !base::StringToInt(param_value, &value) ||
      value < min_value) {
    return default_value;
  }

  return value;
}

void DataReductionProxyParams::EnableQuic(bool enable) {
  quic_enabled_ = enable;
  DCHECK(!quic_enabled_ || IsIncludedInQuicFieldTrial());
  if (override_quic_origin_.empty() && quic_enabled_) {
    origin_ = net::ProxyServer::FromURI(kDefaultQuicOrigin,
                                        net::ProxyServer::SCHEME_HTTP);
    proxies_for_http_.clear();
    if (origin_.is_valid())
      proxies_for_http_.push_back(origin_);
    if (fallback_allowed_ && fallback_origin_.is_valid())
      proxies_for_http_.push_back(fallback_origin_);
  }
}

DataReductionProxyTypeInfo::DataReductionProxyTypeInfo()
    : is_fallback(false), is_alternative(false), is_ssl(false) {
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
  if (origin_.is_valid())
    proxies_for_http_.push_back(origin_);
  if (fallback_allowed_ && fallback_origin_.is_valid())
    proxies_for_http_.push_back(fallback_origin_);
  if (alt_allowed_ && alt_origin_.is_valid())
    alt_proxies_for_http_.push_back(alt_origin_);
  if (alt_fallback_allowed_ && alt_fallback_origin_.is_valid())
    alt_proxies_for_http_.push_back(alt_fallback_origin_);
  if (alt_allowed_ && ssl_origin_.is_valid())
    alt_proxies_for_https_.push_back(ssl_origin_);

  secure_proxy_check_url_ = GURL(secure_proxy_check_url);
  warmup_url_ = GURL(warmup_url);
}

bool DataReductionProxyParams::UsingHTTPTunnel(
    const net::HostPortPair& proxy_server) const {
  return ssl_origin_.is_valid() &&
         ssl_origin_.host_port_pair().Equals(proxy_server);
}

const std::vector<net::ProxyServer>& DataReductionProxyParams::proxies_for_http(
    bool use_alternative_configuration) const {
  return use_alternative_configuration ? alt_proxies_for_http_
                                       : proxies_for_http_;
}

const std::vector<net::ProxyServer>&
DataReductionProxyParams::proxies_for_https(
    bool use_alternative_configuration) const {
  return use_alternative_configuration ? alt_proxies_for_https_
                                       : proxies_for_https_;
}

void DataReductionProxyParams::PopulateConfigResponse(
    base::DictionaryValue* response) const {
  scoped_ptr<base::Value> proxy_config(new base::DictionaryValue());
  if (!holdback_) {
    base::DictionaryValue* proxy_config_dict = nullptr;
    if (!proxy_config->GetAsDictionary(&proxy_config_dict))
      return;

    scoped_ptr<base::Value> proxy_servers(new base::ListValue());
    base::ListValue* proxy_servers_list = nullptr;
    if (!proxy_servers->GetAsList(&proxy_servers_list))
      return;

    proxy_servers->GetAsList(&proxy_servers_list);
    scoped_ptr<base::DictionaryValue> server(new base::DictionaryValue());

    server->SetString(kConfigScheme,
                      config_parser::GetSchemeString(origin_.scheme()));
    server->SetString(kConfigHost, origin_.host_port_pair().host());
    server->SetInteger(kConfigPort, origin_.host_port_pair().port());
    proxy_servers_list->Append(server.release());
    server.reset(new base::DictionaryValue());

    server->SetString(kConfigScheme, config_parser::GetSchemeString(
                                         fallback_origin_.scheme()));
    server->SetString(kConfigHost, fallback_origin_.host_port_pair().host());
    server->SetInteger(kConfigPort, fallback_origin_.host_port_pair().port());
    proxy_servers_list->Append(server.release());

    proxy_config_dict->Set("httpProxyServers", proxy_servers.Pass());
  }

  response->Set("proxyConfig", proxy_config.Pass());
}

// Returns the URL to check to decide if the secure proxy origin should be
// used.
const GURL& DataReductionProxyParams::secure_proxy_check_url() const {
  return secure_proxy_check_url_;
}

// Returns true if the data reduction proxy configuration may be used.
bool DataReductionProxyParams::allowed() const {
  return allowed_;
}

// Returns true if the fallback proxy may be used.
bool DataReductionProxyParams::fallback_allowed() const {
  return fallback_allowed_;
}

// Returns true if the alternative data reduction proxy configuration may be
// used.
bool DataReductionProxyParams::alternative_allowed() const {
  return alt_allowed_;
}

// Returns true if the alternative fallback data reduction proxy
// configuration may be used.
bool DataReductionProxyParams::alternative_fallback_allowed() const {
  return alt_fallback_allowed_;
}

// Returns true if the data reduction proxy promo may be shown.
// This is idependent of whether the data reduction proxy is allowed.
// TODO(bengr): maybe tie to whether proxy is allowed.
bool DataReductionProxyParams::promo_allowed() const {
  return promo_allowed_;
}

// Returns true if the data reduction proxy should not actually use the
// proxy if enabled.
bool DataReductionProxyParams::holdback() const {
  return holdback_;
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

// TODO(kundaji): Remove tests for macro definitions.
std::string DataReductionProxyParams::GetDefaultOrigin() const {
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(switches::kEnableDataReductionProxyCarrierTest))
    return kCarrierTestOrigin;
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
