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
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_switches.h"
#include "components/variations/variations_associated_data.h"
#include "net/base/host_port_pair.h"
#include "net/proxy/proxy_server.h"
#include "url/url_constants.h"

using base::FieldTrialList;

namespace {

const char kEnabled[] = "Enabled";
const char kControl[] = "Control";
const char kPreview[] = "Enabled_Preview";
const char kDefaultSpdyOrigin[] = "https://proxy.googlezip.net:443";
const char kDefaultQuicOrigin[] = "quic://proxy.googlezip.net:443";
// A one-off change, until the Data Reduction Proxy configuration service is
// available.
const char kCarrierTestOrigin[] =
    "http://o-o.preferred.nttdocomodcp-hnd1.proxy-dev.googlezip.net:80";
const char kDevOrigin[] = "https://proxy-dev.googlezip.net:443";
const char kDevFallbackOrigin[] = "proxy-dev.googlezip.net:80";
const char kDefaultFallbackOrigin[] = "compress.googlezip.net:80";
const char kDefaultSecureProxyCheckUrl[] = "http://check.googlezip.net/connect";
const char kDefaultWarmupUrl[] = "http://www.gstatic.com/generate_204";

const char kAndroidOneIdentifier[] = "sprout";

const char kQuicFieldTrial[] = "DataReductionProxyUseQuic";
const char kDevRolloutFieldTrial[] = "DataCompressionProxyDevRollout";

const char kLoFiFieldTrial[] = "DataCompressionProxyLoFi";
const char kLoFiFlagFieldTrial[] = "DataCompressionProxyLoFiFlag";

const char kConfigServiceFieldTrial[] = "DataReductionProxyConfigService";
const char kConfigServiceURLParam[] = "url";

const char kTrustedSpdyProxyFieldTrialName[] = "DataReductionTrustedSpdyProxy";

// Default URL for retrieving the Data Reduction Proxy configuration.
const char kClientConfigURL[] =
    "https://datasaver.googleapis.com/v1/clientConfigs";

// The name of the server side experiment field trial.
const char kServerExperimentsFieldTrial[] =
    "DataReductionProxyServerExperiments";

}  // namespace

namespace data_reduction_proxy {
namespace params {

bool IsIncludedInPromoFieldTrial() {
  return FieldTrialList::FindFullName(
      "DataCompressionProxyPromoVisibility") == kEnabled;
}

bool IsIncludedInHoldbackFieldTrial() {
  return FieldTrialList::FindFullName(
      "DataCompressionProxyHoldback") == kEnabled;
}

bool IsIncludedInAndroidOnePromoFieldTrial(const char* build_fingerprint) {
  base::StringPiece fingerprint(build_fingerprint);
  return (fingerprint.find(kAndroidOneIdentifier) != std::string::npos);
}

std::string GetTrustedSpdyProxyFieldTrialName() {
  return kTrustedSpdyProxyFieldTrialName;
}

bool IsIncludedInTrustedSpdyProxyFieldTrial() {
  return base::FieldTrialList::FindFullName(GetTrustedSpdyProxyFieldTrialName())
             .find(kEnabled) == 0;
}

std::string GetLoFiFieldTrialName() {
  return kLoFiFieldTrial;
}

std::string GetLoFiFlagFieldTrialName() {
  return kLoFiFlagFieldTrial;
}

bool IsIncludedInLoFiEnabledFieldTrial() {
  return FieldTrialList::FindFullName(GetLoFiFieldTrialName()).find(kEnabled) ==
         0;
}

bool IsIncludedInLoFiControlFieldTrial() {
  return FieldTrialList::FindFullName(GetLoFiFieldTrialName()) == kControl;
}

bool IsIncludedInLoFiPreviewFieldTrial() {
  return FieldTrialList::FindFullName(GetLoFiFieldTrialName()) == kPreview;
}

bool IsIncludedInTamperDetectionExperiment() {
  return FieldTrialList::FindFullName("DataReductionProxyServerExperiments")
             .find("TamperDetection_Enabled") == 0;
}

bool IsLoFiOnViaFlags() {
  return IsLoFiAlwaysOnViaFlags() || IsLoFiCellularOnlyViaFlags() ||
         IsLoFiSlowConnectionsOnlyViaFlags();
}

bool IsLoFiAlwaysOnViaFlags() {
  const std::string& lo_fi_value =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          data_reduction_proxy::switches::kDataReductionProxyLoFi);
  return lo_fi_value ==
         data_reduction_proxy::switches::kDataReductionProxyLoFiValueAlwaysOn;
}

bool IsLoFiCellularOnlyViaFlags() {
  const std::string& lo_fi_value =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          data_reduction_proxy::switches::kDataReductionProxyLoFi);
  return lo_fi_value == data_reduction_proxy::switches::
                            kDataReductionProxyLoFiValueCellularOnly;
}

