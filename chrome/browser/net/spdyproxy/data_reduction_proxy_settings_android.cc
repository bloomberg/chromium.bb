// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/spdyproxy/data_reduction_proxy_settings_android.h"

#include "base/android/build_info.h"
#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/base64.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram.h"
#include "base/prefs/pref_member.h"
#include "base/prefs/pref_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/prefs/proxy_prefs.h"
#include "chrome/browser/prefs/scoped_user_pref_update.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "jni/DataReductionProxySettings_jni.h"
#include "net/base/host_port_pair.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "net/url_request/url_request_status.h"
#include "url/gurl.h"

using base::android::CheckException;
using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertUTF8ToJavaString;
using base::android::ScopedJavaLocalRef;
using base::FieldTrialList;
using base::StringPrintf;


namespace {

// The C++ definition of enum SpdyProxyAuthState defined in
// tools/histograms/histograms.xml.
// New values should be added at the end before |NUM_SPDY_PROXY_AUTH_STATE|.
enum {
  CHROME_STARTUP,
  SPDY_PROXY_AUTH_ON_AT_STARTUP,
  SPDY_PROXY_AUTH_ON_BY_USER,
  SPDY_PROXY_AUTH_OFF_BY_USER,
  // Used by UMA histograms and should always be the last value.
  NUM_SPDY_PROXY_AUTH_STATE
};

const char kEnabled[] = "Enabled";

bool IsProxyOriginSetOnCommandLine() {
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  return command_line.HasSwitch(switches::kSpdyProxyAuthOrigin);
}

bool IsProxyAllowed() {
  return IsProxyOriginSetOnCommandLine() ||
      (FieldTrialList::FindFullName("DataCompressionProxyRollout") == kEnabled);
}

bool IsProxyPromoAllowed() {
  return IsProxyOriginSetOnCommandLine() ||
      (IsProxyAllowed() &&
          FieldTrialList::FindFullName("DataCompressionProxyPromoVisibility") ==
              kEnabled);
}

int64 GetInt64PrefValue(const ListValue& list_value, size_t index) {
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

std::string GetProxyCheckURL(){
  if (!IsProxyAllowed())
    return std::string();
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(switches::kDataReductionProxyProbeURL)) {
    return command_line.GetSwitchValueASCII(switches::kSpdyProxyAuthOrigin);
  }
#if defined(DATA_REDUCTION_PROXY_PROBE_URL)
  return DATA_REDUCTION_PROXY_PROBE_URL;
#else
  return std::string();
#endif
}

std::string GetDataReductionProxyOriginInternal() {
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(switches::kSpdyProxyAuthOrigin)) {
    return command_line.GetSwitchValueASCII(switches::kSpdyProxyAuthOrigin);
  }
#if defined(SPDY_PROXY_AUTH_ORIGIN)
  return SPDY_PROXY_AUTH_ORIGIN;
#else
  return std::string();
#endif
}

std::string GetDataReductionProxyOriginHostPort() {
  std::string spdy_proxy = GetDataReductionProxyOriginInternal();
  if (spdy_proxy.empty()) {
    DLOG(ERROR) << "A SPDY proxy has not been set.";
    return spdy_proxy;
  }
  // Remove a trailing slash from the proxy string if one exists as well as
  // leading HTTPS scheme.
  return net::HostPortPair::FromURL(GURL(spdy_proxy)).ToString();
}

}  // namespace


DataReductionProxySettingsAndroid::DataReductionProxySettingsAndroid(
    JNIEnv* env, jobject obj)
    : has_turned_on_(false),
      has_turned_off_(false),
      disabled_by_carrier_(false),
      enabled_by_user_(false) {
}

DataReductionProxySettingsAndroid::~DataReductionProxySettingsAndroid() {
  if (IsProxyAllowed())
    spdy_proxy_auth_enabled_.Destroy();
}

void DataReductionProxySettingsAndroid::InitPrefMembers() {
  spdy_proxy_auth_enabled_.Init(
      prefs::kSpdyProxyAuthEnabled,
      GetOriginalProfilePrefs(),
      base::Bind(&DataReductionProxySettingsAndroid::OnProxyEnabledPrefChange,
                 base::Unretained(this)));
}

