// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/browser/data_reduction_proxy_settings.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram.h"
#include "base/prefs/pref_member.h"
#include "base/prefs/pref_service.h"
#include "base/prefs/scoped_user_pref_update.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "components/data_reduction_proxy/browser/data_reduction_proxy_configurator.h"
#include "components/data_reduction_proxy/common/data_reduction_proxy_pref_names.h"
#include "components/data_reduction_proxy/common/data_reduction_proxy_switches.h"
#include "crypto/random.h"
#include "net/base/auth.h"
#include "net/base/host_port_pair.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/http/http_auth.h"
#include "net/http/http_auth_cache.h"
#include "net/http/http_network_session.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_status.h"
#include "url/gurl.h"

using base::FieldTrialList;
using base::StringPrintf;

namespace {

// Key of the UMA DataReductionProxy.StartupState histogram.
const char kUMAProxyStartupStateHistogram[] =
    "DataReductionProxy.StartupState";

// Key of the UMA DataReductionProxy.ProbeURL histogram.
const char kUMAProxyProbeURL[] = "DataReductionProxy.ProbeURL";

const char kEnabled[] = "Enabled";

// TODO(marq): Factor this string out into a constant here and in
//             http_auth_handler_spdyproxy.
const char kAuthenticationRealmName[] = "SpdyProxy";

int64 GetInt64PrefValue(const base::ListValue& list_value, size_t index) {
  int64 val = 0;
  std::string pref_value;
  bool rv = list_value.GetString(index, &pref_value);
  DCHECK(rv);
  if (rv) {
    rv = base::StringToInt64(pref_value, &val);
    DCHECK(rv);
  }
  return val;
}

bool IsProxyOriginSetOnCommandLine() {
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  return command_line.HasSwitch(
      data_reduction_proxy::switches::kDataReductionProxy);
}

bool IsEnableSpdyProxyAuthSetOnCommandLine() {
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  return command_line.HasSwitch(
      data_reduction_proxy::switches::kEnableDataReductionProxy);
}

}  // namespace

namespace data_reduction_proxy {

DataReductionProxySettings::DataReductionProxySettings()
    : restricted_by_carrier_(false),
      enabled_by_user_(false),
      prefs_(NULL),
      local_state_prefs_(NULL),
      url_request_context_getter_(NULL) {
}

DataReductionProxySettings::~DataReductionProxySettings() {
  if (IsDataReductionProxyAllowed())
    spdy_proxy_auth_enabled_.Destroy();
}

void DataReductionProxySettings::InitPrefMembers() {
  DCHECK(thread_checker_.CalledOnValidThread());
  spdy_proxy_auth_enabled_.Init(
      prefs::kDataReductionProxyEnabled,
      GetOriginalProfilePrefs(),
      base::Bind(&DataReductionProxySettings::OnProxyEnabledPrefChange,
                 base::Unretained(this)));
}

void DataReductionProxySettings::InitDataReductionProxySettings(
    PrefService* prefs,
    PrefService* local_state_prefs,
    net::URLRequestContextGetter* url_request_context_getter,
    scoped_ptr<DataReductionProxyConfigurator> config) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(prefs);
  DCHECK(local_state_prefs);
  DCHECK(url_request_context_getter);
  DCHECK(config);
  prefs_ = prefs;
  local_state_prefs_ = local_state_prefs;
  url_request_context_getter_ = url_request_context_getter;
  config_ = config.Pass();
  InitPrefMembers();
  RecordDataReductionInit();

  // Disable the proxy if it is not allowed to be used.
  if (!IsDataReductionProxyAllowed())
    return;

  AddDefaultProxyBypassRules();
  net::NetworkChangeNotifier::AddIPAddressObserver(this);

  // We set or reset the proxy pref at startup.
  MaybeActivateDataReductionProxy(true);
}

