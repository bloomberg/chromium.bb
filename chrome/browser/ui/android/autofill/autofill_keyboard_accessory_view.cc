// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/autofill/autofill_keyboard_accessory_view.h"

#include <numeric>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "chrome/browser/android/resource_mapper.h"
#include "chrome/browser/ui/android/view_android_helper.h"
#include "chrome/browser/ui/autofill/autofill_popup_controller.h"
#include "chrome/browser/ui/autofill/autofill_popup_layout_model.h"
#include "components/autofill/core/browser/popup_item_ids.h"
#include "components/autofill/core/browser/suggestion.h"
#include "jni/AutofillKeyboardAccessoryBridge_jni.h"
#include "ui/android/view_android.h"
#include "ui/android/window_android.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/geometry/rect.h"

using base::android::JavaParamRef;
using base::android::JavaRef;
using base::android::ScopedJavaLocalRef;

namespace autofill {

AutofillKeyboardAccessoryView::AutofillKeyboardAccessoryView(
    AutofillPopupController* controller,
    unsigned int animation_duration_millis,
    bool should_limit_label_width)
    : controller_(controller),
      animation_duration_millis_(animation_duration_millis),
      should_limit_label_width_(should_limit_label_width),
      deleting_index_(-1) {
  JNIEnv* env = base::android::AttachCurrentThread();
  java_object_.Reset(Java_AutofillKeyboardAccessoryBridge_create(env));
}

AutofillKeyboardAccessoryView::~AutofillKeyboardAccessoryView() {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_AutofillKeyboardAccessoryBridge_resetNativeViewPointer(env,
                                                              java_object_);
}

void AutofillKeyboardAccessoryView::Show() {
  JNIEnv* env = base::android::AttachCurrentThread();
  ui::ViewAndroid* view_android = controller_->container_view();
  DCHECK(view_android);
  Java_AutofillKeyboardAccessoryBridge_init(
      env, java_object_, reinterpret_cast<intptr_t>(this),
      view_android->GetWindowAndroid()->GetJavaObject(),
      animation_duration_millis_, should_limit_label_width_);

  OnSuggestionsChanged();
}

void AutofillKeyboardAccessoryView::Hide() {
  controller_ = nullptr;
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_AutofillKeyboardAccessoryBridge_dismiss(env, java_object_);
}

void AutofillKeyboardAccessoryView::OnSelectedRowChanged(
    base::Optional<int> previous_row_selection,
    base::Optional<int> current_row_selection) {}

void AutofillKeyboardAccessoryView::OnSuggestionsChanged() {
  JNIEnv* env = base::android::AttachCurrentThread();
  size_t count = controller_->GetLineCount();
  ScopedJavaLocalRef<jobjectArray> data_array =
      Java_AutofillKeyboardAccessoryBridge_createAutofillSuggestionArray(env,
                                                                         count);
  auto is_front_item = [controller = controller_](int i) {
    const Suggestion& suggestion = controller->GetSuggestionAt(i);
    return suggestion.frontend_id == POPUP_ITEM_ID_CLEAR_FORM ||
           suggestion.frontend_id == POPUP_ITEM_ID_CREATE_HINT;
  };

  positions_.resize(count);
  std::iota(positions_.begin(), positions_.end(), 0);
  // Only one front item may exist!
  DCHECK_LT(std::count_if(positions_.begin(), positions_.end(), is_front_item),
            2);

  // Place the "CLEAR FORM" or "CREATE HINT" item first in the list.
  auto item = std::find_if(positions_.begin(), positions_.end(), is_front_item);
  if (item != positions_.end())
    std::rotate(positions_.begin(), item, item + 1);

  for (size_t i = 0; i < positions_.size(); ++i) {
    const Suggestion& suggestion = controller_->GetSuggestionAt(positions_[i]);

    base::string16 label = controller_->GetElidedLabelAt(i);
    if (label.empty())
      label = suggestion.additional_label;

    int android_icon_id = 0;
    if (!suggestion.icon.empty()) {
      android_icon_id = ResourceMapper::MapFromChromiumId(
          controller_->layout_model().GetIconResourceID(suggestion.icon));
    }

    Java_AutofillKeyboardAccessoryBridge_addToAutofillSuggestionArray(
        env, data_array, i,
        base::android::ConvertUTF16ToJavaString(env, suggestion.value),
        base::android::ConvertUTF16ToJavaString(env, label), android_icon_id,
        suggestion.frontend_id,
        controller_->GetRemovalConfirmationText(positions_[i], nullptr,
                                                nullptr));
  }

  Java_AutofillKeyboardAccessoryBridge_show(env, java_object_, data_array,
                                            controller_->IsRTL());
}

void AutofillKeyboardAccessoryView::SuggestionSelected(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jint list_index) {
  // Race: Hide() may have already run.
  if (controller_)
    controller_->AcceptSuggestion(positions_[list_index]);
}

void AutofillKeyboardAccessoryView::DeletionRequested(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jint list_index) {
  if (!controller_)
    return;

  base::string16 confirmation_title, confirmation_body;
  if (!controller_->GetRemovalConfirmationText(
          positions_[list_index], &confirmation_title, &confirmation_body)) {
    return;
  }

  deleting_index_ = positions_[list_index];
  Java_AutofillKeyboardAccessoryBridge_confirmDeletion(
      env, java_object_,
      base::android::ConvertUTF16ToJavaString(env, confirmation_title),
      base::android::ConvertUTF16ToJavaString(env, confirmation_body));
}

void AutofillKeyboardAccessoryView::DeletionConfirmed(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  if (!controller_)
    return;

  CHECK_GE(deleting_index_, 0);
  controller_->RemoveSuggestion(deleting_index_);
}

void AutofillKeyboardAccessoryView::ViewDismissed(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  if (controller_)
    controller_->ViewDestroyed();

  delete this;
}

}  // namespace autofill
