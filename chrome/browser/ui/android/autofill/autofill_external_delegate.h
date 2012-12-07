// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_AUTOFILL_AUTOFILL_EXTERNAL_DELEGATE_H_
#define CHROME_BROWSER_UI_ANDROID_AUTOFILL_AUTOFILL_EXTERNAL_DELEGATE_H_

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/autofill/autofill_external_delegate.h"

class AutofillPopupViewAndroid;

// Android implementation of external delegate for display and selection of
// Autofill suggestions.
class AutofillExternalDelegateAndroid : public AutofillExternalDelegate {
 public:
  AutofillExternalDelegateAndroid(content::WebContents* web_contents,
                                  AutofillManager* manager);
  virtual ~AutofillExternalDelegateAndroid();

 protected:
  // AutofillExternalDelegate implementations.
  virtual void ApplyAutofillSuggestions(
      const std::vector<string16>& autofill_values,
      const std::vector<string16>& autofill_labels,
      const std::vector<string16>& autofill_icons,
      const std::vector<int>& autofill_unique_ids) OVERRIDE;
  virtual void CreatePopupForElement(const gfx::Rect& element_bounds) OVERRIDE;
  virtual void HideAutofillPopupInternal() OVERRIDE;

 private:
  scoped_ptr<AutofillPopupViewAndroid> view_;

  DISALLOW_COPY_AND_ASSIGN(AutofillExternalDelegateAndroid);
};

#endif  // CHROME_BROWSER_UI_ANDROID_AUTOFILL_AUTOFILL_EXTERNAL_DELEGATE_H_
