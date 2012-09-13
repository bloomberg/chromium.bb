// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_AUTOFILL_MANAGER_DELEGATE_H_
#define CHROME_BROWSER_AUTOFILL_AUTOFILL_MANAGER_DELEGATE_H_

namespace autofill {
class PasswordGenerator;
}

namespace content {
class BrowserContext;
}

namespace gfx {
class Rect;
}

namespace webkit {
namespace forms {
struct PasswordForm;
}
}

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

  // Gets the BrowserContext the AutofillManager is in.
  virtual content::BrowserContext* GetBrowserContext() const = 0;

  // Gets the BrowserContext the AutofillManager is in, or if in an
  // incognito mode, the associated (original) BrowserContext.
  virtual content::BrowserContext* GetOriginalBrowserContext() const = 0;

  // Gets the infobar service associated with the delegate.
  virtual InfoBarService* GetInfoBarService() = 0;

  // Gets the preferences associated with the delegate.
  virtual PrefServiceBase* GetPrefs() = 0;

  // Returns true if saving passwords is currently enabled for the
  // delegate.
  virtual bool IsSavingPasswordsEnabled() const = 0;

  // Causes the Autofill settings UI to be shown.
  virtual void ShowAutofillSettings() = 0;

  // Causes the password generation bubble UI to be shown using the
  // specified form with the given bounds.
  virtual void ShowPasswordGenerationBubble(
      const gfx::Rect& bounds,
      const webkit::forms::PasswordForm& form,
      autofill::PasswordGenerator* generator) = 0;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_AUTOFILL_AUTOFILL_MANAGER_DELEGATE_H_
