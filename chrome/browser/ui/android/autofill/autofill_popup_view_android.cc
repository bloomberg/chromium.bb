// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "autofill_popup_view_android.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "chrome/browser/ui/android/autofill/autofill_external_delegate.h"
#include "chrome/browser/ui/android/window_android_helper.h"
#include "content/public/browser/android/content_view_core.h"
#include "content/public/browser/browser_thread.h"
#include "jni/AutofillPopupGlue_jni.h"
#include "ui/gfx/android/window_android.h"
#include "ui/gfx/rect.h"

using base::android::MethodID;
using content::BrowserThread;

AutofillPopupViewAndroid::AutofillPopupViewAndroid(
    content::WebContents* web_contents,
    AutofillExternalDelegate* external_delegate,
    const gfx::Rect& element_bounds)
    : AutofillPopupView(web_contents, external_delegate, element_bounds),
      web_contents_(web_contents) {
  JNIEnv* env = base::android::AttachCurrentThread();
  content::ContentViewCore* content_view_core =
      content::ContentViewCore::FromWebContents(web_contents);
  ui::WindowAndroid* window_android =
      WindowAndroidHelper::FromWebContents(web_contents_)->GetWindowAndroid();

  java_object_.Reset(Java_AutofillPopupGlue_create(env,
      reinterpret_cast<jint>(this), window_android->GetJavaObject().obj(),
      content_view_core->GetContainerViewDelegate().obj()));
}

AutofillPopupViewAndroid::~AutofillPopupViewAndroid() {
}

void AutofillPopupViewAndroid::ShowInternal() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // We need an array of AutofillSuggestion.
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jclass> autofill_suggestion_clazz =
      base::android::GetClass(env,
          "org/chromium/chrome/browser/autofill/AutofillSuggestion");
  ScopedJavaLocalRef<jobjectArray> data_array(env,
      env->NewObjectArray(autofill_values().size(),
                          autofill_suggestion_clazz.obj(), NULL));
  base::android::CheckException(env);
  for (size_t i = 0; i < autofill_values().size(); ++i) {
    ScopedJavaLocalRef<jstring> value =
        base::android::ConvertUTF16ToJavaString(env, autofill_values()[i]);
    ScopedJavaLocalRef<jstring> label =
        base::android::ConvertUTF16ToJavaString(env, autofill_labels()[i]);
    int unique_id = autofill_unique_ids()[i];
    ScopedJavaLocalRef<jobject> data =
        Java_AutofillPopupGlue_createAutofillSuggestion(env,
                                                        value.obj(),
                                                        label.obj(),
                                                        unique_id);
    env->SetObjectArrayElement(data_array.obj(), i, data.obj());
    base::android::CheckException(env);
  }

  Java_AutofillPopupGlue_show(env, java_object_.obj(), data_array.obj());
}
void AutofillPopupViewAndroid::Hide() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_AutofillPopupGlue_dismissFromNative(env, java_object_.obj());
}

void AutofillPopupViewAndroid::UpdateBoundsAndRedrawPopupInternal() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_AutofillPopupGlue_setAnchorRect(env,
                                       java_object_.obj(),
                                       element_bounds().x(),
                                       element_bounds().y(),
                                       element_bounds().width(),
                                       element_bounds().height());
}

void AutofillPopupViewAndroid::SuggestionSelected(JNIEnv *env,
                                                  jobject obj,
                                                  jint list_index,
                                                  jstring value,
                                                  jint unique_id) {
  string16 value_utf16 = base::android::ConvertJavaStringToUTF16(env, value);
  external_delegate()->DidAcceptAutofillSuggestions(value_utf16,
                                                    unique_id,
                                                    list_index);
}

void AutofillPopupViewAndroid::Dismiss(JNIEnv *env,
                                      jobject obj) {
  external_delegate()->HideAutofillPopup();
}

void AutofillPopupViewAndroid::InvalidateRow(size_t) {
}

// static
bool AutofillPopupViewAndroid::RegisterAutofillPopupViewAndroid(
    JNIEnv* env) {
  return RegisterNativesImpl(env);
}
