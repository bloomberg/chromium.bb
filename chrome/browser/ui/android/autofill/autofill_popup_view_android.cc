// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/autofill/autofill_popup_view_android.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "chrome/browser/ui/android/window_android_helper.h"
#include "chrome/browser/ui/autofill/autofill_popup_controller.h"
#include "content/public/browser/android/content_view_core.h"
#include "jni/AutofillPopupGlue_jni.h"
#include "ui/gfx/android/window_android.h"
#include "ui/gfx/rect.h"

using base::android::MethodID;

AutofillPopupViewAndroid::AutofillPopupViewAndroid(
    AutofillPopupController* controller)
    : controller_(controller) {
  JNIEnv* env = base::android::AttachCurrentThread();
  content::ContentViewCore* content_view_core = controller_->container_view();

  java_object_.Reset(Java_AutofillPopupGlue_create(
      env,
      reinterpret_cast<jint>(this),
      content_view_core->GetWindowAndroid()->GetJavaObject().obj(),
      content_view_core->GetContainerViewDelegate().obj()));
}

AutofillPopupViewAndroid::~AutofillPopupViewAndroid() {
  controller_->ViewDestroyed();
}

void AutofillPopupViewAndroid::Show() {
  UpdateBoundsAndRedrawPopup();

  // We need an array of AutofillSuggestion.
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jclass> autofill_suggestion_clazz =
      base::android::GetClass(env,
          "org/chromium/chrome/browser/autofill/AutofillSuggestion");
  ScopedJavaLocalRef<jobjectArray> data_array(env,
      env->NewObjectArray(controller_->autofill_values().size(),
                          autofill_suggestion_clazz.obj(), NULL));
  base::android::CheckException(env);
  for (size_t i = 0; i < controller_->autofill_values().size(); ++i) {
    ScopedJavaLocalRef<jstring> value =
        base::android::ConvertUTF16ToJavaString(
            env, controller_->autofill_values()[i]);
    ScopedJavaLocalRef<jstring> label =
        base::android::ConvertUTF16ToJavaString(
            env, controller_->autofill_labels()[i]);
    int unique_id = controller_->autofill_unique_ids()[i];
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

void AutofillPopupViewAndroid::UpdateBoundsAndRedrawPopup() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_AutofillPopupGlue_setAnchorRect(env,
                                       java_object_.obj(),
                                       controller_->element_bounds().x(),
                                       controller_->element_bounds().y(),
                                       controller_->element_bounds().width(),
                                       controller_->element_bounds().height());
}

void AutofillPopupViewAndroid::SuggestionSelected(JNIEnv* env,
                                                  jobject obj,
                                                  jint list_index,
                                                  jstring value,
                                                  jint unique_id) {
  string16 value_utf16 = base::android::ConvertJavaStringToUTF16(env, value);
  controller_->AcceptAutofillSuggestion(value_utf16,
                                        unique_id,
                                        list_index);
}

void AutofillPopupViewAndroid::Dismiss(JNIEnv* env, jobject obj) {
  delete this;
}

void AutofillPopupViewAndroid::InvalidateRow(size_t) {}

// static
bool AutofillPopupViewAndroid::RegisterAutofillPopupViewAndroid(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

// static
AutofillPopupView* AutofillPopupView::Create(
    AutofillPopupController* controller) {
  return new AutofillPopupViewAndroid(controller);
}