void DataReductionProxySettingsAndroid::InitDataReductionProxySettings(
    JNIEnv* env,
    jobject obj) {
  // Disable the proxy if it is not allowed to be used.
  if (!IsProxyAllowed())
    return;

  InitPrefMembers();

  AddDefaultProxyBypassRules();
  net::NetworkChangeNotifier::AddIPAddressObserver(this);

  const CommandLine& command_line = *CommandLine::ForCurrentProcess();

  // Setting the kEnableSpdyProxyAuth switch has the same effect as enabling
  // the feature via settings, in that once set, the preference will be sticky
  // across instances of Chrome. Disabling the feature can only be done through
  // the settings menu.
  UMA_HISTOGRAM_ENUMERATION("SpdyProxyAuth.State", CHROME_STARTUP,
                            NUM_SPDY_PROXY_AUTH_STATE);
  if (spdy_proxy_auth_enabled_.GetValue() ||
      command_line.HasSwitch(switches::kEnableSpdyProxyAuth)) {
    MaybeActivateDataReductionProxy(true);
  } else {
    LOG(WARNING) << "SPDY proxy OFF at startup.";
  }
}

void DataReductionProxySettingsAndroid::BypassHostPattern(
    JNIEnv* env, jobject obj, jstring pattern) {
  AddHostPatternToBypass(ConvertJavaStringToUTF8(env, pattern));
}
void DataReductionProxySettingsAndroid::BypassURLPattern(
    JNIEnv* env, jobject obj, jstring pattern) {
  AddURLPatternToBypass(ConvertJavaStringToUTF8(env, pattern));
}

jboolean DataReductionProxySettingsAndroid::IsDataReductionProxyAllowed(
    JNIEnv* env, jobject obj) {
  return IsProxyAllowed();
}

jboolean DataReductionProxySettingsAndroid::IsDataReductionProxyPromoAllowed(
    JNIEnv* env, jobject obj) {
  return IsProxyPromoAllowed();
}

ScopedJavaLocalRef<jstring>
DataReductionProxySettingsAndroid::GetDataReductionProxyOrigin(
    JNIEnv* env, jobject obj) {
  return ConvertUTF8ToJavaString(env, GetDataReductionProxyOriginInternal());
}

ScopedJavaLocalRef<jstring>
DataReductionProxySettingsAndroid::GetDataReductionProxyAuth(
    JNIEnv* env, jobject obj) {
  if (!IsProxyAllowed())
    return ConvertUTF8ToJavaString(env, std::string());
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(switches::kSpdyProxyAuthOrigin)) {
    // If an origin is provided via a switch, then only consider the value
    // that is provided by a switch. Do not use the preprocessor constant.
    // Don't expose SPDY_PROXY_AUTH_VALUE to a proxy passed in via the command
    // line.
    if (command_line.HasSwitch(switches::kSpdyProxyAuthValue)) {
      return ConvertUTF8ToJavaString(
          env,
          command_line.GetSwitchValueASCII(switches::kSpdyProxyAuthValue));
    }
    return ConvertUTF8ToJavaString(env, std::string());
  }
#if defined(SPDY_PROXY_AUTH_VALUE)
  return ConvertUTF8ToJavaString(env, SPDY_PROXY_AUTH_VALUE);
#else
  return ConvertUTF8ToJavaString(env, std::string());
#endif
}

jboolean DataReductionProxySettingsAndroid::IsDataReductionProxyEnabled(
    JNIEnv* env, jobject obj) {
  return spdy_proxy_auth_enabled_.GetValue();
}

jboolean DataReductionProxySettingsAndroid::IsDataReductionProxyManaged(
    JNIEnv* env, jobject obj) {
  return spdy_proxy_auth_enabled_.IsManaged();
}

void DataReductionProxySettingsAndroid::SetDataReductionProxyEnabled(
    JNIEnv* env,
    jobject obj,
    jboolean enabled) {
  // Prevent configuring the proxy when it is not allowed to be used.
  if (!IsProxyAllowed())
    return;

  spdy_proxy_auth_enabled_.SetValue(enabled);
  OnProxyEnabledPrefChange();
}

jlong DataReductionProxySettingsAndroid::GetDataReductionLastUpdateTime(
    JNIEnv* env, jobject obj) {
  PrefService* local_state = GetLocalStatePrefs();
  int64 last_update_internal =
      local_state->GetInt64(prefs::kDailyHttpContentLengthLastUpdateDate);
  base::Time last_update = base::Time::FromInternalValue(last_update_internal);
  return static_cast<int64>(last_update.ToJsTime());
}

