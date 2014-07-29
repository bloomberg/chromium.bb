// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/autofill/autofill_popup_view_android.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "chrome/browser/ui/android/window_android_helper.h"
#include "chrome/browser/ui/autofill/autofill_popup_controller.h"
#include "content/public/browser/android/content_view_core.h"
#include "jni/AutofillPopupBridge_jni.h"
#include "ui/base/android/view_android.h"
#include "ui/base/android/window_android.h"
#include "ui/gfx/rect.h"

namespace autofill {

AutofillPopupViewAndroid::AutofillPopupViewAndroid(
    AutofillPopupController* controller)
    : controller_(controller) {}

AutofillPopupViewAndroid::~AutofillPopupViewAndroid() {}

void AutofillPopupViewAndroid::Show() {
  JNIEnv* env = base::android::AttachCurrentThread();
  ui::ViewAndroid* view_android = controller_->container_view();

  DCHECK(view_android);

  java_object_.Reset(Java_AutofillPopupBridge_create(
      env,
      reinterpret_cast<intptr_t>(this),
      view_android->GetWindowAndroid()->GetJavaObject().obj(),
      view_android->GetJavaObject().obj()));

  UpdateBoundsAndRedrawPopup();
}

void AutofillPopupViewAndroid::Hide() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_AutofillPopupBridge_hide(env, java_object_.obj());
  delete this;
}

void AutofillPopupViewAndroid::UpdateBoundsAndRedrawPopup() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_AutofillPopupBridge_setAnchorRect(
      env,
      java_object_.obj(),
      controller_->element_bounds().x(),
      controller_->element_bounds().y(),
      controller_->element_bounds().width(),
      controller_->element_bounds().height());

  // We need an array of AutofillSuggestion.
  size_t count = controller_->names().size();

  ScopedJavaLocalRef<jobjectArray> data_array =
      Java_AutofillPopupBridge_createAutofillSuggestionArray(env, count);

  for (size_t i = 0; i < count; ++i) {
    ScopedJavaLocalRef<jstring> name =
        base::android::ConvertUTF16ToJavaString(env, controller_->names()[i]);
    ScopedJavaLocalRef<jstring> subtext =
        base::android::ConvertUTF16ToJavaString(env,
                                                controller_->subtexts()[i]);
    Java_AutofillPopupBridge_addToAutofillSuggestionArray(
        env,
        data_array.obj(),
        i,
        name.obj(),
        subtext.obj(),
        controller_->identifiers()[i]);
  }

  Java_AutofillPopupBridge_show(
      env, java_object_.obj(), data_array.obj(), controller_->IsRTL());
}

void AutofillPopupViewAndroid::SuggestionSelected(JNIEnv* env,
                                                  jobject obj,
                                                  jint list_index) {
  controller_->AcceptSuggestion(list_index);
}

void AutofillPopupViewAndroid::RequestHide(JNIEnv* env, jobject obj) {
  controller_->Hide();
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

}  // namespace autofill
