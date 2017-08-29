// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params.h"

#include <map>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/metrics/field_trial.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_features.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_server.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_switches.h"
#include "components/variations/variations_associated_data.h"
#include "net/proxy/proxy_server.h"
#include "url/url_constants.h"

#if defined(OS_ANDROID)
#include "base/android/build_info.h"
#include "base/sys_info.h"
#endif

namespace {

const char kEnabled[] = "Enabled";
const char kControl[] = "Control";
const char kDisabled[] = "Disabled";
const char kDefaultSecureProxyCheckUrl[] = "http://check.googlezip.net/connect";
const char kDefaultWarmupUrl[] = "http://check.googlezip.net/generate_204";

const char kQuicFieldTrial[] = "DataReductionProxyUseQuic";

const char kLoFiFieldTrial[] = "DataCompressionProxyLoFi";
const char kLoFiFlagFieldTrial[] = "DataCompressionProxyLoFiFlag";

const char kBlackListTransitionFieldTrial[] =
    "DataReductionProxyPreviewsBlackListTransition";

// Default URL for retrieving the Data Reduction Proxy configuration.
const char kClientConfigURL[] =
    "https://datasaver.googleapis.com/v1/clientConfigs";

// Default URL for sending pageload metrics.
const char kPingbackURL[] =
    "https://datasaver.googleapis.com/v1/metrics:recordPageloadMetrics";

// The name of the server side experiment field trial.
const char kServerExperimentsFieldTrial[] =
    "DataReductionProxyServerExperiments";

// LitePage black list version.
const char kLitePageBlackListVersion[] = "lite-page-blacklist-version";

bool IsIncludedInFieldTrial(const std::string& name) {
  return base::StartsWith(base::FieldTrialList::FindFullName(name), kEnabled,
                          base::CompareCase::SENSITIVE);
}

// Returns the variation value for |parameter_name|. If the value is
// unavailable, |default_value| is returned.
std::string GetStringValueForVariationParamWithDefaultValue(
    const std::map<std::string, std::string>& variation_params,
    const std::string& parameter_name,
    const std::string& default_value) {
  const auto it = variation_params.find(parameter_name);
  if (it == variation_params.end())
    return default_value;
  return it->second;
}

bool IsIncludedInAndroidOnePromoFieldTrial(
    base::StringPiece build_fingerprint) {
  static const char kAndroidOneIdentifier[] = "sprout";
  return build_fingerprint.find(kAndroidOneIdentifier) != std::string::npos;
}

bool CanShowAndroidLowMemoryDevicePromo() {
#if defined(OS_ANDROID)
  return base::SysInfo::IsLowEndDevice() &&
         base::FeatureList::IsEnabled(
             data_reduction_proxy::features::
                 kDataReductionProxyLowMemoryDevicePromo);
#endif
  return false;
}

}  // namespace

