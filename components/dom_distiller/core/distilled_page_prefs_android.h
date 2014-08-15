// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOM_DISTILLER_CORE_DISTILLED_PAGE_PREFS_ANDROID_H_
#define COMPONENTS_DOM_DISTILLER_CORE_DISTILLED_PAGE_PREFS_ANDROID_H_

#include <jni.h>

#include "base/android/scoped_java_ref.h"
#include "components/dom_distiller/core/distilled_page_prefs.h"

namespace dom_distiller {
namespace android {

class DistilledPagePrefsAndroid {
 public:
  DistilledPagePrefsAndroid(JNIEnv* env,
                            jobject obj,
                            DistilledPagePrefs* distilled_page_prefs_ptr);
  virtual ~DistilledPagePrefsAndroid();
  static bool Register(JNIEnv* env);
  void SetFontFamily(JNIEnv* env, jobject obj, jint font_family);
  jint GetFontFamily(JNIEnv* env, jobject obj);
  void SetTheme(JNIEnv* env, jobject obj, jint theme);
  jint GetTheme(JNIEnv* env, jobject obj);

  void AddObserver(JNIEnv* env, jobject obj, jlong obs);
  void RemoveObserver(JNIEnv* env, jobject obj, jlong obs);

 private:
  DistilledPagePrefs* distilled_page_prefs_;

  DISALLOW_COPY_AND_ASSIGN(DistilledPagePrefsAndroid);
};

class DistilledPagePrefsObserverAndroid : public DistilledPagePrefs::Observer {
 public:
  DistilledPagePrefsObserverAndroid(JNIEnv* env, jobject obj);
  virtual ~DistilledPagePrefsObserverAndroid();

  // DistilledPagePrefs::Observer implementation.
  virtual void OnChangeFontFamily(
      DistilledPagePrefs::FontFamily new_font_family) OVERRIDE;
  virtual void OnChangeTheme(DistilledPagePrefs::Theme new_theme) OVERRIDE;

  virtual void DestroyObserverAndroid(JNIEnv* env, jobject obj);

 private:
  base::android::ScopedJavaGlobalRef<jobject> java_ref_;
};

}  // namespace android
}  // namespace dom_distiller

#endif  // COMPONENTS_DOM_DISTILLER_CORE_DISTILLED_PAGE_PREFS_ANDROID_H_