bool IsLoFiSlowConnectionsOnlyViaFlags() {
  const std::string& lo_fi_value =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          data_reduction_proxy::switches::kDataReductionProxyLoFi);
  return lo_fi_value == data_reduction_proxy::switches::
                            kDataReductionProxyLoFiValueSlowConnectionsOnly;
}

bool IsLoFiDisabledViaFlags() {
  const std::string& lo_fi_value =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          data_reduction_proxy::switches::kDataReductionProxyLoFi);
  return lo_fi_value ==
         data_reduction_proxy::switches::kDataReductionProxyLoFiValueDisabled;
}

bool AreLoFiPreviewsEnabledViaFlags() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      data_reduction_proxy::switches::kEnableDataReductionProxyLoFiPreview);
}

bool WarnIfNoDataReductionProxy() {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          data_reduction_proxy::switches::
          kEnableDataReductionProxyBypassWarning)) {
    return true;
  }
  return false;
}

bool IsIncludedInQuicFieldTrial() {
  return FieldTrialList::FindFullName(kQuicFieldTrial).find(kEnabled) == 0;
}

std::string GetQuicFieldTrialName() {
  return kQuicFieldTrial;
}

bool IsDevRolloutEnabled() {
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(switches::kDisableDataReductionProxyDev))
    return false;

  return command_line.HasSwitch(switches::kEnableDataReductionProxyDev) ||
         (FieldTrialList::FindFullName(kDevRolloutFieldTrial) == kEnabled);
}

std::string GetClientConfigFieldTrialName() {
  return kConfigServiceFieldTrial;
}

bool IsConfigClientEnabled() {
  std::string group_value =
      base::FieldTrialList::FindFullName(kConfigServiceFieldTrial);
  base::StringPiece group = group_value;
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
             data_reduction_proxy::switches::
                 kEnableDataReductionProxyConfigClient) ||
         group.starts_with(kEnabled);
}

GURL GetConfigServiceURL() {
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

bool ShouldForceEnableDataReductionProxy() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      data_reduction_proxy::switches::kEnableDataReductionProxy);
}

bool ShouldUseSecureProxyByDefault() {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          data_reduction_proxy::switches::
              kDataReductionProxyStartSecureDisabled))
    return false;

  if (FieldTrialList::FindFullName("DataReductionProxySecureProxyAfterCheck") ==
      kEnabled)
    return false;

  return true;
}

int GetFieldTrialParameterAsInteger(const std::string& group,
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

bool GetOverrideProxiesForHttpFromCommandLine(
    std::vector<net::ProxyServer>* override_proxies_for_http) {
  DCHECK(override_proxies_for_http);
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDataReductionProxyHttpProxies)) {
    return false;
  }

  override_proxies_for_http->clear();

  std::string proxy_overrides =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kDataReductionProxyHttpProxies);
  std::vector<std::string> proxy_override_values = base::SplitString(
      proxy_overrides, ";", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  for (const std::string& proxy_override : proxy_override_values) {
    override_proxies_for_http->push_back(net::ProxyServer::FromURI(
        proxy_override, net::ProxyServer::SCHEME_HTTP));
  }

  return true;
}

std::string GetServerExperimentsFieldTrialName() {
  return kServerExperimentsFieldTrial;
}

}  // namespace params

