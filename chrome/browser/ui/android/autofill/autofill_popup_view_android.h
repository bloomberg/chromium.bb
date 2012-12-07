// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_AUTOFILL_AUTOFILL_POPUP_VIEW_ANDROID_H_
#define CHROME_BROWSER_UI_ANDROID_AUTOFILL_AUTOFILL_POPUP_VIEW_ANDROID_H_

#include <jni.h>

#include "base/android/scoped_java_ref.h"
#include "chrome/browser/autofill/autofill_popup_view.h"

namespace content {
class WebContents;
}

namespace gfx {
class Rect;
}

class AutofillPopupViewAndroid : public AutofillPopupView {
 public:
  AutofillPopupViewAndroid(content::WebContents* web_contents,
                           AutofillExternalDelegate* external_delegate,
                           const gfx::Rect& element_bounds);
  virtual ~AutofillPopupViewAndroid();

  // --------------------------------------------------------------------------
  // Methods called from Java via JNI
  // --------------------------------------------------------------------------
  // Called when an autofill item was selected.
  void SuggestionSelected(JNIEnv* env,
                          jobject obj,
                          jint list_index,
                          jstring value,
                          jint unique_id);

  // AutofillPopupView implementation.
  virtual void Hide() OVERRIDE;

  void Dismiss(JNIEnv *env, jobject obj);

  static bool RegisterAutofillPopupViewAndroid(JNIEnv* env);

 protected:
  // AutofillPopupView implementations.
  virtual void ShowInternal() OVERRIDE;
  virtual void InvalidateRow(size_t row) OVERRIDE;
  virtual void UpdateBoundsAndRedrawPopupInternal() OVERRIDE;

 private:
  content::WebContents* web_contents_;  // weak; owns me.

  // The corresponding AutofillExternalDelegate java object.
  base::android::ScopedJavaGlobalRef<jobject> java_object_;

  DISALLOW_COPY_AND_ASSIGN(AutofillPopupViewAndroid);
};

#endif  // CHROME_BROWSER_UI_ANDROID_AUTOFILL_AUTOFILL_POPUP_VIEW_ANDROID_H_
