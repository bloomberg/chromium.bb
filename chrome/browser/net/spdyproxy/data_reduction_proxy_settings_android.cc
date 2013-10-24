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
#include "net/base/auth.h"
#include "net/base/host_port_pair.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "net/url_request/url_request_status.h"
#include "url/gurl.h"

using base::android::CheckException;
using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertUTF16ToJavaString;
using base::android::ConvertUTF8ToJavaString;
using base::android::ScopedJavaLocalRef;
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

// Generates a PAC proxy string component, including trailing semicolon and
// space, for |origin|. Eg:
//     "http://foo.com/"        -> "HTTP foo.com:80; "
//     "https://bar.com:10443"  -> "HTTPS bar.coom:10443; "
// The returned strings are suitable for concatenating into a PAC string.
// If |origin| is empty, returns an empty string.
std::string ProtocolAndHostForPACString(const std::string& origin) {
  if (origin.empty()) {
    return std::string();
  }
  GURL url = GURL(origin);
  std::string protocol = url.SchemeIsSecure() ? "HTTPS "  : "HTTP ";
  return protocol + net::HostPortPair::FromURL(url).ToString() + "; ";
}

}  // namespace

DataReductionProxySettingsAndroid::DataReductionProxySettingsAndroid(
    JNIEnv* env, jobject obj): DataReductionProxySettings() {
}

DataReductionProxySettingsAndroid::~DataReductionProxySettingsAndroid() {}

void DataReductionProxySettingsAndroid::InitDataReductionProxySettings(
    JNIEnv* env,
    jobject obj) {
  DataReductionProxySettings::InitDataReductionProxySettings();
}

void DataReductionProxySettingsAndroid::BypassHostPattern(
    JNIEnv* env, jobject obj, jstring pattern) {
  DataReductionProxySettings::AddHostPatternToBypass(
      ConvertJavaStringToUTF8(env, pattern));
}
void DataReductionProxySettingsAndroid::BypassURLPattern(
    JNIEnv* env, jobject obj, jstring pattern) {
  AddURLPatternToBypass(ConvertJavaStringToUTF8(env, pattern));
}

void DataReductionProxySettingsAndroid::AddURLPatternToBypass(
    const std::string& pattern) {
  pac_bypass_rules_.push_back(
      StringPrintf("shExpMatch(%s, '%s')", "url", pattern.c_str()));
}

jboolean DataReductionProxySettingsAndroid::IsDataReductionProxyAllowed(
    JNIEnv* env, jobject obj) {
  return DataReductionProxySettings::IsDataReductionProxyAllowed();
}

jboolean DataReductionProxySettingsAndroid::IsDataReductionProxyPromoAllowed(
    JNIEnv* env, jobject obj) {
  return DataReductionProxySettings::IsDataReductionProxyPromoAllowed();
}

ScopedJavaLocalRef<jstring>
DataReductionProxySettingsAndroid::GetDataReductionProxyOrigin(
    JNIEnv* env, jobject obj) {
  return ConvertUTF8ToJavaString(
      env, DataReductionProxySettings::GetDataReductionProxyOrigin());
}

jboolean DataReductionProxySettingsAndroid::IsDataReductionProxyEnabled(
    JNIEnv* env, jobject obj) {
  return DataReductionProxySettings::IsDataReductionProxyEnabled();
}

jboolean DataReductionProxySettingsAndroid::IsDataReductionProxyManaged(
    JNIEnv* env, jobject obj) {
  return DataReductionProxySettings::IsDataReductionProxyManaged();
}

void DataReductionProxySettingsAndroid::SetDataReductionProxyEnabled(
    JNIEnv* env,
    jobject obj,
    jboolean enabled) {
  DataReductionProxySettings::SetDataReductionProxyEnabled(enabled);
}

jlong DataReductionProxySettingsAndroid::GetDataReductionLastUpdateTime(
    JNIEnv* env, jobject obj) {
  return DataReductionProxySettings::GetDataReductionLastUpdateTime();
}

base::android::ScopedJavaLocalRef<jobject>
DataReductionProxySettingsAndroid::GetContentLengths(JNIEnv* env,
                                                     jobject obj) {
  int64 original_content_length;
  int64 received_content_length;
  int64 last_update_internal;
  DataReductionProxySettings::GetContentLengths(
      spdyproxy::kNumDaysInHistorySummary,
      &original_content_length,
      &received_content_length,
      &last_update_internal);

  return Java_ContentLengths_create(env,
                                    original_content_length,
                                    received_content_length);
}

jboolean DataReductionProxySettingsAndroid::IsAcceptableAuthChallenge(
    JNIEnv* env,
    jobject obj,
    jstring host,
    jstring realm) {
  scoped_refptr<net::AuthChallengeInfo> auth_info(new net::AuthChallengeInfo);
  auth_info->realm = ConvertJavaStringToUTF8(env, realm);
  auth_info->challenger =
      net::HostPortPair::FromString(ConvertJavaStringToUTF8(env, host));
  return DataReductionProxySettings::IsAcceptableAuthChallenge(auth_info.get());
}