base::android::ScopedJavaLocalRef<jobject>
DataReductionProxySettingsAndroid::GetContentLengths(JNIEnv* env,
                                                     jobject obj) {
  int64 original_content_length;
  int64 received_content_length;
  int64 last_update_internal;
  GetContentLengthsInternal(spdyproxy::kNumDaysInHistorySummary,
                            &original_content_length,
                            &received_content_length,
                            &last_update_internal);

  return Java_ContentLengths_create(env,
                                    original_content_length,
                                    received_content_length);
}

ScopedJavaLocalRef<jlongArray>
DataReductionProxySettingsAndroid::GetDailyOriginalContentLengths(
    JNIEnv* env, jobject obj) {
  return GetDailyContentLengths(env, prefs::kDailyHttpOriginalContentLength);
}

ScopedJavaLocalRef<jlongArray>
DataReductionProxySettingsAndroid::GetDailyReceivedContentLengths(
    JNIEnv* env, jobject obj) {
  return GetDailyContentLengths(env, prefs::kDailyHttpReceivedContentLength);
}

// static
bool DataReductionProxySettingsAndroid::Register(JNIEnv* env) {
  bool register_natives_impl_result = RegisterNativesImpl(env);
  return register_natives_impl_result;
}

void DataReductionProxySettingsAndroid::OnURLFetchComplete(
    const net::URLFetcher* source) {
  net::URLRequestStatus status = source->GetStatus();
  if (status.status() == net::URLRequestStatus::FAILED &&
      status.error() == net::ERR_INTERNET_DISCONNECTED) {
    return;
  }

  std::string response;
  source->GetResponseAsString(&response);

  if ("OK" == response.substr(0, 2)) {
    DVLOG(1) << "The data reduction proxy is not blocked.";

    if (enabled_by_user_ && disabled_by_carrier_) {
      // The user enabled the proxy, but sometime previously in the session,
      // the network operator had blocked the proxy. Now that the network
      // operator is unblocking it, configure it to the user's desires.
      SetProxyPac(true, false);
    }
    disabled_by_carrier_ = false;
    return;
  }
  DVLOG(1) << "The data reduction proxy is blocked.";

  if (enabled_by_user_ && !disabled_by_carrier_) {
    // Disable the proxy.
    SetProxyPac(false, false);
  }
  disabled_by_carrier_ = true;
}

void DataReductionProxySettingsAndroid::OnIPAddressChanged() {
  if (enabled_by_user_) {
    DCHECK(IsProxyAllowed());
    ProbeWhetherDataReductionProxyIsAvailable();
  }
}

void DataReductionProxySettingsAndroid::OnProxyEnabledPrefChange() {
  if (!IsProxyAllowed())
    return;
  MaybeActivateDataReductionProxy(false);
}

void DataReductionProxySettingsAndroid::AddDefaultProxyBypassRules() {
  // localhost
  AddHostToBypass("localhost");
  AddHostPatternToBypass("localhost.*");
  AddHostToBypass("127.0.0.1");
  AddHostToBypass("::1");
  // TODO(bengr): revisit 192.168.*,  10.*, 172.16.0.0 - 172.31.255.255. The
  // concern was that adding these and other rules would add to the processing
  // time.

  // TODO(bengr): See http://crbug.com/169959. For some reason the data
  // reduction proxy is breaking the omnibox SearchProvider.  Remove this rule
  // when this is fixed.
  AddURLPatternToBypass("http://www.google.com/complete/search*");

  // Check for proxy availability
  std::string proxy_check_url = GetProxyCheckURL();
  if (!proxy_check_url.empty()) {
    AddURLPatternToBypass(GetProxyCheckURL());
  }
}

void DataReductionProxySettingsAndroid::AddURLPatternToBypass(
    const std::string& pattern) {
  AddPatternToBypass("url", pattern);
}

void DataReductionProxySettingsAndroid::AddHostPatternToBypass(
    const std::string& pattern) {
  AddPatternToBypass("host", pattern);
}

void DataReductionProxySettingsAndroid::AddPatternToBypass(
    const std::string& url_or_host,
    const std::string& pattern) {
  bypass_rules_.push_back(
      StringPrintf("shExpMatch(%s, '%s')",
                   url_or_host.c_str(), pattern.c_str()));
}

void DataReductionProxySettingsAndroid::AddHostToBypass(
    const std::string& host) {
  bypass_rules_.push_back(
      StringPrintf("host == '%s'", host.c_str()));
}

PrefService* DataReductionProxySettingsAndroid::GetOriginalProfilePrefs() {
  return g_browser_process->profile_manager()->GetDefaultProfile()->
      GetOriginalProfile()->GetPrefs();
}

