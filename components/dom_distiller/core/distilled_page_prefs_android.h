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
                            DistilledPagePrefs* distillerPagePrefsPtr);
  virtual ~DistilledPagePrefsAndroid();
  static bool Register(JNIEnv* env);
  void SetTheme(JNIEnv* env, jobject obj, jint theme);
  jint GetTheme(JNIEnv* env, jobject obj);

 private:
  DistilledPagePrefs* distilled_page_prefs_;

  DISALLOW_COPY_AND_ASSIGN(DistilledPagePrefsAndroid);
};

}  // namespace android
}  // namespace dom_distiller

#endif  // COMPONENTS_DOM_DISTILLER_CORE_DISTILLED_PAGE_PREFS_ANDROID_H_
