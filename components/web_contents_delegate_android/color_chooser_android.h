// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEB_CONTENTS_DELEGATE_ANDROID_COLOR_CHOOSER_ANDROID_H_
#define COMPONENTS_WEB_CONTENTS_DELEGATE_ANDROID_COLOR_CHOOSER_ANDROID_H_

#include "base/android/jni_android.h"
#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "content/public/browser/color_chooser.h"
#include "content/public/browser/web_contents_observer.h"

using base::android::AttachCurrentThread;
using base::android::ScopedJavaLocalRef;

namespace components {

// Glues the Java (ColorPickerChooser.java) picker with the native part.
class ColorChooserAndroid : public content::ColorChooser,
                            public content::WebContentsObserver {
 public:
  ColorChooserAndroid(int identifier, content::WebContents* tab,
                      SkColor initial_color);
  virtual ~ColorChooserAndroid();

  void OnColorChosen(JNIEnv* env, jobject obj, jint color);

  // ColorChooser interface
  virtual void End() OVERRIDE;
  virtual void SetSelectedColor(SkColor color) OVERRIDE;

 private:
  base::android::ScopedJavaGlobalRef<jobject> j_color_chooser_;

  DISALLOW_COPY_AND_ASSIGN(ColorChooserAndroid);
};

// Native JNI methods
bool RegisterColorChooserAndroid(JNIEnv* env);

}  // namespace components

#endif  // COMPONENTS_WEB_CONTENTS_DELEGATE_ANDROID_COLOR_CHOOSER_ANDROID_H_
