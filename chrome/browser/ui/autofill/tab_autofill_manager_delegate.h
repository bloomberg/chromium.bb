// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_TAB_AUTOFILL_MANAGER_DELEGATE_H_
#define CHROME_BROWSER_UI_AUTOFILL_TAB_AUTOFILL_MANAGER_DELEGATE_H_

#include "base/compiler_specific.h"
#include "chrome/browser/autofill/api/autofill_manager_delegate.h"

class TabContents;

// Chrome implementation of AutofillManagerDelegate.
class TabAutofillManagerDelegate : public autofill::AutofillManagerDelegate {
 public:
  // Lifetime of |tab| must exceed lifetime of TabAutofillManagerDelegate.
  explicit TabAutofillManagerDelegate(TabContents* tab);
  virtual ~TabAutofillManagerDelegate() {}

  virtual InfoBarTabService* GetInfoBarService() OVERRIDE;
  virtual PrefServiceBase* GetPrefs() OVERRIDE;
  virtual bool IsSavingPasswordsEnabled() const OVERRIDE;

 private:
  TabContents* const tab_;
};

#endif  // CHROME_BROWSER_UI_AUTOFILL_TAB_AUTOFILL_MANAGER_DELEGATE_H_