// static
void DataReductionProxySettings::InitDataReductionProxySession(
    net::HttpNetworkSession* session) {
// This is a no-op unless the authentication parameters are compiled in.
// (even though values for them may be specified on the command line).
// Authentication will still work if the command line parameters are used,
// however there will be a round-trip overhead for each challenge/response
// (typically once per session).
// TODO(bengr):Pass a configuration struct into DataReductionProxyConfigurator's
// constructor. The struct would carry everything in the preprocessor flags.
#if defined(SPDY_PROXY_AUTH_ORIGIN) && defined(SPDY_PROXY_AUTH_VALUE)
  DCHECK(session);
  net::HttpAuthCache* auth_cache = session->http_auth_cache();
  DCHECK(auth_cache);
  InitDataReductionAuthentication(auth_cache);
#endif  // defined(SPDY_PROXY_AUTH_ORIGIN) && defined(SPDY_PROXY_AUTH_VALUE)
}

// static
void DataReductionProxySettings::InitDataReductionAuthentication(
    net::HttpAuthCache* auth_cache) {
  DCHECK(auth_cache);
  int64 timestamp =
      (base::Time::Now() - base::Time::UnixEpoch()).InMilliseconds() / 1000;

  DataReductionProxyList proxies = GetDataReductionProxies();
  for (DataReductionProxyList::iterator it = proxies.begin();
      it != proxies.end(); ++it) {
    GURL auth_origin = (*it).GetOrigin();
    int32 rand[3];
    crypto::RandBytes(rand, 3 * sizeof(rand[0]));

    std::string realm =
        base::StringPrintf("%s%lld", kAuthenticationRealmName,
                           static_cast<long long>(timestamp));
    std::string challenge = base::StringPrintf(
        "%s realm=\"%s\", ps=\"%lld-%u-%u-%u\"",
        kAuthenticationRealmName,
        realm.data(),
        static_cast<long long>(timestamp),
        rand[0],
        rand[1],
        rand[2]);
    base::string16 password = AuthHashForSalt(timestamp);

    DVLOG(1) << "origin: [" << auth_origin << "] realm: [" << realm
        << "] challenge: [" << challenge << "] password: [" << password << "]";

    net::AuthCredentials credentials(base::string16(), password);
    // |HttpAuthController| searches this cache by origin and path, the latter
    // being '/' in the case of the data reduction proxy.
    auth_cache->Add(auth_origin,
                    realm,
                    net::HttpAuth::AUTH_SCHEME_SPDYPROXY,
                    challenge,
                    credentials,
                    std::string("/"));
  }
}

// TODO(bengr): Use a configuration struct to carry field trial state as well.
// static
bool DataReductionProxySettings::IsDataReductionProxyAllowed() {
  return IsProxyOriginSetOnCommandLine() ||
      (FieldTrialList::FindFullName("DataCompressionProxyRollout") == kEnabled);
}

// static
bool DataReductionProxySettings::IsDataReductionProxyPromoAllowed() {
  return IsProxyOriginSetOnCommandLine() ||
      (IsDataReductionProxyAllowed() &&
        FieldTrialList::FindFullName("DataCompressionProxyPromoVisibility") ==
            kEnabled);
}

// static
bool DataReductionProxySettings::IsPreconnectHintingAllowed() {
  if (!IsDataReductionProxyAllowed())
    return false;
  return FieldTrialList::FindFullName("DataCompressionProxyPreconnectHints") ==
      kEnabled;
}

// static
std::string DataReductionProxySettings::GetDataReductionProxyOrigin() {
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(switches::kDataReductionProxyDev))
    return command_line.GetSwitchValueASCII(switches::kDataReductionProxyDev);
  if (command_line.HasSwitch(switches::kDataReductionProxy))
    return command_line.GetSwitchValueASCII(switches::kDataReductionProxy);
#if defined(DATA_REDUCTION_DEV_HOST)
  if (FieldTrialList::FindFullName("DataCompressionProxyDevRollout") ==
      kEnabled) {
    return DATA_REDUCTION_DEV_HOST;
  }
#endif
#if defined(SPDY_PROXY_AUTH_ORIGIN)
  return SPDY_PROXY_AUTH_ORIGIN;
#else
  return std::string();
#endif
}

