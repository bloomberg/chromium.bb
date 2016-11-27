// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_AUTOFILL_AUTOFILL_KEYBOARD_ACCESSORY_VIEW_H_
#define CHROME_BROWSER_UI_ANDROID_AUTOFILL_AUTOFILL_KEYBOARD_ACCESSORY_VIEW_H_

#include <jni.h>
#include <stddef.h>
#include <vector>

#include "base/android/scoped_java_ref.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "chrome/browser/ui/autofill/autofill_popup_view.h"

namespace autofill {

class AutofillPopupController;

// A suggestion view that acts as an alternative to the field-attached popup
// window. This view appears above the keyboard and spans the width of the
// screen, condensing rather than overlaying the content area. Enable via
// --enable-autofill-keyboard-accessory-view.
class AutofillKeyboardAccessoryView : public AutofillPopupView {
 public:
  explicit AutofillKeyboardAccessoryView(AutofillPopupController* controller);

  // --------------------------------------------------------------------------
  // Methods called from Java via JNI
  // --------------------------------------------------------------------------
  // Called when an autofill item was selected.
  void SuggestionSelected(JNIEnv* env,
                          const base::android::JavaParamRef<jobject>& obj,
                          jint list_index);

  void DeletionRequested(JNIEnv* env,
                         const base::android::JavaParamRef<jobject>& obj,
                         jint list_index);

  void DeletionConfirmed(JNIEnv* env,
                         const base::android::JavaParamRef<jobject>& obj);

  void ViewDismissed(JNIEnv* env,
                     const base::android::JavaParamRef<jobject>& obj);

  static bool RegisterAutofillKeyboardAccessoryView(JNIEnv* env);

 protected:
  // AutofillPopupView implementation.
  void Show() override;
  void Hide() override;
  void InvalidateRow(size_t row) override;
  void UpdateBoundsAndRedrawPopup() override;

 private:
  ~AutofillKeyboardAccessoryView() override;

  AutofillPopupController* controller_;  // weak.

  // The index of the last item the user long-pressed (they will be shown a
  // confirmation dialog).
  int deleting_index_;

  // Mapping from Java list index to autofill suggestion index.
  std::vector<int> positions_;

  // The corresponding java object.
  base::android::ScopedJavaGlobalRef<jobject> java_object_;

  DISALLOW_COPY_AND_ASSIGN(AutofillKeyboardAccessoryView);
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_ANDROID_AUTOFILL_AUTOFILL_KEYBOARD_ACCESSORY_VIEW_H_
