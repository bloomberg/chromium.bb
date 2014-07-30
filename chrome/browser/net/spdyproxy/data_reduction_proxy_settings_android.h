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

using base::android::ScopedJavaLocalRef;

class Profile;

namespace data_reduction_proxy {
class DataReductionProxySettings;
}

// Central point for configuring the data reduction proxy on Android.
// This object lives on the UI thread and all of its methods are expected to
// be called from there.
class DataReductionProxySettingsAndroid {
 public:
  DataReductionProxySettingsAndroid();

  virtual ~DataReductionProxySettingsAndroid();

  void InitDataReductionProxySettings(Profile* profile);

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

 private:
  friend class DataReductionProxySettingsAndroidTest;
  FRIEND_TEST_ALL_PREFIXES(DataReductionProxySettingsAndroidTest,
                           TestGetDailyContentLengths);


  ScopedJavaLocalRef<jlongArray> GetDailyContentLengths(JNIEnv* env,
                                                        const char* pref_name);

  virtual data_reduction_proxy::DataReductionProxySettings* Settings();

  DISALLOW_COPY_AND_ASSIGN(DataReductionProxySettingsAndroid);
};

#endif  // CHROME_BROWSER_NET_SPDYPROXY_DATA_REDUCTION_PROXY_SETTINGS_ANDROID_H_
