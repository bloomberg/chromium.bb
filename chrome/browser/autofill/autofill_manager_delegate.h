// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_AUTOFILL_MANAGER_DELEGATE_H_
#define CHROME_BROWSER_AUTOFILL_AUTOFILL_MANAGER_DELEGATE_H_

#include "base/callback_forward.h"

namespace autofill {
class PasswordGenerator;
}

namespace content {
class BrowserContext;
struct PasswordForm;
struct SSLStatus;
}

namespace gfx {
class Rect;
}

class FormStructure;
class GURL;
class InfoBarService;
class PrefServiceBase;
class Profile;
class ProfileSyncServiceBase;

struct FormData;

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

  // Gets the BrowserContext associated with the delegate.
  virtual content::BrowserContext* GetBrowserContext() const = 0;

  // Gets the BrowserContext associated with the delegate, or if in an
  // incognito mode, the associated (original) BrowserContext.
  virtual content::BrowserContext* GetOriginalBrowserContext() const = 0;

  // TODO(joi): Remove, this is temporary.
  virtual Profile* GetOriginalProfile() const = 0;

  // Gets the infobar service associated with the delegate.
  virtual InfoBarService* GetInfoBarService() = 0;

  // Gets the preferences associated with the delegate.
  virtual PrefServiceBase* GetPrefs() = 0;

  // Gets the profile sync service associated with the delegate.  Will
  // be NULL if sync is not enabled.
  virtual ProfileSyncServiceBase* GetProfileSyncService() = 0;

  // Returns true if saving passwords is currently enabled for the
  // delegate.
  virtual bool IsSavingPasswordsEnabled() const = 0;

  // Causes the Autofill settings UI to be shown.
  virtual void ShowAutofillSettings() = 0;

  // Causes the password generation bubble UI to be shown using the
  // specified form with the given bounds.
  virtual void ShowPasswordGenerationBubble(
      const gfx::Rect& bounds,
      const content::PasswordForm& form,
      autofill::PasswordGenerator* generator) = 0;

  // Causes the dialog for request autocomplete feature to be shown.
  virtual void ShowRequestAutocompleteDialog(
      const FormData& form,
      const GURL& source_url,
      const content::SSLStatus& ssl_status,
      const base::Callback<void(const FormStructure*)>& callback) = 0;

  // Called when the dialog for request autocomplete closes.
  virtual void RequestAutocompleteDialogClosed() = 0;

  // Updates the Autocheckout progress bar. |value| must be in [0.0, 1.0].
  virtual void UpdateProgressBar(double value) = 0;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_AUTOFILL_AUTOFILL_MANAGER_DELEGATE_H_