ScopedJavaLocalRef<jstring>
DataReductionProxySettingsAndroid::GetTokenForAuthChallenge(JNIEnv* env,
                                                            jobject obj,
                                                            jstring host,
                                                            jstring realm) {
  scoped_refptr<net::AuthChallengeInfo> auth_info(new net::AuthChallengeInfo);
  auth_info->realm = ConvertJavaStringToUTF8(env, realm);
  auth_info->challenger =
      net::HostPortPair::FromString(ConvertJavaStringToUTF8(env, host));

  // If an empty string != null in Java, then here we should test for the
  // token being empty and return a java null.
  return ConvertUTF16ToJavaString(env,
      DataReductionProxySettings::GetTokenForAuthChallenge(auth_info.get()));
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

// Metrics methods -- obsolete; see crbug/241518
void DataReductionProxySettingsAndroid::RecordDataReductionInit() {
  UMA_HISTOGRAM_ENUMERATION("SpdyProxyAuth.State", CHROME_STARTUP,
                            NUM_SPDY_PROXY_AUTH_STATE);
}

void DataReductionProxySettingsAndroid::AddDefaultProxyBypassRules() {
   DataReductionProxySettings::AddDefaultProxyBypassRules();

  // TODO(bengr): See http://crbug.com/169959. For some reason the data
  // reduction proxy is breaking the omnibox SearchProvider.  Remove this rule
  // when this is fixed.
  AddURLPatternToBypass("http://www.google.com/complete/search*");
}

void DataReductionProxySettingsAndroid::SetProxyConfigs(bool enabled,
                                                        bool at_startup) {
  // Keys duplicated from proxy_config_dictionary.cc
  // TODO(bengr): Move these to proxy_config_dictionary.h and reuse them here.
  const char kProxyMode[] = "mode";
  const char kProxyPacURL[] = "pac_url";
  const char kProxyBypassList[] = "bypass_list";

  LogProxyState(enabled, at_startup);

  PrefService* prefs = GetOriginalProfilePrefs();
  DCHECK(prefs);
  DictionaryPrefUpdate update(prefs, prefs::kProxy);
  base::DictionaryValue* dict = update.Get();
  // TODO(marq): All of the UMA in here are obsolete.
  if (enabled) {
    // Convert to a data URI and update the PAC settings.
    std::string base64_pac;
    base::Base64Encode(GetProxyPacScript(), &base64_pac);

    dict->SetString(kProxyPacURL,
                    "data:application/x-ns-proxy-autoconfig;base64," +
                    base64_pac);
    dict->SetString(kProxyMode,
                    ProxyModeToString(ProxyPrefs::MODE_PAC_SCRIPT));
    dict->SetString(kProxyBypassList, JoinString(BypassRules(), ", "));

    if (at_startup) {
      UMA_HISTOGRAM_ENUMERATION("SpdyProxyAuth.State",
                                SPDY_PROXY_AUTH_ON_AT_STARTUP,
                                NUM_SPDY_PROXY_AUTH_STATE);
    } else if (!DataReductionProxySettings::HasTurnedOn()) {
      // SPDY proxy auth is turned on by user action for the first time in
      // this session.
      UMA_HISTOGRAM_ENUMERATION("SpdyProxyAuth.State",
                                SPDY_PROXY_AUTH_ON_BY_USER,
                                NUM_SPDY_PROXY_AUTH_STATE);
      DataReductionProxySettings::SetHasTurnedOn();
    }
  } else {
    dict->SetString(kProxyMode, ProxyModeToString(ProxyPrefs::MODE_SYSTEM));
    dict->SetString(kProxyPacURL, "");
    dict->SetString(kProxyBypassList, "");

    if (!at_startup && !DataReductionProxySettings::HasTurnedOff()) {
      UMA_HISTOGRAM_ENUMERATION("SpdyProxyAuth.State",
                                SPDY_PROXY_AUTH_OFF_BY_USER,
                                NUM_SPDY_PROXY_AUTH_STATE);
      DataReductionProxySettings::SetHasTurnedOff();
    }
  }
}

ScopedJavaLocalRef<jlongArray>
DataReductionProxySettingsAndroid::GetDailyContentLengths(
    JNIEnv* env,  const char* pref_name) {
  jlongArray result = env->NewLongArray(spdyproxy::kNumDaysInHistory);

  DataReductionProxySettings::ContentLengthList lengths  =
      DataReductionProxySettings::GetDailyContentLengths(pref_name);

  if (!lengths.empty()) {
    DCHECK_EQ(lengths.size(), spdyproxy::kNumDaysInHistory);
    env->SetLongArrayRegion(result, 0, lengths.size(), &lengths[0]);
    return ScopedJavaLocalRef<jlongArray>(env, result);
  }

  return ScopedJavaLocalRef<jlongArray>(env, result);
}

// TODO(bengr): Replace with our own ProxyResolver.
std::string DataReductionProxySettingsAndroid::GetProxyPacScript() {
  // Compose the PAC-only bypass code; these will be URL patterns that
  // are matched by regular expression. Host bypasses are handled outside
  // of the PAC file using the regular proxy bypass list configs.
  std::string bypass_clause =
      "(" + JoinString(pac_bypass_rules_, ") || (") + ")";

  // Generate a proxy PAC that falls back to direct loading when the proxy is
  // unavailable and only process HTTP traffic.

  std::string proxy_host = ProtocolAndHostForPACString(
      DataReductionProxySettings::GetDataReductionProxyOrigin());
  std::string fallback_host = ProtocolAndHostForPACString(
      DataReductionProxySettings::GetDataReductionProxyFallback());
  std::string pac = "function FindProxyForURL(url, host) {"
      "  if (" + bypass_clause + ") {"
      "    return 'DIRECT';"
      "  } "
      "  if (url.substring(0, 5) == 'http:') {"
      "    return '" + proxy_host + fallback_host + "DIRECT';"
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
