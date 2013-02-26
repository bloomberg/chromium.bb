// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/web_contents_delegate_android/color_chooser_android.h"

#include "content/public/browser/android/content_view_core.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view.h"
#include "jni/ColorChooserAndroid_jni.h"

namespace content {

ColorChooser* ColorChooser::Create(
    int identifier, WebContents* tab, SkColor initial_color) {
  return new components::ColorChooserAndroid(identifier, tab, initial_color);
}

}  // namespace content

namespace components {

ColorChooserAndroid::ColorChooserAndroid(int identifier,
                                         content::WebContents* tab,
                                         SkColor initial_color)
    : ColorChooser::ColorChooser(identifier),
      content::WebContentsObserver(tab) {
  JNIEnv* env = AttachCurrentThread();
  content::ContentViewCore* content_view_core =
      tab->GetView()->GetContentNativeView();
  DCHECK(content_view_core);

  j_color_chooser_.Reset(Java_ColorChooserAndroid_createColorChooserAndroid(
      env,
      reinterpret_cast<intptr_t>(this),
      content_view_core->GetJavaObject().obj(),
      initial_color));
}

ColorChooserAndroid::~ColorChooserAndroid() {
}

void ColorChooserAndroid::End() {
  if (!j_color_chooser_.is_null()) {
    JNIEnv* env = AttachCurrentThread();
    Java_ColorChooserAndroid_closeColorChooser(env, j_color_chooser_.obj());
  }
}

void ColorChooserAndroid::SetSelectedColor(SkColor color) {
  // Not implemented since the color is set on the java side only, in theory
  // it can be set from JS which would override the user selection but
  // we don't support that for now.
}

void ColorChooserAndroid::OnColorChosen(JNIEnv* env, jobject obj, jint color) {
  web_contents()->DidChooseColorInColorChooser(identifier(), color);
  web_contents()->DidEndColorChooser(identifier());
}

// ----------------------------------------------------------------------------
// Native JNI methods
// ----------------------------------------------------------------------------
bool RegisterColorChooserAndroid(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}  // namespace components
