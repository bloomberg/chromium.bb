// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_SPDYPROXY_DATA_REDUCTION_PROXY_SETTINGS_ANDROID_H_
#define CHROME_BROWSER_NET_SPDYPROXY_DATA_REDUCTION_PROXY_SETTINGS_ANDROID_H_

#include "base/android/jni_string.h"
#include "base/android/jni_weak_ref.h"
#include "base/android/scoped_java_ref.h"
#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/prefs/pref_member.h"
#include "components/data_reduction_proxy/browser/data_reduction_proxy_settings.h"
#include "components/keyed_service/core/keyed_service.h"

using base::android::ScopedJavaLocalRef;

class Profile;

namespace data_reduction_proxy {
class DataReductionProxyParams;
}

// Central point for configuring the data reduction proxy on Android.
// This object lives on the UI thread and all of its methods are expected to
// be called from there.
class DataReductionProxySettingsAndroid
    : public data_reduction_proxy::DataReductionProxySettings,
      public KeyedService {
 public:
  // Factory constructor.
  DataReductionProxySettingsAndroid(
      data_reduction_proxy::DataReductionProxyParams* params);


  virtual ~DataReductionProxySettingsAndroid();

  void InitDataReductionProxySettings(Profile* profile);

  void BypassHostPattern(JNIEnv* env, jobject obj, jstring pattern);
  // Add a URL pattern to bypass the proxy. Wildcards
  // should be compatible with the JavaScript function shExpMatch, which can be
  // used in proxy PAC resolution. These functions must only be called before
  // the proxy is used.
  void BypassURLPattern(JNIEnv* env, jobject obj, jstring pattern);

  // JNI wrapper interfaces to the indentically-named superclass methods.
  jboolean IsDataReductionProxyAllowed(JNIEnv* env, jobject obj);
  jboolean IsDataReductionProxyPromoAllowed(JNIEnv* env, jobject obj);
  jboolean IsIncludedInAltFieldTrial(JNIEnv* env, jobject obj);
  ScopedJavaLocalRef<jstring> GetDataReductionProxyOrigin(JNIEnv* env,
                                                          jobject obj);
  jboolean IsDataReductionProxyEnabled(JNIEnv* env, jobject obj);
  jboolean IsDataReductionProxyManaged(JNIEnv* env, jobject obj);
  void SetDataReductionProxyEnabled(JNIEnv* env, jobject obj, jboolean enabled);

  jlong GetDataReductionLastUpdateTime(JNIEnv* env, jobject obj);
  ScopedJavaLocalRef<jlongArray> GetDailyOriginalContentLengths(JNIEnv* env,
                                                                jobject obj);
  ScopedJavaLocalRef<jlongArray> GetDailyReceivedContentLengths(JNIEnv* env,
                                                                jobject obj);

  // Return a Java |ContentLengths| object wrapping the results of a call to
  // DataReductionProxySettings::GetContentLengths.
  base::android::ScopedJavaLocalRef<jobject> GetContentLengths(JNIEnv* env,
                                                               jobject obj);

  // Determines whether the data reduction proxy is unreachable. This is
  // done by keeping a count of requests which go through proxy vs those
  // which should have gone through the proxy based on the config.
  jboolean IsDataReductionProxyUnreachable(JNIEnv* env, jobject obj);

  ScopedJavaLocalRef<jstring> GetTokenForAuthChallenge(JNIEnv* env,
                                                       jobject obj,
                                                       jstring host,
                                                       jstring realm);

  // Registers the native methods to be call from Java.
  static bool Register(JNIEnv* env);

 protected:
  // DataReductionProxySettings overrides.
  virtual void AddDefaultProxyBypassRules() OVERRIDE;

  // Configures the proxy settings by generating a data URL containing a PAC
  // file.
  virtual void SetProxyConfigs(bool enabled,
                               bool alt_enabled,
                               bool restricted,
                               bool at_startup) OVERRIDE;

 private:
  friend class DataReductionProxySettingsAndroidTest;
  FRIEND_TEST_ALL_PREFIXES(DataReductionProxySettingsAndroidTest,
                           TestGetDailyContentLengths);


  ScopedJavaLocalRef<jlongArray> GetDailyContentLengths(JNIEnv* env,
                                                        const char* pref_name);

  DISALLOW_COPY_AND_ASSIGN(DataReductionProxySettingsAndroid);
};

#endif  // CHROME_BROWSER_NET_SPDYPROXY_DATA_REDUCTION_PROXY_SETTINGS_ANDROID_H_
