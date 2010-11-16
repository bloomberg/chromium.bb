// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_AUTOFILL_MANAGER_H_
#define CHROME_BROWSER_AUTOFILL_AUTOFILL_MANAGER_H_
#pragma once

#include <vector>
#include <string>
#include <list>

#include "base/gtest_prod_util.h"
#include "base/scoped_ptr.h"
#include "base/scoped_vector.h"
#include "chrome/browser/autofill/autofill_dialog.h"
#include "chrome/browser/autofill/autofill_download.h"
#include "chrome/browser/autofill/personal_data_manager.h"
#include "chrome/browser/renderer_host/render_view_host_delegate.h"

class AutoFillCCInfoBarDelegate;
class AutoFillProfile;
class CreditCard;
class FormStructure;
class PrefService;
class TabContents;

namespace webkit_glue {
struct FormData;
class FormField;
}  // namespace webkit_glue

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

  // Returns the TabContents hosting this AutoFillManager.
  TabContents* tab_contents() const { return tab_contents_; }

  // RenderViewHostDelegate::AutoFill implementation:
  virtual void FormSubmitted(const webkit_glue::FormData& form);
  virtual void FormsSeen(const std::vector<webkit_glue::FormData>& forms);
  virtual bool GetAutoFillSuggestions(bool field_autofilled,
                                      const webkit_glue::FormField& field);
  virtual bool FillAutoFillFormData(int query_id,
                                    const webkit_glue::FormData& form,
                                    int unique_id);
  virtual void ShowAutoFillDialog();

  // Called by the AutoFillCCInfoBarDelegate when the user interacts with the
  // infobar.
  virtual void OnInfoBarClosed(bool should_save);

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

 protected:
  // For tests.
  AutoFillManager();
  AutoFillManager(TabContents* tab_contents,
                  PersonalDataManager* personal_data);

  void set_personal_data_manager(PersonalDataManager* personal_data) {
    personal_data_ = personal_data;
  }

 private:
  // Returns a list of values from the stored profiles that match |type| and the
  // value of |field| and returns the labels of the matching profiles. |labels|
  // is filled with the Profile label.
  void GetProfileSuggestions(FormStructure* form,
                             const webkit_glue::FormField& field,
                             AutoFillType type,
                             std::vector<string16>* values,
                             std::vector<string16>* labels,
                             std::vector<string16>* icons,
                             std::vector<int>* unique_ids);

  // Returns a list of values from the stored credit cards that match |type| and
  // the value of |field| and returns the labels of the matching credit cards.
  void GetCreditCardSuggestions(FormStructure* form,
                                const webkit_glue::FormField& field,
                                AutoFillType type,
                                std::vector<string16>* values,
                                std::vector<string16>* labels,
                                std::vector<string16>* icons,
                                std::vector<int>* unique_ids);

  // Set |field| argument's value based on |type| and contents of the
  // |credit_card|.
  void FillCreditCardFormField(const CreditCard* credit_card,
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

  // Parses the forms using heuristic matching and querying the AutoFill server.
  void ParseForms(const std::vector<webkit_glue::FormData>& forms);

  // Methods for packing and unpacking credit card and profile IDs for sending
  // and receiving to and from the renderer process.
  int PackGUIDs(const std::string& cc_guid, const std::string& profile_guid);
  void UnpackGUIDs(int id, std::string* cc_guid, std::string* profile_guid);

  // Maps GUIDs to and from IDs that are used to identify profiles and credit
  // cards sent to and from the renderer process.
  int GUIDToID(const std::string& guid);
  const std::string IDToGUID(int id);

  // The following function is meant to be called from unit-test only.
  void set_disable_download_manager_requests(bool value) {
    disable_download_manager_requests_ = value;
  }

  // The TabContents hosting this AutoFillManager.
  // Weak reference.
  // May not be NULL.
  TabContents* tab_contents_;

  // The personal data manager, used to save and load personal data to/from the
  // web database.  This is overridden by the AutoFillManagerTest.
  // Weak reference.
  // May be NULL.  NULL indicates OTR.
  PersonalDataManager* personal_data_;

  std::list<std::string> autofilled_forms_signatures_;
  // Handles queries and uploads to AutoFill servers.
  AutoFillDownloadManager download_manager_;

  // Should be set to true in AutoFillManagerTest and other tests, false in
  // AutoFillDownloadManagerTest and in non-test environment. Is false by
  // default.
  bool disable_download_manager_requests_;

  // Our copy of the form data.
  ScopedVector<FormStructure> form_structures_;

  // The form data the user has submitted.
  scoped_ptr<FormStructure> upload_form_structure_;

  // The InfoBar that asks for permission to store credit card information.
  // Deletes itself when closed.
  AutoFillCCInfoBarDelegate* cc_infobar_;

  // GUID to ID mapping.  We keep two maps to convert back and forth.
  std::map<std::string, int> guid_id_map_;
  std::map<int, std::string> id_guid_map_;

  friend class TestAutoFillManager;
  FRIEND_TEST_ALL_PREFIXES(AutoFillManagerTest, FillCreditCardForm);
  FRIEND_TEST_ALL_PREFIXES(AutoFillManagerTest, FillAddressForm);
  FRIEND_TEST_ALL_PREFIXES(AutoFillManagerTest, FillAddressAndCreditCardForm);
  FRIEND_TEST_ALL_PREFIXES(AutoFillManagerTest, FillPhoneNumber);
  FRIEND_TEST_ALL_PREFIXES(AutoFillManagerTest, FormChangesRemoveField);
  FRIEND_TEST_ALL_PREFIXES(AutoFillManagerTest, FormChangesAddField);

  DISALLOW_COPY_AND_ASSIGN(AutoFillManager);
};

#endif  // CHROME_BROWSER_AUTOFILL_AUTOFILL_MANAGER_H_
