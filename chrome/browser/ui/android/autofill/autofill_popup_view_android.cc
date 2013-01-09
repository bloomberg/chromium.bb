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
    : controller_(controller) {}

AutofillPopupViewAndroid::~AutofillPopupViewAndroid() {
  controller_->ViewDestroyed();
}

void AutofillPopupViewAndroid::Show() {
  JNIEnv* env = base::android::AttachCurrentThread();
  content::ContentViewCore* content_view_core = controller_->container_view();

  java_object_.Reset(Java_AutofillPopupGlue_create(
      env,
      reinterpret_cast<jint>(this),
      content_view_core->GetWindowAndroid()->GetJavaObject().obj(),
      content_view_core->GetContainerViewDelegate().obj()));

  UpdateBoundsAndRedrawPopup();
}

void AutofillPopupViewAndroid::Hide() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_AutofillPopupGlue_dismiss(env, java_object_.obj());
}

void AutofillPopupViewAndroid::UpdateBoundsAndRedrawPopup() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_AutofillPopupGlue_setAnchorRect(env,
                                       java_object_.obj(),
                                       controller_->element_bounds().x(),
                                       controller_->element_bounds().y(),
                                       controller_->element_bounds().width(),
                                       controller_->element_bounds().height());

  // We need an array of AutofillSuggestion.
  ScopedJavaLocalRef<jclass> autofill_suggestion_clazz =
      base::android::GetClass(env,
          "org/chromium/chrome/browser/autofill/AutofillSuggestion");
  ScopedJavaLocalRef<jobjectArray> data_array(env,
      env->NewObjectArray(controller_->names().size(),
                          autofill_suggestion_clazz.obj(), NULL));
  base::android::CheckException(env);
  for (size_t i = 0; i < controller_->names().size(); ++i) {
    ScopedJavaLocalRef<jstring> name =
        base::android::ConvertUTF16ToJavaString(
            env, controller_->names()[i]);
    ScopedJavaLocalRef<jstring> subtext =
        base::android::ConvertUTF16ToJavaString(
            env, controller_->subtexts()[i]);
    int identifier = controller_->identifiers()[i];
    ScopedJavaLocalRef<jobject> data =
        Java_AutofillPopupGlue_createAutofillSuggestion(env,
                                                        name.obj(),
                                                        subtext.obj(),
                                                        identifier);
    env->SetObjectArrayElement(data_array.obj(), i, data.obj());
    base::android::CheckException(env);
  }

  Java_AutofillPopupGlue_show(env, java_object_.obj(), data_array.obj());
}

void AutofillPopupViewAndroid::SuggestionSelected(JNIEnv* env,
                                                  jobject obj,
                                                  jint list_index) {
  controller_->AcceptSuggestion(list_index);
}

void AutofillPopupViewAndroid::Dismissed(JNIEnv* env, jobject obj) {
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
