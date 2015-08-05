// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/autofill/autofill_keyboard_accessory_view.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "chrome/browser/android/resource_mapper.h"
#include "chrome/browser/ui/android/window_android_helper.h"
#include "chrome/browser/ui/autofill/autofill_popup_controller.h"
#include "components/autofill/core/browser/suggestion.h"
#include "grit/components_strings.h"
#include "jni/AutofillKeyboardAccessoryBridge_jni.h"
#include "ui/android/view_android.h"
#include "ui/android/window_android.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/geometry/rect.h"

namespace autofill {

AutofillKeyboardAccessoryView::AutofillKeyboardAccessoryView(
    AutofillPopupController* controller)
    : controller_(controller) {
  JNIEnv* env = base::android::AttachCurrentThread();
  java_object_.Reset(Java_AutofillKeyboardAccessoryBridge_create(env));
}

AutofillKeyboardAccessoryView::~AutofillKeyboardAccessoryView() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_AutofillKeyboardAccessoryBridge_resetNativeViewPointer(
      env, java_object_.obj());
}

void AutofillKeyboardAccessoryView::Show() {
  JNIEnv* env = base::android::AttachCurrentThread();
  ui::ViewAndroid* view_android = controller_->container_view();
  DCHECK(view_android);
  Java_AutofillKeyboardAccessoryBridge_init(
      env, java_object_.obj(),
      reinterpret_cast<intptr_t>(this),
      view_android->GetWindowAndroid()->GetJavaObject().obj());

  UpdateBoundsAndRedrawPopup();
}

void AutofillKeyboardAccessoryView::Hide() {
  controller_ = nullptr;
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_AutofillKeyboardAccessoryBridge_dismiss(env, java_object_.obj());
}

void AutofillKeyboardAccessoryView::UpdateBoundsAndRedrawPopup() {
  JNIEnv* env = base::android::AttachCurrentThread();
  size_t count = controller_->GetLineCount();
  ScopedJavaLocalRef<jobjectArray> data_array =
      Java_AutofillKeyboardAccessoryBridge_createAutofillSuggestionArray(env,
                                                                         count);

  for (size_t i = 0; i < count; ++i) {
    const autofill::Suggestion& suggestion = controller_->GetSuggestionAt(i);
    int android_icon_id = 0;
    if (!suggestion.icon.empty()) {
      android_icon_id = ResourceMapper::MapFromChromiumId(
          controller_->GetIconResourceID(suggestion.icon));
    }

    Java_AutofillKeyboardAccessoryBridge_addToAutofillSuggestionArray(
        env, data_array.obj(), i,
        base::android::ConvertUTF16ToJavaString(env, suggestion.value).obj(),
        base::android::ConvertUTF16ToJavaString(env, suggestion.label).obj(),
        android_icon_id, suggestion.frontend_id);
  }

  Java_AutofillKeyboardAccessoryBridge_show(
      env, java_object_.obj(), data_array.obj(), controller_->IsRTL());
}

void AutofillKeyboardAccessoryView::SuggestionSelected(JNIEnv* env,
                                                       jobject obj,
                                                       jint list_index) {
  // Race: Hide() may have already run.
  if (controller_)
    controller_->AcceptSuggestion(list_index);
}

void AutofillKeyboardAccessoryView::ViewDismissed(JNIEnv* env, jobject obj) {
  if (controller_)
    controller_->ViewDestroyed();

  delete this;
}

void AutofillKeyboardAccessoryView::InvalidateRow(size_t) {
}

// static
bool AutofillKeyboardAccessoryView::RegisterAutofillKeyboardAccessoryView(
    JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}  // namespace autofill
