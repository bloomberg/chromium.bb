// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_AUTOFILL_MANAGER_H_
#define CHROME_BROWSER_AUTOFILL_AUTOFILL_MANAGER_H_

#include <vector>

#include "base/scoped_ptr.h"
#include "base/scoped_vector.h"
#include "chrome/browser/autofill/autofill_dialog.h"
#include "chrome/browser/autofill/personal_data_manager.h"
#include "chrome/browser/renderer_host/render_view_host_delegate.h"

namespace webkit_glue {
struct FormData;
class FormField;
class FormFieldValues;
}

class AutoFillInfoBarDelegate;
class AutoFillProfile;
class CreditCard;
class FormStructure;
class PrefService;
class TabContents;

// Manages saving and restoring the user's personal information entered into web
// forms.
class AutoFillManager : public RenderViewHostDelegate::AutoFill,
                        public AutoFillDialogObserver,
                        public PersonalDataManager::Observer {
 public:
  explicit AutoFillManager(TabContents* tab_contents);
  virtual ~AutoFillManager();

  // Registers our browser prefs.
  static void RegisterBrowserPrefs(PrefService* prefs);

  // Registers our Enable/Disable AutoFill pref.
  static void RegisterUserPrefs(PrefService* prefs);

  // RenderViewHostDelegate::AutoFill implementation:
  virtual void FormFieldValuesSubmitted(
      const webkit_glue::FormFieldValues& form);
  virtual void FormsSeen(
      const std::vector<webkit_glue::FormFieldValues>& forms);
  virtual bool GetAutoFillSuggestions(int query_id,
                                      const webkit_glue::FormField& field);
  virtual bool FillAutoFillFormData(int query_id,
                                    const webkit_glue::FormData& form,
                                    const string16& name,
                                    const string16& label);

  // AutoFillDialogObserver implementation:
  virtual void OnAutoFillDialogApply(
      std::vector<AutoFillProfile>* profiles,
      std::vector<CreditCard>* credit_cards);

  // PersonalDataManager::Observer implementation:
  virtual void OnPersonalDataLoaded();

  // Called by the AutoFillInfoBarDelegate when the user closes the infobar.
  virtual void OnInfoBarClosed();

  // Called by the AutoFillInfoBarDelegate when the user accepts the infobar.
  virtual void OnInfoBarAccepted();

  // Called by the AutoFillInfoBarDelegate when the user cancels the infobar.
  virtual void OnInfoBarCancelled();

  // Resets the stored form data.
  virtual void Reset();

  // Uses heuristics and existing personal data to determine the possible field
  // types.
  void DeterminePossibleFieldTypes(FormStructure* form_structure);

  // Handles the form data submitted by the user.
  void HandleSubmit();


  // Uploads the form data to the autofill server.
  void UploadFormData();

  // Returns the value of the AutoFillEnabled pref.
  bool IsAutoFillEnabled();

 protected:
  // For AutoFillInfoBarDelegateTest.
  AutoFillManager();

 private:
  // The TabContents hosting this AutoFillManager.
  // Weak reference.
  // May not be NULL.
  TabContents* tab_contents_;

  // The personal data manager, used to save and load personal data to/from the
  // web database.
  // Weak reference.
  // May be NULL.  NULL indicates OTR.
  PersonalDataManager* personal_data_;

  // Our copy of the form data.
  ScopedVector<FormStructure> form_structures_;

  // The form data the user has submitted.
  scoped_ptr<FormStructure> upload_form_structure_;

  // The infobar that asks for permission to store form information.
  scoped_ptr<AutoFillInfoBarDelegate> infobar_;

  DISALLOW_COPY_AND_ASSIGN(AutoFillManager);
};

#endif  // CHROME_BROWSER_AUTOFILL_AUTOFILL_MANAGER_H_