namespace data_reduction_proxy {
namespace params {

bool IsIncludedInPromoFieldTrial() {
  if (IsIncludedInFieldTrial("DataCompressionProxyPromoVisibility"))
    return true;

#if defined(OS_ANDROID)
  base::StringPiece android_build_fingerprint =
      base::android::BuildInfo::GetInstance()->android_build_fp();
  if (IsIncludedInAndroidOnePromoFieldTrial(android_build_fingerprint))
    return true;
#endif
  return CanShowAndroidLowMemoryDevicePromo();
}

bool IsIncludedInFREPromoFieldTrial() {
  if (IsIncludedInFieldTrial("DataReductionProxyFREPromo"))
    return true;

#if defined(OS_ANDROID)
  base::StringPiece android_build_fingerprint =
      base::android::BuildInfo::GetInstance()->android_build_fp();
  if (IsIncludedInAndroidOnePromoFieldTrial(android_build_fingerprint))
    return true;
#endif
  return CanShowAndroidLowMemoryDevicePromo();
}

bool IsIncludedInAndroidOnePromoFieldTrialForTesting(
    base::StringPiece build_fingerprint) {
  return IsIncludedInAndroidOnePromoFieldTrial(build_fingerprint);
}

bool IsIncludedInHoldbackFieldTrial() {
  return IsIncludedInFieldTrial("DataCompressionProxyHoldback");
}

std::string HoldbackFieldTrialGroup() {
  return base::FieldTrialList::FindFullName("DataCompressionProxyHoldback");
}

const char* GetLoFiFieldTrialName() {
  return kLoFiFieldTrial;
}

const char* GetLoFiFlagFieldTrialName() {
  return kLoFiFlagFieldTrial;
}

bool IsIncludedInServerExperimentsFieldTrial() {
  return !base::CommandLine::ForCurrentProcess()->HasSwitch(
             data_reduction_proxy::switches::
                 kDataReductionProxyServerExperimentsDisabled) &&
         base::FieldTrialList::FindFullName(kServerExperimentsFieldTrial)
                 .find(kDisabled) != 0;
}
bool IsIncludedInTamperDetectionExperiment() {
  return IsIncludedInServerExperimentsFieldTrial() &&
         base::StartsWith(
             base::FieldTrialList::FindFullName(kServerExperimentsFieldTrial),
             "TamperDetection_Enabled", base::CompareCase::SENSITIVE);
}

bool FetchWarmupURLEnabled() {
  // Fetching of the warmup URL can be enabled only for Enabled* and Control*
  // groups.
  if (!IsIncludedInQuicFieldTrial() &&
      !base::StartsWith(base::FieldTrialList::FindFullName(kQuicFieldTrial),
                        kControl, base::CompareCase::SENSITIVE)) {
    return false;
  }

  std::map<std::string, std::string> params;
  variations::GetVariationParams(GetQuicFieldTrialName(), &params);
  return GetStringValueForVariationParamWithDefaultValue(
             params, "enable_warmup", "false") == "true";
}

GURL GetWarmupURL() {
  std::map<std::string, std::string> params;
  variations::GetVariationParams(GetQuicFieldTrialName(), &params);
  return GURL(GetStringValueForVariationParamWithDefaultValue(
      params, "warmup_url", kDefaultWarmupUrl));
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

bool AreLitePagesEnabledViaFlags() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      data_reduction_proxy::switches::kEnableDataReductionProxyLitePage);
}

bool IsForcePingbackEnabledViaFlags() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      data_reduction_proxy::switches::kEnableDataReductionProxyForcePingback);
}

bool WarnIfNoDataReductionProxy() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      data_reduction_proxy::switches::kEnableDataReductionProxyBypassWarning);
}

bool IsIncludedInQuicFieldTrial() {
  if (base::StartsWith(base::FieldTrialList::FindFullName(kQuicFieldTrial),
                       kControl, base::CompareCase::SENSITIVE)) {
    return false;
  }
  if (base::StartsWith(base::FieldTrialList::FindFullName(kQuicFieldTrial),
                       kDisabled, base::CompareCase::SENSITIVE)) {
    return false;
  }
  // QUIC is enabled by default.
  return true;
}

bool IsQuicEnabledForNonCoreProxies() {
  DCHECK(IsIncludedInQuicFieldTrial());
  std::map<std::string, std::string> params;
  variations::GetVariationParams(GetQuicFieldTrialName(), &params);
  return GetStringValueForVariationParamWithDefaultValue(
             params, "enable_quic_non_core_proxies", "false") == "true";
}

const char* GetQuicFieldTrialName() {
  return kQuicFieldTrial;
}

bool IsBrotliAcceptEncodingEnabled() {
  // Brotli encoding is enabled by default since the data reduction proxy server
  // controls when to serve Brotli encoded content. It can be disabled in
  // Chromium only if Chromium belongs to a field trial group whose name starts
  // with "Disabled".
  return !base::StartsWith(base::FieldTrialList::FindFullName(
                               "DataReductionProxyBrotliAcceptEncoding"),
                           kDisabled, base::CompareCase::SENSITIVE);
}

bool IsConfigClientEnabled() {
  // Config client is enabled by default. It can be disabled only if Chromium
  // belongs to a field trial group whose name starts with "Disabled".
  return !base::StartsWith(
      base::FieldTrialList::FindFullName("DataReductionProxyConfigService"),
      kDisabled, base::CompareCase::SENSITIVE);
}

bool IsBlackListEnabledForServerPreviews() {
  return base::StartsWith(
      base::FieldTrialList::FindFullName(kBlackListTransitionFieldTrial),
      kEnabled, base::CompareCase::SENSITIVE);
}

GURL GetConfigServiceURL() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  std::string url;
  if (command_line->HasSwitch(switches::kDataReductionProxyConfigURL)) {
    url = command_line->GetSwitchValueASCII(
        switches::kDataReductionProxyConfigURL);
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

GURL GetPingbackURL() {
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  std::string url;
  if (command_line->HasSwitch(switches::kDataReductionPingbackURL)) {
    url =
        command_line->GetSwitchValueASCII(switches::kDataReductionPingbackURL);
  }

  if (url.empty())
    return GURL(kPingbackURL);

  GURL result(url);
  if (result.is_valid())
    return result;

  LOG(WARNING) << "The following page load metrics URL specified at the "
               << "command-line or variation is invalid: " << url;
  return GURL(kPingbackURL);
}

bool ShouldForceEnableDataReductionProxy() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      data_reduction_proxy::switches::kEnableDataReductionProxy);
}

