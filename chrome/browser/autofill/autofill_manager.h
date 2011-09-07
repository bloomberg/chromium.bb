// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_AUTOFILL_MANAGER_H_
#define CHROME_BROWSER_AUTOFILL_AUTOFILL_MANAGER_H_
#pragma once

#include <list>
#include <map>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/string16.h"
#include "base/time.h"
#include "chrome/browser/autofill/autofill_download.h"
#include "chrome/browser/autofill/field_types.h"
#include "chrome/browser/autofill/form_structure.h"
#include "content/browser/tab_contents/tab_contents_observer.h"

class AutofillField;
class AutofillProfile;
class AutofillMetrics;
class CreditCard;
class PersonalDataManager;
class PrefService;
class RenderViewHost;
class TabContentsWrapper;

struct ViewHostMsg_FrameNavigate_Params;

namespace IPC {
class Message;
}

namespace webkit_glue {
struct FormData;
struct FormField;
}

// Manages saving and restoring the user's personal information entered into web
// forms.
class AutofillManager : public TabContentsObserver,
                        public AutofillDownloadManager::Observer {
 public:
  explicit AutofillManager(TabContentsWrapper* tab_contents);
  virtual ~AutofillManager();

  // Registers our Enable/Disable Autofill pref.
  static void RegisterUserPrefs(PrefService* prefs);

  // TabContentsObserver implementation.
  virtual void DidNavigateMainFramePostCommit(
      const content::LoadCommittedDetails& details,
      const ViewHostMsg_FrameNavigate_Params& params);
  virtual bool OnMessageReceived(const IPC::Message& message);

  // AutofillDownloadManager::Observer implementation:
  virtual void OnLoadedServerPredictions(const std::string& response_xml);
  virtual void OnUploadedPossibleFieldTypes();
  virtual void OnServerRequestError(
      const std::string& form_signature,
      AutofillDownloadManager::AutofillRequestType request_type,
      int http_error);

  // Returns the value of the AutofillEnabled pref.
  virtual bool IsAutofillEnabled() const;

  // Imports the form data, submitted by the user, into |personal_data_|.
  void ImportFormData(const FormStructure& submitted_form);

  // Uploads the form data to the Autofill server.
  virtual void UploadFormData(const FormStructure& submitted_form);

  // Reset cache.
  void Reset();

 protected:
  // For tests:

  // The string/int pair is composed of the guid string and variant index
  // respectively.  The variant index is an index into the multi-valued item
  // (where applicable).
  typedef std::pair<std::string, size_t> GUIDPair;

  AutofillManager(TabContentsWrapper* tab_contents,
                  PersonalDataManager* personal_data);

  void set_personal_data_manager(PersonalDataManager* personal_data) {
    personal_data_ = personal_data;
  }

  const AutofillMetrics* metric_logger() const { return metric_logger_.get(); }
  void set_metric_logger(const AutofillMetrics* metric_logger);

  ScopedVector<FormStructure>* form_structures() { return &form_structures_; }

  // Maps GUIDs to and from IDs that are used to identify profiles and credit
  // cards sent to and from the renderer process.
  virtual int GUIDToID(const GUIDPair& guid) const;
  virtual const GUIDPair IDToGUID(int id) const;

  // Methods for packing and unpacking credit card and profile IDs for sending
  // and receiving to and from the renderer process.
  int PackGUIDs(const GUIDPair& cc_guid, const GUIDPair& profile_guid) const;
  void UnpackGUIDs(int id, GUIDPair* cc_guid, GUIDPair* profile_guid) const;

 private:
  void OnFormSubmitted(const webkit_glue::FormData& form,
                       const base::TimeTicks& timestamp);
  void OnFormsSeen(const std::vector<webkit_glue::FormData>& forms,
                   const base::TimeTicks& timestamp);
  void OnTextFieldDidChange(const webkit_glue::FormData& form,
                            const webkit_glue::FormField& field,
                            const base::TimeTicks& timestamp);
  void OnQueryFormFieldAutofill(int query_id,
                                const webkit_glue::FormData& form,
                                const webkit_glue::FormField& field);
  void OnFillAutofillFormData(int query_id,
                              const webkit_glue::FormData& form,
                              const webkit_glue::FormField& field,
                              int unique_id);
  void OnShowAutofillDialog();
  void OnDidPreviewAutofillFormData();
  void OnDidFillAutofillFormData(const base::TimeTicks& timestamp);
  void OnDidShowAutofillSuggestions(bool is_new_popup);

  // Fills |host| with the RenderViewHost for this tab.
  // Returns false if Autofill is disabled or if the host is unavailable.
  bool GetHost(const std::vector<AutofillProfile*>& profiles,
               const std::vector<CreditCard*>& credit_cards,
               RenderViewHost** host) const WARN_UNUSED_RESULT;

  // Unpacks |unique_id| and fills |profile| or |credit_card| with the
  // appropriate data source.  Returns false if the unpacked id cannot be found.
  bool GetProfileOrCreditCard(int unique_id,
                              const std::vector<AutofillProfile*>& profiles,
                              const std::vector<CreditCard*>& credit_cards,
                              const AutofillProfile** profile,
                              const CreditCard** credit_card,
                              size_t* variant) const WARN_UNUSED_RESULT;

  // Fills |form_structure| cached element corresponding to |form|.
  // Returns false if the cached element was not found.
  bool FindCachedForm(const webkit_glue::FormData& form,
                      FormStructure** form_structure) const WARN_UNUSED_RESULT;

  // Fills |form_structure| and |autofill_field| with the cached elements
  // corresponding to |form| and |field|.  This might have the side-effect of
  // updating the cache.  Returns false if the |form| is not autofillable, or if
  // it is not already present in the cache and the cache is full.
  bool GetCachedFormAndField(const webkit_glue::FormData& form,
                             const webkit_glue::FormField& field,
                             FormStructure** form_structure,
                             AutofillField** autofill_field) WARN_UNUSED_RESULT;

  // Re-parses |live_form| and adds the result to |form_structures_|.
  // |cached_form| should be a pointer to the existing version of the form, or
  // NULL if no cached version exists.  The updated form is then written into
  // |updated_form|.
  void UpdateCachedForm(const webkit_glue::FormData& live_form,
                        const FormStructure* cached_form,
                        FormStructure** updated_form);

  // Returns a list of values from the stored profiles that match |type| and the
  // value of |field| and returns the labels of the matching profiles. |labels|
  // is filled with the Profile label.
  void GetProfileSuggestions(FormStructure* form,
                             const webkit_glue::FormField& field,
                             AutofillFieldType type,
                             std::vector<string16>* values,
                             std::vector<string16>* labels,
                             std::vector<string16>* icons,
                             std::vector<int>* unique_ids) const;

  // Returns a list of values from the stored credit cards that match |type| and
  // the value of |field| and returns the labels of the matching credit cards.
  void GetCreditCardSuggestions(FormStructure* form,
                                const webkit_glue::FormField& field,
                                AutofillFieldType type,
                                std::vector<string16>* values,
                                std::vector<string16>* labels,
                                std::vector<string16>* icons,
                                std::vector<int>* unique_ids) const;

  // Set |field|'s value based on |type| and contents of the |credit_card|.
  void FillCreditCardFormField(const CreditCard& credit_card,
                               AutofillFieldType type,
                               webkit_glue::FormField* field);

  // Set |field|'s value based on |cached_field|'s type and contents of the
  // |profile|.  The |variant| parameter specifies which value in a multi-valued
  // profile.
  void FillFormField(const AutofillProfile& profile,
                     const AutofillField& cached_field,
                     size_t variant,
                     webkit_glue::FormField* field);

  // Set |field|'s value for phone/fax number based on contents of the
  // |profile|.  The |cached_field| specifies the type of the phone and whether
  // this is a phone prefix or suffix.  The |variant| parameter specifies which
  // value in a multi-valued profile.
  void FillPhoneNumberField(const AutofillProfile& profile,
                            const AutofillField& cached_field,
                            size_t variant,
                            webkit_glue::FormField* field);

  // Parses the forms using heuristic matching and querying the Autofill server.
  void ParseForms(const std::vector<webkit_glue::FormData>& forms);

  // Uses existing personal data to determine possible field types for the
  // |submitted_form|.
  void DeterminePossibleFieldTypesForUpload(FormStructure* submitted_form);

  // If |initial_interaction_timestamp_| is unset or is set to a later time than
  // |interaction_timestamp|, updates the cached timestamp.  The latter check is
  // needed because IPC messages can arrive out of order.
  void UpdateInitialInteractionTimestamp(
      const base::TimeTicks& interaction_timestamp);

  // Send our current field type predictions to the renderer.
  // This is a no-op if the appropriate command-line flag is not set.
  void SendAutofillTypePredictions(
      const std::vector<FormStructure*>& forms) const;

  // The owning TabContentsWrapper.
  TabContentsWrapper* tab_contents_wrapper_;

  // The personal data manager, used to save and load personal data to/from the
  // web database.  This is overridden by the AutofillManagerTest.
  // Weak reference.
  // May be NULL.  NULL indicates OTR.
  PersonalDataManager* personal_data_;

  std::list<std::string> autofilled_form_signatures_;
  // Handles queries and uploads to Autofill servers.
  AutofillDownloadManager download_manager_;

  // Should be set to true in AutofillManagerTest and other tests, false in
  // AutofillDownloadManagerTest and in non-test environment. Is false by
  // default for the public constructor, and true by default for the test-only
  // constructors.
  bool disable_download_manager_requests_;

  // For logging UMA metrics. Overridden by metrics tests.
  scoped_ptr<const AutofillMetrics> metric_logger_;
  // Have we logged whether Autofill is enabled for this page load?
  bool has_logged_autofill_enabled_;
  // Have we logged an address suggestions count metric for this page?
  bool has_logged_address_suggestions_count_;
  // Have we shown Autofill suggestions at least once?
  bool did_show_suggestions_;
  // Has the user manually edited at least one form field among the autofillable
  // ones?
  bool user_did_type_;
  // Has the user autofilled a form on this page?
  bool user_did_autofill_;
  // Has the user edited a field that was previously autofilled?
  bool user_did_edit_autofilled_field_;
  // When the page finished loading.
  base::TimeTicks forms_loaded_timestamp_;
  // When the user first interacted with a potentially fillable form on this
  // page.
  base::TimeTicks initial_interaction_timestamp_;

  // Our copy of the form data.
  ScopedVector<FormStructure> form_structures_;

  // GUID to ID mapping.  We keep two maps to convert back and forth.
  mutable std::map<GUIDPair, int> guid_id_map_;
  mutable std::map<int, GUIDPair> id_guid_map_;

  friend class AutofillManagerTest;
  friend class FormStructureBrowserTest;
  FRIEND_TEST_ALL_PREFIXES(AutofillManagerTest, FillCreditCardForm);
  FRIEND_TEST_ALL_PREFIXES(AutofillManagerTest,
                           FillCreditCardFormNoYearNoMonth);
  FRIEND_TEST_ALL_PREFIXES(AutofillManagerTest, FillCreditCardFormYearNoMonth);
  FRIEND_TEST_ALL_PREFIXES(AutofillManagerTest, FillCreditCardFormNoYearMonth);
  FRIEND_TEST_ALL_PREFIXES(AutofillManagerTest, FillCreditCardFormYearMonth);
  FRIEND_TEST_ALL_PREFIXES(AutofillManagerTest, FillAddressForm);
  FRIEND_TEST_ALL_PREFIXES(AutofillManagerTest, FillAddressAndCreditCardForm);
  FRIEND_TEST_ALL_PREFIXES(AutofillManagerTest, FillFormWithMultipleSections);
  FRIEND_TEST_ALL_PREFIXES(AutofillManagerTest,
                           FillFormWithAuthorSpecifiedSections);
  FRIEND_TEST_ALL_PREFIXES(AutofillManagerTest, FillFormWithMultipleEmails);
  FRIEND_TEST_ALL_PREFIXES(AutofillManagerTest, FillAutofilledForm);
  FRIEND_TEST_ALL_PREFIXES(AutofillManagerTest, FillPhoneNumber);
  FRIEND_TEST_ALL_PREFIXES(AutofillManagerTest, FormChangesRemoveField);
  FRIEND_TEST_ALL_PREFIXES(AutofillManagerTest, FormChangesAddField);
  FRIEND_TEST_ALL_PREFIXES(AutofillManagerTest, FormSubmitted);
  FRIEND_TEST_ALL_PREFIXES(AutofillManagerTest, FormSubmittedServerTypes);
  FRIEND_TEST_ALL_PREFIXES(AutofillManagerTest, FormSubmittedWithDefaultValues);
  FRIEND_TEST_ALL_PREFIXES(AutofillManagerTest,
                           DeterminePossibleFieldTypesForUpload);
  FRIEND_TEST_ALL_PREFIXES(AutofillManagerTest,
                           DeterminePossibleFieldTypesForUploadStressTest);
  FRIEND_TEST_ALL_PREFIXES(AutofillMetricsTest, AddressSuggestionsCount);
  FRIEND_TEST_ALL_PREFIXES(AutofillMetricsTest, AutofillIsEnabledAtPageLoad);
  FRIEND_TEST_ALL_PREFIXES(AutofillMetricsTest,
                           NoQualityMetricsForNonAutofillableForms);
  FRIEND_TEST_ALL_PREFIXES(AutofillMetricsTest, QualityMetrics);
  FRIEND_TEST_ALL_PREFIXES(AutofillMetricsTest, QualityMetricsForFailure);
  FRIEND_TEST_ALL_PREFIXES(AutofillMetricsTest, QualityMetricsWithExperimentId);
  FRIEND_TEST_ALL_PREFIXES(AutofillMetricsTest, SaneMetricsWithCacheMismatch);
  FRIEND_TEST_ALL_PREFIXES(AutofillMetricsTest,
                           UserHappinessFormLoadAndSubmission);
  FRIEND_TEST_ALL_PREFIXES(AutofillMetricsTest, UserHappinessFormInteraction);
  FRIEND_TEST_ALL_PREFIXES(AutofillMetricsTest, FormFillDuration);

  DISALLOW_COPY_AND_ASSIGN(AutofillManager);
};

#endif  // CHROME_BROWSER_AUTOFILL_AUTOFILL_MANAGER_H_
