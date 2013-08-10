// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_DIALOG_CONTROLLER_H_
#define CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_DIALOG_CONTROLLER_H_

#include <string>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"
#include "chrome/browser/ui/autofill/autofill_dialog_types.h"
#include "components/autofill/content/browser/autocheckout_steps.h"
#include "components/autofill/core/browser/form_structure.h"

class GURL;

namespace content {
class WebContents;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace autofill {

// This class defines the interface to the controller for TabAutofillManager.
class AutofillDialogController {
 public:
  virtual ~AutofillDialogController();

  // Creates the AutofillDialogController.
  static base::WeakPtr<AutofillDialogController> Create(
      content::WebContents* contents,
      const FormData& form_structure,
      const GURL& source_url,
      const DialogType dialog_type,
      const base::Callback<void(const FormStructure*,
                                const std::string&)>& callback);

  // Registers profile preferences.
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  // Shows the Autofill dialog.
  virtual void Show() = 0;

  // Hides the Autofill dialog.
  virtual void Hide() = 0;

  // Called when the tab hosting this dialog is activated by a user gesture.
  // Used to trigger a refresh of the user's Wallet data.
  virtual void TabActivated() = 0;

  // Adds a step in the flow to the Autocheckout UI.
  virtual void AddAutocheckoutStep(AutocheckoutStepType step_type) = 0;

  // Updates the status of a step in the Autocheckout UI.
  virtual void UpdateAutocheckoutStep(
      AutocheckoutStepType step_type,
      AutocheckoutStepStatus step_status) = 0;

  // Called when there is an error in an active Autocheckout flow.
  virtual void OnAutocheckoutError() = 0;

  // Called when an Autocheckout flow completes successfully.
  virtual void OnAutocheckoutSuccess() = 0;

  // Returns the dialog type.
  virtual DialogType GetDialogType() const = 0;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_DIALOG_CONTROLLER_H_
