// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_TEST_AUTOFILL_EXTERNAL_DELEGATE_H_
#define CHROME_BROWSER_AUTOFILL_TEST_AUTOFILL_EXTERNAL_DELEGATE_H_
#pragma once

#include "chrome/browser/autofill/autofill_external_delegate.h"

class AutofillManager;
class TabContents;

// This test class is meant to give tests a base AutofillExternalDelegate
// class that requires no additional work to compile with (i.e. all the
// pure virtual functions have been giving empty methods).
class TestAutofillExternalDelegate : public AutofillExternalDelegate {
 public:
  TestAutofillExternalDelegate(TabContents* tab_contents,
                               AutofillManager* autofill_manager);
  virtual ~TestAutofillExternalDelegate();

  virtual void ApplyAutofillSuggestions(
      const std::vector<string16>& autofill_values,
      const std::vector<string16>& autofill_labels,
      const std::vector<string16>& autofill_icons,
      const std::vector<int>& autofill_unique_ids) OVERRIDE;

  virtual void OnQueryPlatformSpecific(int query_id,
                                       const webkit::forms::FormData& form,
                                       const webkit::forms::FormField& field,
                                       const gfx::Rect& bounds) OVERRIDE;


  virtual void HideAutofillPopupInternal() OVERRIDE;

  virtual void SetBounds(const gfx::Rect& bounds) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(TestAutofillExternalDelegate);
};

#endif  // CHROME_BROWSER_AUTOFILL_TEST_AUTOFILL_EXTERNAL_DELEGATE_H_