// static
std::string DataReductionProxySettings::GetDataReductionProxyFallback() {
  // Regardless of what else is defined, only return a value if the main proxy
  // origin is defined.
  if (GetDataReductionProxyOrigin().empty())
    return std::string();
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(switches::kDataReductionProxyFallback)) {
    return command_line.GetSwitchValueASCII(
        switches::kDataReductionProxyFallback);
  }
#if defined(DATA_REDUCTION_FALLBACK_HOST)
  return DATA_REDUCTION_FALLBACK_HOST;
#else
  return std::string();
#endif
}

bool DataReductionProxySettings::IsAcceptableAuthChallenge(
    net::AuthChallengeInfo* auth_info) {
  // Challenge realm must start with the authentication realm name.
  std::string realm_prefix =
      auth_info->realm.substr(0, strlen(kAuthenticationRealmName));
  if (realm_prefix != kAuthenticationRealmName)
    return false;

  // The challenger must be one of the configured proxies.
  DataReductionProxyList proxies = GetDataReductionProxies();
  for (DataReductionProxyList::iterator it = proxies.begin();
       it != proxies.end(); ++it) {
    net::HostPortPair origin_host = net::HostPortPair::FromURL(*it);
    if (origin_host.Equals(auth_info->challenger))
      return true;
  }
  return false;
}

base::string16 DataReductionProxySettings::GetTokenForAuthChallenge(
    net::AuthChallengeInfo* auth_info) {
  if (auth_info->realm.length() > strlen(kAuthenticationRealmName)) {
    int64 salt;
    std::string realm_suffix =
        auth_info->realm.substr(strlen(kAuthenticationRealmName));
    if (base::StringToInt64(realm_suffix, &salt)) {
      return AuthHashForSalt(salt);
    } else {
      DVLOG(1) << "Unable to parse realm name " << auth_info->realm
               << "into an int for salting.";
      return base::string16();
    }
  } else {
    return base::string16();
  }
}

bool DataReductionProxySettings::IsDataReductionProxyEnabled() {
  return spdy_proxy_auth_enabled_.GetValue() ||
      IsEnableSpdyProxyAuthSetOnCommandLine();
}

bool DataReductionProxySettings::IsDataReductionProxyManaged() {
  return spdy_proxy_auth_enabled_.IsManaged();
}

// static
DataReductionProxySettings::DataReductionProxyList
DataReductionProxySettings::GetDataReductionProxies() {
  DataReductionProxyList proxies;
  std::string proxy = GetDataReductionProxyOrigin();
  std::string fallback = GetDataReductionProxyFallback();

  if (!proxy.empty())
    proxies.push_back(GURL(proxy));

  if (!fallback.empty()) {
    // Sanity check: fallback isn't the only proxy.
    DCHECK(!proxies.empty());
    proxies.push_back(GURL(fallback));
  }

  return proxies;
}

void DataReductionProxySettings::SetDataReductionProxyEnabled(bool enabled) {
  DCHECK(thread_checker_.CalledOnValidThread());
  // Prevent configuring the proxy when it is not allowed to be used.
  if (!IsDataReductionProxyAllowed())
    return;

  if (spdy_proxy_auth_enabled_.GetValue() != enabled) {
    spdy_proxy_auth_enabled_.SetValue(enabled);
    OnProxyEnabledPrefChange();
  }
}

int64 DataReductionProxySettings::GetDataReductionLastUpdateTime() {
  DCHECK(thread_checker_.CalledOnValidThread());
  PrefService* local_state = GetLocalStatePrefs();
  int64 last_update_internal =
      local_state->GetInt64(prefs::kDailyHttpContentLengthLastUpdateDate);
  base::Time last_update = base::Time::FromInternalValue(last_update_internal);
  return static_cast<int64>(last_update.ToJsTime());
}

DataReductionProxySettings::ContentLengthList
DataReductionProxySettings::GetDailyOriginalContentLengths() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return GetDailyContentLengths(prefs::kDailyHttpOriginalContentLength);
}