PrefService* DataReductionProxySettingsAndroid::GetLocalStatePrefs() {
  return g_browser_process->local_state();
}

void DataReductionProxySettingsAndroid::ResetDataReductionStatistics() {
  PrefService* prefs = GetLocalStatePrefs();
  if (!prefs)
    return;
  ListPrefUpdate original_update(prefs, prefs::kDailyHttpOriginalContentLength);
  ListPrefUpdate received_update(prefs, prefs::kDailyHttpReceivedContentLength);
  original_update->Clear();
  received_update->Clear();
  for (size_t i = 0; i < spdyproxy::kNumDaysInHistory; ++i) {
    original_update->AppendString(base::Int64ToString(0));
    received_update->AppendString(base::Int64ToString(0));
  }
}

void DataReductionProxySettingsAndroid::MaybeActivateDataReductionProxy(
    bool at_startup) {
  PrefService* prefs = GetOriginalProfilePrefs();

  if (spdy_proxy_auth_enabled_.GetValue() &&
      !prefs->GetBoolean(prefs::kSpdyProxyAuthWasEnabledBefore)) {
    prefs->SetBoolean(prefs::kSpdyProxyAuthWasEnabledBefore, true);
    ResetDataReductionStatistics();
  }

  std::string spdy_proxy_origin = GetDataReductionProxyOriginHostPort();

  // Configure use of the data reduction proxy if it is enabled and the proxy
  // origin is non-empty.
  enabled_by_user_=
      spdy_proxy_auth_enabled_.GetValue() && !spdy_proxy_origin.empty();
  SetProxyPac(enabled_by_user_ && !disabled_by_carrier_, at_startup);

  // Check if the proxy has been disabled explicitly by the carrier.
  if (enabled_by_user_)
    ProbeWhetherDataReductionProxyIsAvailable();
}

void DataReductionProxySettingsAndroid::SetProxyPac(bool enable_spdy_proxy,
                                                    bool at_startup) {
  PrefService* prefs = GetOriginalProfilePrefs();
  DCHECK(prefs);
  // Keys duplicated from proxy_config_dictionary.cc
  // TODO(bengr): Move these to proxy_config_dictionary.h and reuse them here.
  const char kProxyMode[] = "mode";
  const char kProxyPacURL[] = "pac_url";
  const char kAtStartup[] = "at startup";
  const char kByUser[] = "by user action";

  DictionaryPrefUpdate update(prefs, prefs::kProxy);
  DictionaryValue* dict = update.Get();
  if (enable_spdy_proxy) {
    LOG(WARNING) << "SPDY proxy ON " << (at_startup ? kAtStartup : kByUser);
    // Convert to a data URI and update the PAC settings.
    std::string base64_pac;
    base::Base64Encode(GetProxyPacScript(), &base64_pac);

    dict->SetString(kProxyPacURL,
                    "data:application/x-ns-proxy-autoconfig;base64," +
                    base64_pac);
    dict->SetString(kProxyMode,
                    ProxyModeToString(ProxyPrefs::MODE_PAC_SCRIPT));

    if (at_startup) {
      UMA_HISTOGRAM_ENUMERATION("SpdyProxyAuth.State",
                                SPDY_PROXY_AUTH_ON_AT_STARTUP,
                                NUM_SPDY_PROXY_AUTH_STATE);
    } else if (!has_turned_on_) {
      // SPDY proxy auth is turned on by user action for the first time in
      // this session.
      UMA_HISTOGRAM_ENUMERATION("SpdyProxyAuth.State",
                                SPDY_PROXY_AUTH_ON_BY_USER,
                                NUM_SPDY_PROXY_AUTH_STATE);
      has_turned_on_ = true;
    }
  } else {
    LOG(WARNING) << "SPDY proxy OFF " << (at_startup ? kAtStartup : kByUser);
    dict->SetString(kProxyMode, ProxyModeToString(ProxyPrefs::MODE_SYSTEM));
    dict->SetString(kProxyPacURL, "");

    if (!at_startup && !has_turned_off_) {
      UMA_HISTOGRAM_ENUMERATION("SpdyProxyAuth.State",
                                SPDY_PROXY_AUTH_OFF_BY_USER,
                                NUM_SPDY_PROXY_AUTH_STATE);
      has_turned_off_ = true;
    }
  }
}

