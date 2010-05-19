// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_AUTOFILL_MANAGER_H_
#define CHROME_BROWSER_AUTOFILL_AUTOFILL_MANAGER_H_

#include <vector>
#include <string>

#include "base/scoped_ptr.h"
#include "base/scoped_vector.h"
#include "chrome/browser/autofill/autofill_dialog.h"
#include "chrome/browser/autofill/autofill_download.h"
#include "chrome/browser/autofill/personal_data_manager.h"
#include "chrome/browser/renderer_host/render_view_host_delegate.h"

class AutoFillInfoBarDelegate;
class AutoFillProfile;
class CreditCard;
class FormStructure;
class PrefService;
class TabContents;

namespace webkit_glue {
struct FormData;
class FormField;
}  // namespace webkit_glue

// TODO(jhawkins): Maybe this should be in a grd file?
extern const char* kAutoFillLearnMoreUrl;

// Manages saving and restoring the user's personal information entered into web
// forms.
class AutoFillManager : public RenderViewHostDelegate::AutoFill,
                        public AutoFillDownloadManager::Observer {
 public:
  explicit AutoFillManager(TabContents* tab_contents);
  virtual ~AutoFillManager();

  // Registers our browser prefs.
  static void RegisterBrowserPrefs(PrefService* prefs);

  // Registers our Enable/Disable AutoFill pref.
  static void RegisterUserPrefs(PrefService* prefs);

  // RenderViewHostDelegate::AutoFill implementation:
  virtual void FormSubmitted(const webkit_glue::FormData& form);
  virtual void FormsSeen(
      const std::vector<webkit_glue::FormData>& forms);
  virtual bool GetAutoFillSuggestions(int query_id,
                                      const webkit_glue::FormField& field);
  virtual bool FillAutoFillFormData(int query_id,
                                    const webkit_glue::FormData& form,
                                    const string16& value,
                                    const string16& label);

  // Called by the AutoFillInfoBarDelegate when the user closes the infobar.
  virtual void OnInfoBarClosed();

  // Called by the AutoFillInfoBarDelegate when the user accepts the infobar.
  virtual void OnInfoBarAccepted();

  // Called by the AutoFillInfoBarDelegate when the user cancels the infobar.
  virtual void OnInfoBarCancelled();

  // Resets the stored form data.
  virtual void Reset();

  // AutoFillDownloadManager::Observer implementation:
  virtual void OnLoadedAutoFillHeuristics(const std::string& heuristic_xml);
  virtual void OnUploadedAutoFillHeuristics(const std::string& form_signature);
  virtual void OnHeuristicsRequestError(
      const std::string& form_signature,
      AutoFillDownloadManager::AutoFillRequestType request_type,
      int http_error);

  // Returns the value of the AutoFillEnabled pref.
  virtual bool IsAutoFillEnabled() const;

  // Uses heuristics and existing personal data to determine the possible field
  // types.
  void DeterminePossibleFieldTypes(FormStructure* form_structure);

  // Handles the form data submitted by the user.
  void HandleSubmit();

  // Uploads the form data to the AutoFill server.
  void UploadFormData();

  // Fills all the forms in the page with the default profile.  Credit card
  // fields are not filled out.
  // TODO(jhawkins): Do we really want to fill all of the forms on the page?
  // Check how toolbar handles this case.
  void FillDefaultProfile();

 protected:
  // For tests.
  AutoFillManager();
  AutoFillManager(TabContents* tab_contents,
                  PersonalDataManager* personal_data);

 private:
  // Returns a list of values from the stored profiles that match |type| and the
  // value of |field| and returns the labels of the matching profiles.
  void GetProfileSuggestions(const webkit_glue::FormField& field,
                             AutoFillFieldType type,
                             std::vector<string16>* values,
                             std::vector<string16>* labels);

  // Returns a list of values from the stored credit cards that match |type| and
  // the value of |field| and returns the labels of the matching credit cards.
  void GetCreditCardSuggestions(const webkit_glue::FormField& field,
                                AutoFillFieldType type,
                                std::vector<string16>* values,
                                std::vector<string16>* labels);

  // Set |field| argument's value based on |type| and contents of the
  // |credit_card|.  The |type| field is expected to have main group type of
  // ADDRESS_BILLING.  The address information is retrieved from the billing
  // profile asscociated with the |credit_card|, if there is one set.
  void FillBillingFormField(const CreditCard* credit_card,
                            AutoFillType type,
                            webkit_glue::FormField* field);

  // Set |field| argument's value based on |type| and contents of the |profile|.
  void FillFormField(const AutoFillProfile* profile,
                     AutoFillType type,
                     webkit_glue::FormField* field);

  // Set |field| argument's value for phone number based on contents of the
  // |profile|.
  void FillPhoneNumberField(const AutoFillProfile* profile,
                            webkit_glue::FormField* field);

  // The TabContents hosting this AutoFillManager.
  // Weak reference.
  // May not be NULL.
  TabContents* tab_contents_;

  // The personal data manager, used to save and load personal data to/from the
  // web database.  This is overridden by the AutoFillManagerTest.
  // Weak reference.
  // May be NULL.  NULL indicates OTR.
  PersonalDataManager* personal_data_;

  // Handles queries and uploads to AutoFill servers.
  AutoFillDownloadManager download_manager_;

  // Our copy of the form data.
  ScopedVector<FormStructure> form_structures_;

  // The form data the user has submitted.
  scoped_ptr<FormStructure> upload_form_structure_;

  // The InfoBar that asks for permission to store form information.
  scoped_ptr<AutoFillInfoBarDelegate> infobar_;

  DISALLOW_COPY_AND_ASSIGN(AutoFillManager);
};

#endif  // CHROME_BROWSER_AUTOFILL_AUTOFILL_MANAGER_H_