DataReductionProxySettings::ContentLengthList
DataReductionProxySettings::GetDailyReceivedContentLengths() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return GetDailyContentLengths(prefs::kDailyHttpReceivedContentLength);
}

void DataReductionProxySettings::OnURLFetchComplete(
    const net::URLFetcher* source) {
  DCHECK(thread_checker_.CalledOnValidThread());
  net::URLRequestStatus status = source->GetStatus();
  if (status.status() == net::URLRequestStatus::FAILED &&
      status.error() == net::ERR_INTERNET_DISCONNECTED) {
    RecordProbeURLFetchResult(INTERNET_DISCONNECTED);
    return;
  }

  std::string response;
  source->GetResponseAsString(&response);

  if ("OK" == response.substr(0, 2)) {
    DVLOG(1) << "The data reduction proxy is unrestricted.";

    if (enabled_by_user_) {
      if (restricted_by_carrier_) {
        // The user enabled the proxy, but sometime previously in the session,
        // the network operator had blocked the canary and restricted the user.
        // The current network doesn't block the canary, so don't restrict the
        // proxy configurations.
        SetProxyConfigs(true /* enabled */,
                        false /* restricted */,
                        false /* at_startup */);
        RecordProbeURLFetchResult(SUCCEEDED_PROXY_ENABLED);
      } else {
        RecordProbeURLFetchResult(SUCCEEDED_PROXY_ALREADY_ENABLED);
      }
    }
    restricted_by_carrier_ = false;
    return;
  }
  DVLOG(1) << "The data reduction proxy is restricted to the configured "
           << "fallback proxy.";

  if (enabled_by_user_) {
    if (!restricted_by_carrier_) {
      // Restrict the proxy.
      SetProxyConfigs(true /* enabled */,
                      true /* restricted */,
                      false /* at_startup */);
      RecordProbeURLFetchResult(FAILED_PROXY_DISABLED);
    } else {
      RecordProbeURLFetchResult(FAILED_PROXY_ALREADY_DISABLED);
    }
  }
  restricted_by_carrier_ = true;
}

void DataReductionProxySettings::OnIPAddressChanged() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (enabled_by_user_) {
    DCHECK(IsDataReductionProxyAllowed());
    ProbeWhetherDataReductionProxyIsAvailable();
  }
}

void DataReductionProxySettings::OnProxyEnabledPrefChange() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!DataReductionProxySettings::IsDataReductionProxyAllowed())
    return;
  MaybeActivateDataReductionProxy(false);
}

void DataReductionProxySettings::AddDefaultProxyBypassRules() {
  // localhost
  config_->AddHostPatternToBypass("<local>");
  // RFC1918 private addresses.
  config_->AddHostPatternToBypass("10.0.0.0/8");
  config_->AddHostPatternToBypass("172.16.0.0/12");
  config_->AddHostPatternToBypass("192.168.0.0/16");
  // RFC4193 private addresses.
  config_->AddHostPatternToBypass("fc00::/7");
  // IPV6 probe addresses.
  config_->AddHostPatternToBypass("*-ds.metric.gstatic.com");
  config_->AddHostPatternToBypass("*-v4.metric.gstatic.com");
}

void DataReductionProxySettings::LogProxyState(
    bool enabled, bool restricted, bool at_startup) {
  // This must stay a LOG(WARNING); the output is used in processing customer
  // feedback.
  const char kAtStartup[] = "at startup";
  const char kByUser[] = "by user action";
  const char kOn[] = "ON";
  const char kOff[] = "OFF";
  const char kRestricted[] = "(Restricted)";
  const char kUnrestricted[] = "(Unrestricted)";

  std::string annotated_on =
      kOn + std::string(" ") + (restricted ? kRestricted : kUnrestricted);

  LOG(WARNING) << "SPDY proxy " << (enabled ? annotated_on : kOff)
               << " " << (at_startup ? kAtStartup : kByUser);
}

PrefService* DataReductionProxySettings::GetOriginalProfilePrefs() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return prefs_;
}

