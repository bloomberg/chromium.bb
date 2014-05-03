// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/web_contents_delegate_android/color_chooser_android.h"

#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "content/public/browser/android/content_view_core.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/color_suggestion.h"
#include "jni/ColorChooserAndroid_jni.h"

using base::android::ConvertUTF16ToJavaString;

namespace web_contents_delegate_android {

ColorChooserAndroid::ColorChooserAndroid(
    content::WebContents* web_contents,
    SkColor initial_color,
    const std::vector<content::ColorSuggestion>& suggestions)
    : web_contents_(web_contents) {
  JNIEnv* env = AttachCurrentThread();
  content::ContentViewCore* content_view_core =
      content::ContentViewCore::FromWebContents(web_contents);
  DCHECK(content_view_core);

  ScopedJavaLocalRef<jobjectArray> suggestions_array;

  if (suggestions.size() > 0) {
    suggestions_array = Java_ColorChooserAndroid_createColorSuggestionArray(
        env, suggestions.size());

    for (size_t i = 0; i < suggestions.size(); ++i) {
      const content::ColorSuggestion& suggestion = suggestions[i];
      ScopedJavaLocalRef<jstring> label = ConvertUTF16ToJavaString(
          env, suggestion.label);
      Java_ColorChooserAndroid_addToColorSuggestionArray(
          env,
          suggestions_array.obj(),
          i,
          suggestion.color,
          label.obj());
    }
  }
  j_color_chooser_.Reset(Java_ColorChooserAndroid_createColorChooserAndroid(
      env,
      reinterpret_cast<intptr_t>(this),
      content_view_core->GetJavaObject().obj(),
      initial_color,
      suggestions_array.obj()));
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
  web_contents_->DidChooseColorInColorChooser(color);
  web_contents_->DidEndColorChooser();
}

// ----------------------------------------------------------------------------
// Native JNI methods
// ----------------------------------------------------------------------------
bool RegisterColorChooserAndroid(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}  // namespace web_contents_delegate_android
