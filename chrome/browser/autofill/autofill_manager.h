// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_AUTOFILL_MANAGER_H_
#define CHROME_BROWSER_AUTOFILL_AUTOFILL_MANAGER_H_

#include <vector>

#include "base/scoped_ptr.h"
#include "chrome/browser/autofill/autofill_dialog.h"
#include "chrome/browser/renderer_host/render_view_host_delegate.h"

namespace webkit_glue {
class FormFieldValues;
}

class AutoFillInfoBarDelegate;
class AutoFillProfile;
class CreditCard;
class FormStructure;
class PersonalDataManager;
class PrefService;
class TabContents;

// Manages saving and restoring the user's personal information entered into web
// forms.
class AutoFillManager : public RenderViewHostDelegate::AutoFill,
                               AutoFillDialogObserver {
 public:
  explicit AutoFillManager(TabContents* tab_contents);
  virtual ~AutoFillManager();

  // Registers our Enable/Disable AutoFill pref.
  static void RegisterUserPrefs(PrefService* prefs);

  // RenderViewHostDelegate::AutoFill implementation:
  virtual void FormFieldValuesSubmitted(
      const webkit_glue::FormFieldValues& form);

  // AutoFillDialogObserver implementation:
  virtual void OnAutoFillDialogApply(
      const std::vector<AutoFillProfile>& profiles,
      const std::vector<CreditCard>& credit_cards);

  // Uses heuristics and existing personal data to determine the possible field
  // types.
  void DeterminePossibleFieldTypes(FormStructure* form_structure);

  // Handles the form data submitted by the user.
  void HandleSubmit();

  // Called by the AutoFillInfoBarDelegate when the user accepts the infobar.
  void OnInfoBarAccepted();

  // Saves the form data to the web database.
  void SaveFormData();

  // Uploads the form data to the autofill server.
  void UploadFormData();

  // Resets the stored form data.
  void Reset();

 private:
  // The TabContents hosting this AutoFillManager.
  TabContents* tab_contents_;

  // The personal data manager, used to save and load personal data to/from the
  // web database.
  PersonalDataManager* personal_data_;

  // Our copy of the form data.
  std::vector<FormStructure*> form_structures_;
  scoped_ptr<FormStructure> upload_form_structure_;

  // The infobar that asks for permission to store form information.
  scoped_ptr<AutoFillInfoBarDelegate> infobar_;

  DISALLOW_COPY_AND_ASSIGN(AutoFillManager);
};

#endif  // CHROME_BROWSER_AUTOFILL_AUTOFILL_MANAGER_H_