PrefService* DataReductionProxySettings::GetLocalStatePrefs() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return local_state_prefs_;
}

void DataReductionProxySettings::ResetDataReductionStatistics() {
  DCHECK(thread_checker_.CalledOnValidThread());
  PrefService* prefs = GetLocalStatePrefs();
  if (!prefs)
    return;
  ListPrefUpdate original_update(prefs, prefs::kDailyHttpOriginalContentLength);
  ListPrefUpdate received_update(prefs, prefs::kDailyHttpReceivedContentLength);
  original_update->Clear();
  received_update->Clear();
  for (size_t i = 0; i < kNumDaysInHistory; ++i) {
    original_update->AppendString(base::Int64ToString(0));
    received_update->AppendString(base::Int64ToString(0));
  }
}

void DataReductionProxySettings::MaybeActivateDataReductionProxy(
    bool at_startup) {
  DCHECK(thread_checker_.CalledOnValidThread());
  PrefService* prefs = GetOriginalProfilePrefs();

  // TODO(marq): Consider moving this so stats are wiped the first time the
  // proxy settings are actually (not maybe) turned on.
  if (spdy_proxy_auth_enabled_.GetValue() &&
      !prefs->GetBoolean(prefs::kDataReductionProxyWasEnabledBefore)) {
    prefs->SetBoolean(prefs::kDataReductionProxyWasEnabledBefore, true);
    ResetDataReductionStatistics();
  }

  std::string proxy = GetDataReductionProxyOrigin();
  // Configure use of the data reduction proxy if it is enabled and the proxy
  // origin is non-empty.
  enabled_by_user_= IsDataReductionProxyEnabled() && !proxy.empty();
  SetProxyConfigs(enabled_by_user_, restricted_by_carrier_, at_startup);

  // Check if the proxy has been restricted explicitly by the carrier.
  if (enabled_by_user_)
    ProbeWhetherDataReductionProxyIsAvailable();
}

void DataReductionProxySettings::SetProxyConfigs(
    bool enabled, bool restricted, bool at_startup) {
  DCHECK(thread_checker_.CalledOnValidThread());
  // If |restricted| is true and there is no defined fallback proxy.
  // treat this as a disable.
  std::string fallback = GetDataReductionProxyFallback();
  if (fallback.empty() && enabled && restricted)
      enabled = false;

  LogProxyState(enabled, restricted, at_startup);
  if (enabled) {
    config_->Enable(restricted, GetDataReductionProxyOrigin(), fallback);
  } else {
    config_->Disable();
  }
}

// Metrics methods
void DataReductionProxySettings::RecordDataReductionInit() {
  DCHECK(thread_checker_.CalledOnValidThread());
  ProxyStartupState state = PROXY_NOT_AVAILABLE;
  if (IsDataReductionProxyAllowed()) {
    if (IsDataReductionProxyEnabled())
      state = PROXY_ENABLED;
    else
      state = PROXY_DISABLED;
  }

  RecordStartupState(state);
}

void DataReductionProxySettings::RecordProbeURLFetchResult(
    ProbeURLFetchResult result) {
  UMA_HISTOGRAM_ENUMERATION(kUMAProxyProbeURL,
                            result,
                            PROBE_URL_FETCH_RESULT_COUNT);
}

void DataReductionProxySettings::RecordStartupState(ProxyStartupState state) {
  UMA_HISTOGRAM_ENUMERATION(kUMAProxyStartupStateHistogram,
                            state,
                            PROXY_STARTUP_STATE_COUNT);
}

DataReductionProxySettings::ContentLengthList
DataReductionProxySettings::GetDailyContentLengths(const char* pref_name) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DataReductionProxySettings::ContentLengthList content_lengths;
  const base::ListValue* list_value = GetLocalStatePrefs()->GetList(pref_name);
  if (list_value->GetSize() == kNumDaysInHistory) {
    for (size_t i = 0; i < kNumDaysInHistory; ++i) {
      content_lengths.push_back(GetInt64PrefValue(*list_value, i));
    }
  }
  return content_lengths;
 }

