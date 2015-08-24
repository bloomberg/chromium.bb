// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_TAB_STATE_H_
#define CHROME_BROWSER_ANDROID_TAB_STATE_H_

#include <jni.h>
#include <vector>

#include "base/android/scoped_java_ref.h"

namespace content {
class NavigationEntry;
class WebContents;
}

class TabAndroid;

// Stores state for a WebContents, including its navigation history.
class WebContentsState {
 public:
  static base::android::ScopedJavaLocalRef<jobject>
      GetContentsStateAsByteBuffer(JNIEnv* env, TabAndroid* tab);

  // Common implementation for GetContentsStateAsByteBuffer() and
  // CreateContentsStateAsByteBuffer(). Does not assume ownership of the
  // navigations.
  static base::android::ScopedJavaLocalRef<jobject>
      WriteNavigationsAsByteBuffer(
          JNIEnv* env,
          bool is_off_the_record,
          const std::vector<content::NavigationEntry*>& navigations,
          int current_entry);

  // Extracts display title from serialized tab data on restore
  static base::android::ScopedJavaLocalRef<jstring>
      GetDisplayTitleFromByteBuffer(JNIEnv* env, void* data,
                                    int size, int saved_state_version);

  // Extracts virtual url from serialized tab data on restore
  static base::android::ScopedJavaLocalRef<jstring>
      GetVirtualUrlFromByteBuffer(JNIEnv* env, void* data,
                                  int size, int saved_state_version);

  // Restores a WebContents from the passed in state.
  static content::WebContents* RestoreContentsFromByteBuffer(
      void* data,
      int size,
      int saved_state_version,
      bool initially_hidden);

  // Restores a WebContents from the passed in state.
  static base::android::ScopedJavaLocalRef<jobject>
  RestoreContentsFromByteBuffer(JNIEnv* env,
                                jclass clazz,
                                jobject state,
                                jint saved_state_version,
                                jboolean initially_hidden);

  // Synthesizes a stub, single-navigation state for a tab that will be loaded
  // lazily.
  static base::android::ScopedJavaLocalRef<jobject>
      CreateSingleNavigationStateAsByteBuffer(JNIEnv* env, jstring url,
                                              jstring referrer_url,
                                              jint referrer_policy,
                                              jboolean is_off_the_record);
};

// Registers methods for JNI.
bool RegisterTabState(JNIEnv* env);

#endif  // CHROME_BROWSER_ANDROID_TAB_STATE_H_
