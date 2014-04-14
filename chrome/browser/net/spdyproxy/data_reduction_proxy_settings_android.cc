// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/spdyproxy/data_reduction_proxy_settings_android.h"

#include "base/android/build_info.h"
#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/memory/scoped_ptr.h"
#include "base/metrics/histogram.h"
#include "base/prefs/pref_member.h"
#include "base/prefs/pref_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/net/spdyproxy/data_reduction_proxy_chrome_configurator.h"
#include "chrome/browser/prefs/proxy_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "components/data_reduction_proxy/browser/data_reduction_proxy_configurator.h"
#include "components/data_reduction_proxy/browser/data_reduction_proxy_settings.h"
#include "jni/DataReductionProxySettings_jni.h"
#include "net/base/auth.h"
#include "net/base/host_port_pair.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "net/url_request/url_request_status.h"

using base::android::CheckException;
using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertUTF16ToJavaString;
using base::android::ConvertUTF8ToJavaString;
using base::android::ScopedJavaLocalRef;
using data_reduction_proxy::DataReductionProxySettings;

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

}  // namespace

DataReductionProxySettingsAndroid::DataReductionProxySettingsAndroid(
    JNIEnv* env, jobject obj): DataReductionProxySettings() {
}

DataReductionProxySettingsAndroid::DataReductionProxySettingsAndroid() {}

DataReductionProxySettingsAndroid::~DataReductionProxySettingsAndroid() {}

void DataReductionProxySettingsAndroid::InitDataReductionProxySettings(
    JNIEnv* env,
    jobject obj) {
  scoped_ptr<data_reduction_proxy::DataReductionProxyConfigurator>
      configurator(
          new DataReductionProxyChromeConfigurator(
              ProfileManager::GetActiveUserProfile()->GetPrefs()));
  DataReductionProxySettings::InitDataReductionProxySettings(
      ProfileManager::GetActiveUserProfile()->GetPrefs(),
      g_browser_process->local_state(),
      ProfileManager::GetActiveUserProfile()->GetRequestContext(),
      configurator.Pass());
}

void DataReductionProxySettingsAndroid::BypassHostPattern(
    JNIEnv* env, jobject obj, jstring pattern) {
  config()->AddHostPatternToBypass(
      ConvertJavaStringToUTF8(env, pattern));
}
void DataReductionProxySettingsAndroid::BypassURLPattern(
    JNIEnv* env, jobject obj, jstring pattern) {
  config()->AddURLPatternToBypass(ConvertJavaStringToUTF8(env, pattern));
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
      data_reduction_proxy::kNumDaysInHistorySummary,
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
  return GetDailyContentLengths(
      env, data_reduction_proxy::prefs::kDailyHttpOriginalContentLength);
}

ScopedJavaLocalRef<jlongArray>
DataReductionProxySettingsAndroid::GetDailyReceivedContentLengths(
    JNIEnv* env, jobject obj) {
  return GetDailyContentLengths(
      env, data_reduction_proxy::prefs::kDailyHttpReceivedContentLength);
}

// static
bool DataReductionProxySettingsAndroid::Register(JNIEnv* env) {
  bool register_natives_impl_result = RegisterNativesImpl(env);
  return register_natives_impl_result;
}

void DataReductionProxySettingsAndroid::AddDefaultProxyBypassRules() {
   DataReductionProxySettings::AddDefaultProxyBypassRules();
  // Chrome cannot authenticate with the data reduction proxy when fetching URLs
  // from the settings menu.
  config()->AddURLPatternToBypass("http://www.google.com/policies/privacy*");
}

void DataReductionProxySettingsAndroid::SetProxyConfigs(bool enabled,
                                                        bool restricted,
                                                        bool at_startup) {
  // Sanity check: If there's no fallback proxy, we can't do a restricted mode.
  std::string fallback = GetDataReductionProxyFallback();
  if (fallback.empty() && enabled && restricted)
      enabled = false;

  LogProxyState(enabled, restricted, at_startup);

  if (enabled) {
    config()->Enable(restricted,
                     DataReductionProxySettings::GetDataReductionProxyOrigin(),
                     GetDataReductionProxyFallback());
  } else {
    config()->Disable();
  }
}

ScopedJavaLocalRef<jlongArray>
DataReductionProxySettingsAndroid::GetDailyContentLengths(
    JNIEnv* env,  const char* pref_name) {
  jlongArray result = env->NewLongArray(
      data_reduction_proxy::kNumDaysInHistory);

  DataReductionProxySettings::ContentLengthList lengths  =
      DataReductionProxySettings::GetDailyContentLengths(pref_name);

  if (!lengths.empty()) {
    DCHECK_EQ(lengths.size(), data_reduction_proxy::kNumDaysInHistory);
    env->SetLongArrayRegion(result, 0, lengths.size(), &lengths[0]);
    return ScopedJavaLocalRef<jlongArray>(env, result);
  }

  return ScopedJavaLocalRef<jlongArray>(env, result);
}



// Used by generated jni code.
static jlong Init(JNIEnv* env, jobject obj) {
  DataReductionProxySettingsAndroid* settings =
      new DataReductionProxySettingsAndroid(env, obj);
  return reinterpret_cast<intptr_t>(settings);
}