void DataReductionProxySettings::GetContentLengths(
    unsigned int days,
    int64* original_content_length,
    int64* received_content_length,
    int64* last_update_time) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_LE(days, kNumDaysInHistory);
  PrefService* local_state = GetLocalStatePrefs();
  if (!local_state) {
    *original_content_length = 0L;
    *received_content_length = 0L;
    *last_update_time = 0L;
    return;
  }

  const base::ListValue* original_list =
      local_state->GetList(prefs::kDailyHttpOriginalContentLength);
  const base::ListValue* received_list =
      local_state->GetList(prefs::kDailyHttpReceivedContentLength);

  if (original_list->GetSize() != kNumDaysInHistory ||
      received_list->GetSize() != kNumDaysInHistory) {
    *original_content_length = 0L;
    *received_content_length = 0L;
    *last_update_time = 0L;
    return;
  }

  int64 orig = 0L;
  int64 recv = 0L;
  // Include days from the end of the list going backwards.
  for (size_t i = kNumDaysInHistory - days;
       i < kNumDaysInHistory; ++i) {
    orig += GetInt64PrefValue(*original_list, i);
    recv += GetInt64PrefValue(*received_list, i);
  }
  *original_content_length = orig;
  *received_content_length = recv;
  *last_update_time =
      local_state->GetInt64(prefs::kDailyHttpContentLengthLastUpdateDate);
}

std::string DataReductionProxySettings::GetProxyCheckURL() {
  if (!IsDataReductionProxyAllowed())
    return std::string();
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(switches::kDataReductionProxyProbeURL)) {
    return command_line.GetSwitchValueASCII(
        switches::kDataReductionProxyProbeURL);
  }
#if defined(DATA_REDUCTION_PROXY_PROBE_URL)
  return DATA_REDUCTION_PROXY_PROBE_URL;
#else
  return std::string();
#endif
}

// static
base::string16 DataReductionProxySettings::AuthHashForSalt(int64 salt) {
  if (!IsDataReductionProxyAllowed())
    return base::string16();

  std::string key;

  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(switches::kDataReductionProxy)) {
    // If an origin is provided via a switch, then only consider the value
    // that is provided by a switch. Do not use the preprocessor constant.
    // Don't expose SPDY_PROXY_AUTH_VALUE to a proxy passed in via the command
    // line.
    if (!command_line.HasSwitch(switches::kDataReductionProxyKey))
      return base::string16();
    key = command_line.GetSwitchValueASCII(switches::kDataReductionProxyKey);
  } else {
#if defined(SPDY_PROXY_AUTH_VALUE)
    key = SPDY_PROXY_AUTH_VALUE;
#else
    return base::string16();
#endif
  }

  DCHECK(!key.empty());

  std::string salted_key =
      base::StringPrintf("%lld%s%lld",
                         static_cast<long long>(salt),
                         key.c_str(),
                         static_cast<long long>(salt));
  return base::UTF8ToUTF16(base::MD5String(salted_key));
}

net::URLFetcher* DataReductionProxySettings::GetURLFetcher() {
  DCHECK(url_request_context_getter_);
  std::string url = GetProxyCheckURL();
  if (url.empty())
    return NULL;
  net::URLFetcher* fetcher = net::URLFetcher::Create(GURL(url),
                                                     net::URLFetcher::GET,
                                                     this);
  fetcher->SetLoadFlags(net::LOAD_DISABLE_CACHE | net::LOAD_BYPASS_PROXY);
  fetcher->SetRequestContext(url_request_context_getter_);
  // Configure max retries to be at most kMaxRetries times for 5xx errors.
  static const int kMaxRetries = 5;
  fetcher->SetMaxRetriesOn5xx(kMaxRetries);
  return fetcher;
}

void DataReductionProxySettings::ProbeWhetherDataReductionProxyIsAvailable() {
  net::URLFetcher* fetcher = GetURLFetcher();
  if (!fetcher)
    return;
  fetcher_.reset(fetcher);
  fetcher_->Start();
}

}  // namespace data_reduction_proxy
