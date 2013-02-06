// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_TAB_AUTOFILL_MANAGER_DELEGATE_H_
#define CHROME_BROWSER_UI_AUTOFILL_TAB_AUTOFILL_MANAGER_DELEGATE_H_

#include "base/compiler_specific.h"
#include "chrome/browser/autofill/autofill_manager_delegate.h"
#include "chrome/browser/ui/autofill/autofill_dialog_types.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace autofill {
class AutofillDialogControllerImpl;
}

namespace content {
struct FrameNavigateParams;
struct LoadCommittedDetails;
class WebContents;
}

// Chrome implementation of AutofillManagerDelegate.
class TabAutofillManagerDelegate
    : public autofill::AutofillManagerDelegate,
      public content::WebContentsUserData<TabAutofillManagerDelegate>,
      public content::WebContentsObserver {
 public:
  virtual ~TabAutofillManagerDelegate() {}

  virtual content::BrowserContext* GetBrowserContext() const OVERRIDE;
  virtual content::BrowserContext* GetOriginalBrowserContext() const OVERRIDE;
  virtual Profile* GetOriginalProfile() const OVERRIDE;
  virtual InfoBarService* GetInfoBarService() OVERRIDE;
  virtual PrefServiceBase* GetPrefs() OVERRIDE;
  virtual ProfileSyncServiceBase* GetProfileSyncService() OVERRIDE;
  virtual bool IsSavingPasswordsEnabled() const OVERRIDE;
  virtual void ShowAutofillSettings() OVERRIDE;
  virtual void ShowPasswordGenerationBubble(
      const gfx::Rect& bounds,
      const content::PasswordForm& form,
      autofill::PasswordGenerator* generator) OVERRIDE;
  virtual void ShowAutocheckoutBubble(
      const gfx::RectF& bounds,
      const gfx::NativeView& native_view,
      const base::Closure& callback) OVERRIDE;
  virtual void ShowRequestAutocompleteDialog(
      const FormData& form,
      const GURL& source_url,
      const content::SSLStatus& ssl_status,
      const base::Callback<void(const FormStructure*)>& callback) OVERRIDE;
  virtual void RequestAutocompleteDialogClosed() OVERRIDE;
  virtual void UpdateProgressBar(double value) OVERRIDE;

  // content::WebContentsObserver implementation.
  virtual void DidNavigateMainFrame(
      const content::LoadCommittedDetails& details,
      const content::FrameNavigateParams& params) OVERRIDE;

 private:
  explicit TabAutofillManagerDelegate(content::WebContents* web_contents);
  friend class content::WebContentsUserData<TabAutofillManagerDelegate>;

  // Hides the associated request autocomplete dialog (if it exists).
  void HideRequestAutocompleteDialog();

  content::WebContents* const web_contents_;
  autofill::AutofillDialogControllerImpl* autofill_dialog_controller_;  // weak.

  DISALLOW_COPY_AND_ASSIGN(TabAutofillManagerDelegate);
};

#endif  // CHROME_BROWSER_UI_AUTOFILL_TAB_AUTOFILL_MANAGER_DELEGATE_H_