ScopedJavaLocalRef<jlongArray>
DataReductionProxySettingsAndroid::GetDailyContentLengths(
    JNIEnv* env, const char* pref_name) {
  jlongArray result = env->NewLongArray(spdyproxy::kNumDaysInHistory);
  PrefService* local_state = GetLocalStatePrefs();
  if (!local_state)
    return ScopedJavaLocalRef<jlongArray>(env, result);

  const ListValue* list_value = local_state->GetList(pref_name);
  if (list_value->GetSize() != spdyproxy::kNumDaysInHistory)
    return ScopedJavaLocalRef<jlongArray>(env, result);

  jlong jval[spdyproxy::kNumDaysInHistory];
  for (size_t i = 0; i < spdyproxy::kNumDaysInHistory; ++i) {
    jval[i] = GetInt64PrefValue(*list_value, i);
  }
  env->SetLongArrayRegion(result, 0, spdyproxy::kNumDaysInHistory, jval);
  return ScopedJavaLocalRef<jlongArray>(env, result);
}

void DataReductionProxySettingsAndroid::GetContentLengthsInternal(
    unsigned int days,
    int64* original_content_length,
    int64* received_content_length,
    int64* last_update_time) {
  DCHECK_LE(days, spdyproxy::kNumDaysInHistory);
  PrefService* local_state = GetLocalStatePrefs();
  if (!local_state) {
    *original_content_length = 0L;
    *received_content_length = 0L;
    *last_update_time = 0L;
    return;
  }

  const ListValue* original_list =
      local_state->GetList(prefs::kDailyHttpOriginalContentLength);
  const ListValue* received_list =
      local_state->GetList(prefs::kDailyHttpReceivedContentLength);

  if (original_list->GetSize() != spdyproxy::kNumDaysInHistory ||
      received_list->GetSize() != spdyproxy::kNumDaysInHistory) {
    *original_content_length = 0L;
    *received_content_length = 0L;
    *last_update_time = 0L;
    return;
  }

  int64 orig = 0L;
  int64 recv = 0L;
  // Include days from the end of the list going backwards.
  for (size_t i = spdyproxy::kNumDaysInHistory - days;
      i < spdyproxy::kNumDaysInHistory; ++i) {
    orig += GetInt64PrefValue(*original_list, i);
    recv += GetInt64PrefValue(*received_list, i);
  }
  *original_content_length = orig;
  *received_content_length = recv;
  *last_update_time =
      local_state->GetInt64(prefs::kDailyHttpContentLengthLastUpdateDate);
}

net::URLFetcher* DataReductionProxySettingsAndroid::GetURLFetcher() {
  std::string url = GetProxyCheckURL();
  if (url.empty())
    return NULL;
  net::URLFetcher* fetcher = net::URLFetcher::Create(GURL(url),
                                                     net::URLFetcher::GET,
                                                     this);
  fetcher->SetLoadFlags(net::LOAD_DISABLE_CACHE);
  Profile* profile = g_browser_process->profile_manager()->
      GetDefaultProfile();
  fetcher->SetRequestContext(profile->GetRequestContext());
  // Configure max retries to be at most kMaxRetries times for 5xx errors.
  static const int kMaxRetries = 5;
  fetcher->SetMaxRetriesOn5xx(kMaxRetries);
  return fetcher;
}

void
DataReductionProxySettingsAndroid::ProbeWhetherDataReductionProxyIsAvailable() {
  net::URLFetcher* fetcher = GetURLFetcher();
  if (!fetcher)
    return;
  fetcher_.reset(fetcher);
  fetcher_->Start();
}

// TODO(bengr): Replace with our own ProxyResolver.
std::string DataReductionProxySettingsAndroid::GetProxyPacScript() {
  std::string bypass_clause = "(" + JoinString(bypass_rules_, ") || (") + ")";

  // Generate a proxy PAC that falls back to direct loading when the proxy is
  // unavailable and only process HTTP traffic. (With a statically configured
  // proxy, proxy failures will simply result in a connection error presented to
  // users.)

  std::string pac = "function FindProxyForURL(url, host) {"
      "  if (" + bypass_clause + ") {"
      "    return 'DIRECT';"
      "  } "
      "  if (url.substring(0, 5) == 'http:') {"
      "    return 'HTTPS " + GetDataReductionProxyOriginHostPort() +
      "; DIRECT';"
      "  }"
      "  return 'DIRECT';"
      "}";
  return pac;
}

// Used by generated jni code.
static jint Init(JNIEnv* env, jobject obj) {
  DataReductionProxySettingsAndroid* settings =
      new DataReductionProxySettingsAndroid(env, obj);
  return reinterpret_cast<jint>(settings);
}