void DataReductionProxyParams::EnableQuic(bool enable) {
  quic_enabled_ = enable;
  DCHECK(!quic_enabled_ || params::IsIncludedInQuicFieldTrial());
  if (!params::IsDevRolloutEnabled() && override_quic_origin_.empty() &&
      quic_enabled_) {
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
    : is_fallback(false), is_ssl(false) {
}

DataReductionProxyTypeInfo::~DataReductionProxyTypeInfo(){
}

DataReductionProxyParams::DataReductionProxyParams(int flags)
    : DataReductionProxyParams(flags, true) {}

DataReductionProxyParams::~DataReductionProxyParams() {
}

DataReductionProxyParams::DataReductionProxyParams(int flags,
                                                   bool should_call_init)
    : allowed_((flags & kAllowed) == kAllowed),
      fallback_allowed_((flags & kFallbackAllowed) == kFallbackAllowed),
      promo_allowed_((flags & kPromoAllowed) == kPromoAllowed),
      holdback_((flags & kHoldback) == kHoldback),
      quic_enabled_(false),
      configured_on_command_line_(false),
      use_override_proxies_for_http_(false) {
  if (should_call_init) {
    bool result = Init(allowed_, fallback_allowed_);
    DCHECK(result);
  }
}

bool DataReductionProxyParams::Init(bool allowed, bool fallback_allowed) {
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

  if (allowed && !secure_proxy_check_url_.is_valid()) {
    DVLOG(1) << "Invalid secure proxy check url: <null>";
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
  use_override_proxies_for_http_ =
      params::GetOverrideProxiesForHttpFromCommandLine(
          &override_proxies_for_http_);

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

  configured_on_command_line_ =
      !(origin.empty() && fallback_origin.empty() && ssl_origin.empty());

  // Configuring the proxy on the command line overrides the values of
  // |allowed_|.
  if (configured_on_command_line_)
    allowed_ = true;

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
  if (secure_proxy_check_url.empty())
    secure_proxy_check_url = GetDefaultSecureProxyCheckURL();
  if (warmup_url.empty())
    warmup_url = GetDefaultWarmupURL();

  origin_ = net::ProxyServer::FromURI(origin, net::ProxyServer::SCHEME_HTTP);
  fallback_origin_ =
      net::ProxyServer::FromURI(fallback_origin, net::ProxyServer::SCHEME_HTTP);
  ssl_origin_ =
      net::ProxyServer::FromURI(ssl_origin, net::ProxyServer::SCHEME_HTTP);
  if (origin_.is_valid())
    proxies_for_http_.push_back(origin_);
  if (fallback_allowed_ && fallback_origin_.is_valid())
    proxies_for_http_.push_back(fallback_origin_);
  if (ssl_origin_.is_valid())
    proxies_for_https_.push_back(ssl_origin_);

  secure_proxy_check_url_ = GURL(secure_proxy_check_url);
  warmup_url_ = GURL(warmup_url);
}

bool DataReductionProxyParams::UsingHTTPTunnel(
    const net::HostPortPair& proxy_server) const {
  return ssl_origin_.is_valid() &&
         ssl_origin_.host_port_pair().Equals(proxy_server);
}

const std::vector<net::ProxyServer>&
DataReductionProxyParams::proxies_for_http() const {
  if (use_override_proxies_for_http_)
    return override_proxies_for_http_;
  return proxies_for_http_;
}

const std::vector<net::ProxyServer>&
DataReductionProxyParams::proxies_for_https() const {
  return proxies_for_https_;
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
  return params::IsDevRolloutEnabled() ? kDevOrigin : std::string();
}

std::string DataReductionProxyParams::GetDefaultDevFallbackOrigin() const {
  return params::IsDevRolloutEnabled() ? kDevFallbackOrigin : std::string();
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
  return std::string();
}

std::string DataReductionProxyParams::GetDefaultSecureProxyCheckURL() const {
  return kDefaultSecureProxyCheckUrl;
}

std::string DataReductionProxyParams::GetDefaultWarmupURL() const {
  return kDefaultWarmupUrl;
}

}  // namespace data_reduction_proxy
