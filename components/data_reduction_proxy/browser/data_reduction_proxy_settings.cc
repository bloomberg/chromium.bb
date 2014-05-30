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
#include "components/data_reduction_proxy/browser/data_reduction_proxy_params.h"
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


using base::StringPrintf;

namespace {

// Key of the UMA DataReductionProxy.StartupState histogram.
const char kUMAProxyStartupStateHistogram[] =
    "DataReductionProxy.StartupState";

// Key of the UMA DataReductionProxy.ProbeURL histogram.
const char kUMAProxyProbeURL[] = "DataReductionProxy.ProbeURL";

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

}  // namespace

namespace data_reduction_proxy {

DataReductionProxySettings::DataReductionProxySettings(
    DataReductionProxyParams* params)
    : restricted_by_carrier_(false),
      enabled_by_user_(false),
      prefs_(NULL),
      local_state_prefs_(NULL),
      url_request_context_getter_(NULL) {
  DCHECK(params);
  params_.reset(params);
}

DataReductionProxySettings::~DataReductionProxySettings() {
  if (params_->allowed())
    spdy_proxy_auth_enabled_.Destroy();
}

void DataReductionProxySettings::InitPrefMembers() {
  DCHECK(thread_checker_.CalledOnValidThread());
  spdy_proxy_auth_enabled_.Init(
      prefs::kDataReductionProxyEnabled,
      GetOriginalProfilePrefs(),
      base::Bind(&DataReductionProxySettings::OnProxyEnabledPrefChange,
                 base::Unretained(this)));
  data_reduction_proxy_alternative_enabled_.Init(
      prefs::kDataReductionProxyAltEnabled,
      GetOriginalProfilePrefs(),
      base::Bind(
          &DataReductionProxySettings::OnProxyAlternativeEnabledPrefChange,
          base::Unretained(this)));
}

void DataReductionProxySettings::InitDataReductionProxySettings(
    PrefService* prefs,
    PrefService* local_state_prefs,
    net::URLRequestContextGetter* url_request_context_getter) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(prefs);
  DCHECK(local_state_prefs);
  DCHECK(url_request_context_getter);
  prefs_ = prefs;
  local_state_prefs_ = local_state_prefs;
  url_request_context_getter_ = url_request_context_getter;
  InitPrefMembers();
  RecordDataReductionInit();

  // Disable the proxy if it is not allowed to be used.
  if (!params_->allowed())
    return;

  AddDefaultProxyBypassRules();
  net::NetworkChangeNotifier::AddIPAddressObserver(this);

  // We set or reset the proxy pref at startup.
  MaybeActivateDataReductionProxy(true);
}

void DataReductionProxySettings::InitDataReductionProxySettings(
    PrefService* prefs,
    PrefService* local_state_prefs,
    net::URLRequestContextGetter* url_request_context_getter,
    scoped_ptr<DataReductionProxyConfigurator> configurator) {
  InitDataReductionProxySettings(prefs,
                                 local_state_prefs,
                                 url_request_context_getter);
  SetProxyConfigurator(configurator.Pass());
}

void DataReductionProxySettings::SetProxyConfigurator(
    scoped_ptr<DataReductionProxyConfigurator> configurator) {
  DCHECK(configurator);
  configurator_ = configurator.Pass();
}

// static
void DataReductionProxySettings::InitDataReductionProxySession(
    net::HttpNetworkSession* session,
    const DataReductionProxyParams* params) {
// This is a no-op unless the authentication parameters are compiled in.
// (even though values for them may be specified on the command line).
// Authentication will still work if the command line parameters are used,
// however there will be a round-trip overhead for each challenge/response
// (typically once per session).
// TODO(bengr):Pass a configuration struct into DataReductionProxyConfigurator's
// constructor. The struct would carry everything in the preprocessor flags.
  DCHECK(session);
  net::HttpAuthCache* auth_cache = session->http_auth_cache();
  DCHECK(auth_cache);
  InitDataReductionAuthentication(auth_cache, params);
}

