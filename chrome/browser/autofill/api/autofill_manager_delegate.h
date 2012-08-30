// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_API_AUTOFILL_MANAGER_DELEGATE_H_
#define CHROME_BROWSER_AUTOFILL_API_AUTOFILL_MANAGER_DELEGATE_H_

class InfoBarService;
class PrefServiceBase;

namespace autofill {

// A delegate interface that needs to be supplied to AutofillManager
// by the embedder.
//
// Each delegate instance is associated with a given context within
// which an AutofillManager is used (e.g. a single tab), so when we
// say "for the delegate" below, we mean "in the execution context the
// delegate is associated with" (e.g. for the tab the AutofillManager is
// attached to).
class AutofillManagerDelegate {
 public:
  virtual ~AutofillManagerDelegate() {}

  // Gets the infobar service associated with the delegate.
  virtual InfoBarService* GetInfoBarService() = 0;

  // Gets the preferences associated with the delegate.
  virtual PrefServiceBase* GetPrefs() = 0;

  // Returns true if saving passwords is currently enabled for the
  // delegate.
  virtual bool IsSavingPasswordsEnabled() const = 0;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_AUTOFILL_API_AUTOFILL_MANAGER_DELEGATE_H_
