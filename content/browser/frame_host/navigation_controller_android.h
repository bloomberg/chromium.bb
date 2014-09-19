// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FRAME_HOST_NAVIGATION_CONTROLLER_ANDROID_H_
#define CONTENT_BROWSER_FRAME_HOST_NAVIGATION_CONTROLLER_ANDROID_H_

#include <jni.h>

#include "base/android/scoped_java_ref.h"
#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "content/common/content_export.h"

namespace content {

class NavigationController;

// Android wrapper around NavigationController that provides safer passage
// from java and back to native and provides java with a means of communicating
// with its native counterpart.
class CONTENT_EXPORT NavigationControllerAndroid {
 public:
  static bool Register(JNIEnv* env);

  explicit NavigationControllerAndroid(
      NavigationController* navigation_controller);
  ~NavigationControllerAndroid();

  NavigationController* navigation_controller() const {
    return navigation_controller_;
  }

  base::android::ScopedJavaLocalRef<jobject> GetJavaObject();

  jboolean CanGoBack(JNIEnv* env, jobject obj);
  jboolean CanGoForward(JNIEnv* env, jobject obj);
  jboolean CanGoToOffset(JNIEnv* env, jobject obj, jint offset);
  void GoBack(JNIEnv* env, jobject obj);
  void GoForward(JNIEnv* env, jobject obj);
  void GoToOffset(JNIEnv* env, jobject obj, jint offset);
  void LoadIfNecessary(JNIEnv* env, jobject obj);
  void ContinuePendingReload(JNIEnv* env, jobject obj);
  void Reload(JNIEnv* env, jobject obj, jboolean check_for_repost);
  void ReloadIgnoringCache(JNIEnv* env, jobject obj, jboolean check_for_repost);
  void RequestRestoreLoad(JNIEnv* env, jobject obj);
  void CancelPendingReload(JNIEnv* env, jobject obj);
  void GoToNavigationIndex(JNIEnv* env, jobject obj, jint index);
  void LoadUrl(JNIEnv* env,
               jobject obj,
               jstring url,
               jint load_url_type,
               jint transition_type,
               jstring j_referrer_url,
               jint referrer_policy,
               jint ua_override_option,
               jstring extra_headers,
               jbyteArray post_data,
               jstring base_url_for_data_url,
               jstring virtual_url_for_data_url,
               jboolean can_load_local_resources,
               jboolean is_renderer_initiated);
  void ClearSslPreferences(JNIEnv* env, jobject /* obj */);
  bool GetUseDesktopUserAgent(JNIEnv* env, jobject /* obj */);
  void SetUseDesktopUserAgent(JNIEnv* env,
                              jobject /* obj */,
                              jboolean state,
                              jboolean reload_on_state_change);
  base::android::ScopedJavaLocalRef<jobject> GetPendingEntry(JNIEnv* env,
                                                             jobject /* obj */);
  int GetNavigationHistory(JNIEnv* env, jobject obj, jobject history);
  void GetDirectedNavigationHistory(JNIEnv* env,
                                    jobject obj,
                                    jobject history,
                                    jboolean is_forward,
                                    jint max_entries);
  base::android::ScopedJavaLocalRef<jstring>
      GetOriginalUrlForVisibleNavigationEntry(JNIEnv* env, jobject obj);
  void ClearHistory(JNIEnv* env, jobject obj);

 private:
  NavigationController* navigation_controller_;
  base::android::ScopedJavaGlobalRef<jobject> obj_;

  DISALLOW_COPY_AND_ASSIGN(NavigationControllerAndroid);
};

}  // namespace content

#endif  // CONTENT_BROWSER_FRAME_HOST_NAVIGATION_CONTROLLER_ANDROID_H_