// static
void DataReductionProxySettings::InitDataReductionAuthentication(
    net::HttpAuthCache* auth_cache,
    const DataReductionProxyParams* params) {
  DCHECK(auth_cache);
  DCHECK(params);
  int64 timestamp =
      (base::Time::Now() - base::Time::UnixEpoch()).InMilliseconds() / 1000;

  DataReductionProxyParams::DataReductionProxyList proxies =
      params->GetAllowedProxies();
  for (DataReductionProxyParams::DataReductionProxyList::iterator it =
           proxies.begin();
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
    base::string16 password = AuthHashForSalt(timestamp, params->key());

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

bool DataReductionProxySettings::IsAcceptableAuthChallenge(
    net::AuthChallengeInfo* auth_info) {
  // Challenge realm must start with the authentication realm name.
  std::string realm_prefix =
      auth_info->realm.substr(0, strlen(kAuthenticationRealmName));
  if (realm_prefix != kAuthenticationRealmName)
    return false;

  // The challenger must be one of the configured proxies.
  DataReductionProxyParams::DataReductionProxyList proxies =
      params_->GetAllowedProxies();
  for (DataReductionProxyParams::DataReductionProxyList::iterator it =
       proxies.begin();
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
      return AuthHashForSalt(salt, params_->key());
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
      DataReductionProxyParams::IsKeySetOnCommandLine();
}

bool DataReductionProxySettings::IsDataReductionProxyAlternativeEnabled() {
  return data_reduction_proxy_alternative_enabled_.GetValue();
}

bool DataReductionProxySettings::IsDataReductionProxyManaged() {
  return spdy_proxy_auth_enabled_.IsManaged();
}

void DataReductionProxySettings::SetDataReductionProxyEnabled(bool enabled) {
  DCHECK(thread_checker_.CalledOnValidThread());
  // Prevent configuring the proxy when it is not allowed to be used.
  if (!params_->allowed())
    return;

  if (spdy_proxy_auth_enabled_.GetValue() != enabled) {
    spdy_proxy_auth_enabled_.SetValue(enabled);
    OnProxyEnabledPrefChange();
  }
}

void DataReductionProxySettings::SetDataReductionProxyAlternativeEnabled(
    bool enabled) {
  DCHECK(thread_checker_.CalledOnValidThread());
  // Prevent configuring the proxy when it is not allowed to be used.
  if (!params_->alternative_allowed())
    return;
  if (data_reduction_proxy_alternative_enabled_.GetValue() != enabled) {
    data_reduction_proxy_alternative_enabled_.SetValue(enabled);
    OnProxyAlternativeEnabledPrefChange();
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
                        IsDataReductionProxyAlternativeEnabled(),
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
                      IsDataReductionProxyAlternativeEnabled(),
                      true /* restricted */,
                      false /* at_startup */);
      RecordProbeURLFetchResult(FAILED_PROXY_DISABLED);
    } else {
      RecordProbeURLFetchResult(FAILED_PROXY_ALREADY_DISABLED);
    }
  }
  restricted_by_carrier_ = true;
}

PrefService* DataReductionProxySettings::GetOriginalProfilePrefs() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return prefs_;
}

PrefService* DataReductionProxySettings::GetLocalStatePrefs() {
  DCHECK(thread_checker_.CalledOnValidThread());
  return local_state_prefs_;
}

void DataReductionProxySettings::AddDefaultProxyBypassRules() {
  // localhost
  configurator_->AddHostPatternToBypass("<local>");
  // RFC1918 private addresses.
  configurator_->AddHostPatternToBypass("10.0.0.0/8");
  configurator_->AddHostPatternToBypass("172.16.0.0/12");
  configurator_->AddHostPatternToBypass("192.168.0.0/16");
  // RFC4193 private addresses.
  configurator_->AddHostPatternToBypass("fc00::/7");
  // IPV6 probe addresses.
  configurator_->AddHostPatternToBypass("*-ds.metric.gstatic.com");
  configurator_->AddHostPatternToBypass("*-v4.metric.gstatic.com");
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

void DataReductionProxySettings::OnIPAddressChanged() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (enabled_by_user_) {
    DCHECK(params_->allowed());
    ProbeWhetherDataReductionProxyIsAvailable();
  }
}

void DataReductionProxySettings::OnProxyEnabledPrefChange() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!params_->allowed())
    return;
  MaybeActivateDataReductionProxy(false);
}

void DataReductionProxySettings::OnProxyAlternativeEnabledPrefChange() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!params_->alternative_allowed())
    return;
  MaybeActivateDataReductionProxy(false);
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

  // Configure use of the data reduction proxy if it is enabled.
  enabled_by_user_= IsDataReductionProxyEnabled();
  SetProxyConfigs(enabled_by_user_,
                  IsDataReductionProxyAlternativeEnabled(),
                  restricted_by_carrier_,
                  at_startup);

  // Check if the proxy has been restricted explicitly by the carrier.
  if (enabled_by_user_)
    ProbeWhetherDataReductionProxyIsAvailable();
}

void DataReductionProxySettings::SetProxyConfigs(bool enabled,
                                                 bool alternative_enabled,
                                                 bool restricted,
                                                 bool at_startup) {
  DCHECK(thread_checker_.CalledOnValidThread());
  LogProxyState(enabled, restricted, at_startup);
  // The alternative is only configured if the standard configuration is
  // is enabled.
  if (enabled) {
    if (alternative_enabled) {
      configurator_->Enable(restricted,
                            !params_->fallback_allowed(),
                            params_->alt_origin().spec(),
                            params_->alt_fallback_origin().spec(),
                            params_->ssl_origin().spec());
    } else {
      configurator_->Enable(restricted,
                            !params_->fallback_allowed(),
                            params_->origin().spec(),
                            params_->fallback_origin().spec(),
                            std::string());
    }
  } else {
    configurator_->Disable();
  }
}

// Metrics methods
void DataReductionProxySettings::RecordDataReductionInit() {
  DCHECK(thread_checker_.CalledOnValidThread());
  ProxyStartupState state = PROXY_NOT_AVAILABLE;
  if (params_->allowed()) {
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

void DataReductionProxySettings::ResetParamsForTest(
    DataReductionProxyParams* params) {
  params_.reset(params);
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

// static
base::string16 DataReductionProxySettings::AuthHashForSalt(
    int64 salt,
    const std::string& key) {
  std::string salted_key =
      base::StringPrintf("%lld%s%lld",
                         static_cast<long long>(salt),
                         key.c_str(),
                         static_cast<long long>(salt));
  return base::UTF8ToUTF16(base::MD5String(salted_key));
}

net::URLFetcher* DataReductionProxySettings::GetURLFetcher() {
  DCHECK(url_request_context_getter_);
  net::URLFetcher* fetcher = net::URLFetcher::Create(params_->probe_url(),
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