int LitePageVersion() {
  return GetFieldTrialParameterAsInteger(
      data_reduction_proxy::params::GetLoFiFieldTrialName(),
      kLitePageBlackListVersion, 0, 0);
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
    std::vector<DataReductionProxyServer>* override_proxies_for_http) {
  DCHECK(override_proxies_for_http);
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDataReductionProxyHttpProxies)) {
    // It is illegal to use |kDataReductionProxy| or
    // |kDataReductionProxyFallback| with |kDataReductionProxyHttpProxies|.
    DCHECK(base::CommandLine::ForCurrentProcess()
               ->GetSwitchValueASCII(switches::kDataReductionProxy)
               .empty());
    DCHECK(base::CommandLine::ForCurrentProcess()
               ->GetSwitchValueASCII(switches::kDataReductionProxyFallback)
               .empty());
    override_proxies_for_http->clear();

    std::string proxy_overrides =
        base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
            switches::kDataReductionProxyHttpProxies);
    std::vector<std::string> proxy_override_values = base::SplitString(
        proxy_overrides, ";", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
    for (const std::string& proxy_override : proxy_override_values) {
      // Overriding proxies have type UNSPECIFIED_TYPE.
      override_proxies_for_http->push_back(DataReductionProxyServer(
          net::ProxyServer::FromURI(proxy_override,
                                    net::ProxyServer::SCHEME_HTTP),
          ProxyServer::UNSPECIFIED_TYPE));
    }

    return true;
  }

  std::string origin =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kDataReductionProxy);
  std::string fallback_origin =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kDataReductionProxyFallback);

  if (origin.empty() && fallback_origin.empty())
    return false;

  override_proxies_for_http->clear();
  // Overriding proxies have type UNSPECIFIED_TYPE.
  if (!origin.empty()) {
    override_proxies_for_http->push_back(DataReductionProxyServer(
        net::ProxyServer::FromURI(origin, net::ProxyServer::SCHEME_HTTP),
        ProxyServer::UNSPECIFIED_TYPE));
  }
  if (!fallback_origin.empty()) {
    override_proxies_for_http->push_back(DataReductionProxyServer(
        net::ProxyServer::FromURI(fallback_origin,
                                  net::ProxyServer::SCHEME_HTTP),
        ProxyServer::UNSPECIFIED_TYPE));
  }

  return true;
}

const char* GetServerExperimentsFieldTrialName() {
  return kServerExperimentsFieldTrial;
}

GURL GetSecureProxyCheckURL() {
  std::string secure_proxy_check_url =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kDataReductionProxySecureProxyCheckURL);
  if (secure_proxy_check_url.empty())
    secure_proxy_check_url = kDefaultSecureProxyCheckUrl;

  return GURL(secure_proxy_check_url);
}

}  // namespace params

DataReductionProxyTypeInfo::DataReductionProxyTypeInfo() : proxy_index(0) {}

DataReductionProxyTypeInfo::DataReductionProxyTypeInfo(
    const DataReductionProxyTypeInfo& other) = default;

DataReductionProxyTypeInfo::~DataReductionProxyTypeInfo() {}

DataReductionProxyParams::DataReductionProxyParams() {
  bool use_override_proxies_for_http =
      params::GetOverrideProxiesForHttpFromCommandLine(&proxies_for_http_);

  if (!use_override_proxies_for_http) {
    DCHECK(proxies_for_http_.empty());
    proxies_for_http_.push_back(DataReductionProxyServer(
        net::ProxyServer::FromURI("https://proxy.googlezip.net:443",
                                  net::ProxyServer::SCHEME_HTTP),
        ProxyServer::CORE));
    proxies_for_http_.push_back(DataReductionProxyServer(
        net::ProxyServer::FromURI("compress.googlezip.net:80",
                                  net::ProxyServer::SCHEME_HTTP),
        ProxyServer::CORE));
  }
}

DataReductionProxyParams::~DataReductionProxyParams() {}

void DataReductionProxyParams::SetProxiesForHttpForTesting(
    const std::vector<DataReductionProxyServer>& proxies_for_http) {
  proxies_for_http_ = proxies_for_http;
}

const std::vector<DataReductionProxyServer>&
DataReductionProxyParams::proxies_for_http() const {
  return proxies_for_http_;
}

}  // namespace data_reduction_proxy
