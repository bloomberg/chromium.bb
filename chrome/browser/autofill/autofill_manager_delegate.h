// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_AUTOFILL_MANAGER_DELEGATE_H_
#define CHROME_BROWSER_AUTOFILL_AUTOFILL_MANAGER_DELEGATE_H_

#include "base/callback_forward.h"
#include "ui/gfx/native_widget_types.h"

namespace autofill {
class PasswordGenerator;
}

namespace content {
struct PasswordForm;
struct SSLStatus;
}

namespace gfx {
class Rect;
class RectF;
}

class AutofillMetrics;
class FormStructure;
class GURL;
class InfoBarService;
class PersonalDataManager;
class PrefService;
class ProfileSyncServiceBase;

struct FormData;

namespace autofill {

enum DialogType {
  // Autofill dialog for the Autocheckout feature.
  DIALOG_TYPE_AUTOCHECKOUT,
  // Autofill dialog for the requestAutocomplete feature.
  DIALOG_TYPE_REQUEST_AUTOCOMPLETE,
};

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

  // Gets the PersonalDataManager instance associated with the delegate.
  virtual PersonalDataManager* GetPersonalDataManager() = 0;

  // Gets the preferences associated with the delegate.
  virtual PrefService* GetPrefs() = 0;

  // Gets the profile sync service associated with the delegate.  Will
  // be NULL if sync is not enabled.
  virtual ProfileSyncServiceBase* GetProfileSyncService() = 0;

  // Hides the associated request autocomplete dialog (if it exists).
  virtual void HideRequestAutocompleteDialog() = 0;

  // Returns true if saving passwords is currently enabled for the
  // delegate.
  virtual bool IsSavingPasswordsEnabled() const = 0;

  // Causes an error explaining that Autocheckout has failed to be displayed to
  // the user.
  virtual void OnAutocheckoutError() = 0;

  // Causes the Autofill settings UI to be shown.
  virtual void ShowAutofillSettings() = 0;

  // Causes the password generation bubble UI to be shown using the
  // specified form with the given bounds.
  virtual void ShowPasswordGenerationBubble(
      const gfx::Rect& bounds,
      const content::PasswordForm& form,
      autofill::PasswordGenerator* generator) = 0;

  // Causes the Autocheckout bubble UI to be displayed. |bounding_box| is the
  // anchor for the bubble. |native_view| is the parent view of the bubble.
  // |callback| is run if the bubble is accepted.
  virtual void ShowAutocheckoutBubble(
      const gfx::RectF& bounding_box,
      const gfx::NativeView& native_view,
      const base::Closure& callback) = 0;

  // Causes the dialog for request autocomplete feature to be shown.
  virtual void ShowRequestAutocompleteDialog(
      const FormData& form,
      const GURL& source_url,
      const content::SSLStatus& ssl_status,
      const AutofillMetrics& metric_logger,
      DialogType dialog_type,
      const base::Callback<void(const FormStructure*)>& callback) = 0;

  // Called when the dialog for request autocomplete closes.
  virtual void RequestAutocompleteDialogClosed() = 0;

  // Updates the Autocheckout progress bar. |value| must be in [0.0, 1.0].
  virtual void UpdateProgressBar(double value) = 0;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_AUTOFILL_AUTOFILL_MANAGER_DELEGATE_H_
