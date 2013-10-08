// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_SPDYPROXY_DATA_REDUCTION_PROXY_SETTINGS_ANDROID_H_
#define CHROME_BROWSER_NET_SPDYPROXY_DATA_REDUCTION_PROXY_SETTINGS_ANDROID_H_

#include "base/android/jni_helper.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/prefs/pref_member.h"
#include "net/base/network_change_notifier.h"
#include "net/url_request/url_fetcher_delegate.h"

using base::android::ScopedJavaLocalRef;

class PrefService;

namespace net {
class URLFetcher;
}

namespace spdyproxy {

// The number of days of bandwidth usage statistics that are tracked.
const unsigned int kNumDaysInHistory = 60;

// The number of days of bandwidth usage statistics that are presented.
const unsigned int kNumDaysInHistorySummary = 30;

COMPILE_ASSERT(kNumDaysInHistorySummary <= kNumDaysInHistory,
               DataReductionProxySettings_summary_too_long);

}  // namespace spdyproxy

// Central point for configuring the data reduction proxy on Android.
// This object lives on the UI thread and all of its methods are expected to
// be called from there.
class DataReductionProxySettingsAndroid
    : public net::URLFetcherDelegate,
      public net::NetworkChangeNotifier::IPAddressObserver {
 public:
  DataReductionProxySettingsAndroid(JNIEnv* env, jobject obj);
  virtual ~DataReductionProxySettingsAndroid();

  void InitDataReductionProxySettings(JNIEnv* env, jobject obj);

  // Add a host or URL pattern to bypass the proxy, respectively. Wildcards
  // should be compatible with the JavaScript function shExpMatch, which can be
  // used in proxy PAC resolution. These function must only be called before the
  // proxy is used.
  void BypassHostPattern(JNIEnv* env, jobject obj, jstring pattern);
  void BypassURLPattern(JNIEnv* env, jobject obj, jstring pattern);

  // Returns true if the data reduction proxy is allowed to be used on this
  // instance of Chrome. This could return false, for example, if this instance
  // is not part of the field trial, or if the proxy name is not configured
  // via gyp.
  jboolean IsDataReductionProxyAllowed(JNIEnv* env, jobject obj);

  // Returns true if a screen promoting the data reduction proxy is allowed to
  // be shown. Logic that decides when to show the promo should check its
  // availability. This would return false if not part of a separate field
  // trial that governs the use of the promotion.
  jboolean IsDataReductionProxyPromoAllowed(JNIEnv* env, jobject obj);

  // Returns the origin of the data reduction proxy.
  ScopedJavaLocalRef<jstring> GetDataReductionProxyOrigin(JNIEnv* env,
                                                          jobject obj);

  // Returns a configuration string for the proxy.
  ScopedJavaLocalRef<jstring> GetDataReductionProxyAuth(JNIEnv* env,
                                                        jobject obj);

  // Returns true if the proxy is enabled.
  jboolean IsDataReductionProxyEnabled(JNIEnv* env, jobject obj);

  // Returns true if the proxy is managed by an adminstrator's policy.
  jboolean IsDataReductionProxyManaged(JNIEnv* env, jobject obj);

  // Enables or disables the data reduction proxy. If a probe URL is available,
  // and a probe request fails at some point, the proxy won't be used until a
  // probe succeeds.
  void SetDataReductionProxyEnabled(JNIEnv* env, jobject obj, jboolean enabled);

  // Returns the time in microseconds that the last update was made to the
  // daily original and received content lengths.
  jlong GetDataReductionLastUpdateTime(JNIEnv* env, jobject obj);

  // Return a Java |ContentLengths| object containing the total number of bytes
  // of all received content, before and after compression by the data
  // reduction proxy.
  base::android::ScopedJavaLocalRef<jobject> GetContentLengths(JNIEnv* env,
                                                               jobject obj);

  // Returns an array containing the total size of all HTTP content that was
  // received over the last |kNumDaysInHistory| before any compression by the
  // data reduction proxy. Each element in the array contains one day of data.
  ScopedJavaLocalRef<jlongArray> GetDailyOriginalContentLengths(JNIEnv* env,
                                                                jobject obj);

  // Returns an array containing the aggregate received HTTP content in the last
  // |kNumDaysInHistory| days.
  ScopedJavaLocalRef<jlongArray> GetDailyReceivedContentLengths(JNIEnv* env,
                                                                jobject obj);

  // Registers the native methods to be call from Java.
  static bool Register(JNIEnv* env);

  // net::URLFetcherDelegate:
  virtual void OnURLFetchComplete(const net::URLFetcher* source) OVERRIDE;

 protected:
  void InitPrefMembers();
  virtual net::URLFetcher* GetURLFetcher();
  virtual PrefService* GetOriginalProfilePrefs();
  virtual PrefService* GetLocalStatePrefs();

 private:
  friend class DataReductionProxySettingsAndroidTest;
  FRIEND_TEST_ALL_PREFIXES(DataReductionProxySettingsAndroidTest,
                           TestBypassRules);
  FRIEND_TEST_ALL_PREFIXES(DataReductionProxySettingsAndroidTest,
                           TestResetDataReductionStatistics);
  FRIEND_TEST_ALL_PREFIXES(DataReductionProxySettingsAndroidTest,
                           TestIsProxyEnabledOrManaged);
  FRIEND_TEST_ALL_PREFIXES(DataReductionProxySettingsAndroidTest,
                           TestSetProxyPac);
  FRIEND_TEST_ALL_PREFIXES(DataReductionProxySettingsAndroidTest,
                           TestGetDailyContentLengths);
  FRIEND_TEST_ALL_PREFIXES(DataReductionProxySettingsAndroidTest,
                           TestContentLengthsInternal);
  FRIEND_TEST_ALL_PREFIXES(DataReductionProxySettingsAndroidTest,
                           TestMaybeActivateDataReductionProxy);
  FRIEND_TEST_ALL_PREFIXES(DataReductionProxySettingsAndroidTest,
                           TestOnIPAddressChanged);
  FRIEND_TEST_ALL_PREFIXES(DataReductionProxySettingsAndroidTest,
                           TestOnProxyEnabledPrefChange);
  FRIEND_TEST_ALL_PREFIXES(DataReductionProxySettingsAndroidTest,
                           TestInitDataReductionProxyOn);
  FRIEND_TEST_ALL_PREFIXES(DataReductionProxySettingsAndroidTest,
                           TestInitDataReductionProxyOff);


  // NetworkChangeNotifier::IPAddressObserver:
  virtual void OnIPAddressChanged() OVERRIDE;

  void OnProxyEnabledPrefChange();

  void AddDefaultProxyBypassRules();
  void AddURLPatternToBypass(const std::string& pattern);
  void AddHostPatternToBypass(const std::string& pattern);
  void AddPatternToBypass(const std::string& url_or_host,
                          const std::string& pattern);
  void AddHostToBypass(const std::string& host);

  void ResetDataReductionStatistics();

  void MaybeActivateDataReductionProxy(bool at_startup);
  void SetProxyPac(bool enable_spdy_proxy, bool at_startup);

  ScopedJavaLocalRef<jlongArray> GetDailyContentLengths(JNIEnv* env,
                                                        const char* pref_name);
  void GetContentLengthsInternal(unsigned int days,
                                 int64* original_content_length,
                                 int64* received_content_length,
                                 int64* last_update_time);

  // Requests the proxy probe URL, if one is set.  If unable to do so, disables
  // the proxy, if enabled. Otherwise enables the proxy if disabled by a probe
  // failure.
  void ProbeWhetherDataReductionProxyIsAvailable();

  std::string GetProxyPacScript();

  std::vector<std::string> bypass_rules_;

  // Indicate whether a user has turned on the data reduction proxy previously
  // in this session.
  bool has_turned_on_;

  // Indicate whether a user has turned off the data reduction proxy previously
  // in this session.
  bool has_turned_off_;

  bool disabled_by_carrier_;
  bool enabled_by_user_;

  scoped_ptr<net::URLFetcher> fetcher_;
  BooleanPrefMember spdy_proxy_auth_enabled_;

  DISALLOW_COPY_AND_ASSIGN(DataReductionProxySettingsAndroid);
};

#endif  // CHROME_BROWSER_NET_SPDYPROXY_DATA_REDUCTION_PROXY_SETTINGS_ANDROID_H_
