// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_AUTOFILL_AUTOFILL_EXTERNAL_DELEGATE_H_
#define CHROME_BROWSER_UI_ANDROID_AUTOFILL_AUTOFILL_EXTERNAL_DELEGATE_H_

#include <jni.h>
#include <map>
#include <vector>

#include "base/android/scoped_java_ref.h"
#include "base/compiler_specific.h"
#include "base/string16.h"
#include "chrome/browser/autofill/autofill_external_delegate.h"
#include "chrome/common/form_data.h"
#include "chrome/common/form_field_data.h"

class AutofillManager;

namespace content {
class WebContents;
}

// Android implementation of external delegate for display and selection of
// Autofill suggestions.
class AutofillExternalDelegateAndroid : public AutofillExternalDelegate {
 public:
  AutofillExternalDelegateAndroid(content::WebContents* web_contents,
                                  AutofillManager* manager);
  virtual ~AutofillExternalDelegateAndroid();

  // --------------------------------------------------------------------------
  // Methods called from Java via JNI
  // --------------------------------------------------------------------------
  // Called when an autofill item was selected.
  void AutofillPopupSelected(JNIEnv* env,
                             jobject obj,
                             jint list_index,
                             jstring value,
                             jint unique_id);

  static bool RegisterAutofillExternalDelegate(JNIEnv* env);

 private:
  virtual void ApplyAutofillSuggestions(
      const std::vector<string16>& autofill_values,
      const std::vector<string16>& autofill_labels,
      const std::vector<string16>& autofill_icons,
      const std::vector<int>& autofill_unique_ids) OVERRIDE;
  virtual void HideAutofillPopupInternal() OVERRIDE;
  virtual void SetBounds(const gfx::Rect& bounds) OVERRIDE;

  // The corresponding AutofillExternalDelegate java object.
  base::android::ScopedJavaGlobalRef<jobject> java_object_;

  DISALLOW_COPY_AND_ASSIGN(AutofillExternalDelegateAndroid);
};

#endif  // CHROME_BROWSER_UI_ANDROID_AUTOFILL_AUTOFILL_EXTERNAL_DELEGATE_H_
