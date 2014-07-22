// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_MANAGER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_MANAGER_H_

#include <list>
#include <map>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/callback_forward.h"
#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/autocomplete_history_manager.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/autofill_download_manager.h"
#include "components/autofill/core/browser/autofill_driver.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/common/form_data.h"

class GURL;

namespace content {
class RenderViewHost;
class WebContents;
}

namespace gfx {
class Rect;
class RectF;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace autofill {

class AutofillDataModel;
class AutofillDownloadManager;
class AutofillExternalDelegate;
class AutofillField;
class AutofillClient;
class AutofillManagerTestDelegate;
class AutofillMetrics;
class AutofillProfile;
class AutofillType;
class CreditCard;
class FormStructureBrowserTest;

struct FormData;
struct FormFieldData;
struct PasswordFormFillData;

// Manages saving and restoring the user's personal information entered into web
// forms.
class AutofillManager : public AutofillDownloadManager::Observer {
 public:
  enum AutofillDownloadManagerState {
    ENABLE_AUTOFILL_DOWNLOAD_MANAGER,
    DISABLE_AUTOFILL_DOWNLOAD_MANAGER,
  };

  // Registers our Enable/Disable Autofill pref.
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

#if defined(OS_MACOSX) && !defined(OS_IOS)
  static void MigrateUserPrefs(PrefService* prefs);
#endif  // defined(OS_MACOSX) && !defined(OS_IOS)

  AutofillManager(AutofillDriver* driver,
                  AutofillClient* client,
                  const std::string& app_locale,
                  AutofillDownloadManagerState enable_download_manager);
  virtual ~AutofillManager();

  // Sets an external delegate.
  void SetExternalDelegate(AutofillExternalDelegate* delegate);

  void ShowAutofillSettings();

#if defined(OS_MACOSX) && !defined(OS_IOS)
  // Whether the field represented by |fieldData| should show an entry to prompt
  // the user to give Chrome access to the user's address book.
  bool ShouldShowAccessAddressBookSuggestion(const FormData& data,
                                             const FormFieldData& field_data);

  // If Chrome has not prompted for access to the user's address book, the
  // method prompts the user for permission and blocks the process. Otherwise,
  // this method has no effect. The return value reflects whether the user was
  // prompted with a modal dialog.
  bool AccessAddressBook();
#endif  // defined(OS_MACOSX) && !defined(OS_IOS)

  // Called from our external delegate so they cannot be private.
  virtual void FillOrPreviewForm(AutofillDriver::RendererFormDataAction action,
                                 int query_id,
                                 const FormData& form,
                                 const FormFieldData& field,
                                 int unique_id);
  void DidShowSuggestions(bool is_new_popup);
  void OnDidFillAutofillFormData(const base::TimeTicks& timestamp);
  void OnDidPreviewAutofillFormData();

  // Remove the credit card or Autofill profile that matches |unique_id|
  // from the database.
  void RemoveAutofillProfileOrCreditCard(int unique_id);

  // Remove the specified Autocomplete entry.
  void RemoveAutocompleteEntry(const base::string16& name,
                               const base::string16& value);

  // Returns the present form structures seen by Autofill manager.
  const std::vector<FormStructure*>& GetFormStructures();

  // Happens when the autocomplete dialog runs its callback when being closed.
  void RequestAutocompleteDialogClosed();

  AutofillClient* client() const { return client_; }

  const std::string& app_locale() const { return app_locale_; }

  // Only for testing.
  void SetTestDelegate(AutofillManagerTestDelegate* delegate);

  void OnFormsSeen(const std::vector<FormData>& forms,
                   const base::TimeTicks& timestamp);

  // Processes the submitted |form|, saving any new Autofill data and uploading
  // the possible field types for the submitted fields to the crowdsourcing
  // server.  Returns false if this form is not relevant for Autofill.
  bool OnFormSubmitted(const FormData& form,
                       const base::TimeTicks& timestamp);

  void OnTextFieldDidChange(const FormData& form,
                            const FormFieldData& field,
                            const base::TimeTicks& timestamp);

  // The |bounding_box| is a window relative value.
  void OnQueryFormFieldAutofill(int query_id,
                                const FormData& form,
                                const FormFieldData& field,
                                const gfx::RectF& bounding_box,
                                bool display_warning);
  void OnDidEndTextFieldEditing();
  void OnHidePopup();
  void OnSetDataList(const std::vector<base::string16>& values,
                     const std::vector<base::string16>& labels);

  // Try and upload |form|. This differs from OnFormSubmitted() in a few ways.
  //   - This function will only label the first <input type="password"> field
  //     as ACCOUNT_CREATION_PASSWORD. Other fields will stay unlabeled, as they
  //     should have been labeled during the upload for OnFormSubmitted().
  //   - This function does not assume that |form| is being uploaded during
  //     the same browsing session as it was originally submitted (as we may
  //     not have the necessary information to classify the form at that time)
  //     so it bypasses the cache and doesn't log the same quality UMA metrics.
  bool UploadPasswordGenerationForm(const FormData& form);

  // Resets cache.
  virtual void Reset();

  // Returns the value of the AutofillEnabled pref.
  virtual bool IsAutofillEnabled() const;

 protected:
  // Test code should prefer to use this constructor.
  AutofillManager(AutofillDriver* driver,
                  AutofillClient* client,
                  PersonalDataManager* personal_data);

  // Uploads the form data to the Autofill server.
  virtual void UploadFormData(const FormStructure& submitted_form);

  // Logs quality metrics for the |submitted_form| and uploads the form data
  // to the crowdsourcing server, if appropriate.
  virtual void UploadFormDataAsyncCallback(
      const FormStructure* submitted_form,
      const base::TimeTicks& load_time,
      const base::TimeTicks& interaction_time,
      const base::TimeTicks& submission_time);

  // Maps GUIDs to and from IDs that are used to identify profiles and credit
  // cards sent to and from the renderer process.
  virtual int GUIDToID(const PersonalDataManager::GUIDPair& guid) const;
  virtual const PersonalDataManager::GUIDPair IDToGUID(int id) const;

  // Methods for packing and unpacking credit card and profile IDs for sending
  // and receiving to and from the renderer process.
  int PackGUIDs(const PersonalDataManager::GUIDPair& cc_guid,
                const PersonalDataManager::GUIDPair& profile_guid) const;
  void UnpackGUIDs(int id,
                   PersonalDataManager::GUIDPair* cc_guid,
                   PersonalDataManager::GUIDPair* profile_guid) const;

  const AutofillMetrics* metric_logger() const { return metric_logger_.get(); }
  void set_metric_logger(const AutofillMetrics* metric_logger);

  ScopedVector<FormStructure>* form_structures() { return &form_structures_; }

  // Exposed for testing.
  AutofillExternalDelegate* external_delegate() {
    return external_delegate_;
  }

 private:
  // AutofillDownloadManager::Observer:
  virtual void OnLoadedServerPredictions(
      const std::string& response_xml) OVERRIDE;

  // Returns false if Autofill is disabled or if no Autofill data is available.
  bool RefreshDataModels() const;

  // Unpacks |unique_id| and fills |form_group| and |variant| with the
  // appropriate data source and variant index. Sets |is_credit_card| to true
  // if |data_model| points to a CreditCard data model, false if it's a
  // profile data model.
  // Returns false if the unpacked id cannot be found.
  bool GetProfileOrCreditCard(int unique_id,
                              const AutofillDataModel** data_model,
                              size_t* variant,
                              bool* is_credit_card) const WARN_UNUSED_RESULT;

  // Fills |form_structure| cached element corresponding to |form|.
  // Returns false if the cached element was not found.
  bool FindCachedForm(const FormData& form,
                      FormStructure** form_structure) const WARN_UNUSED_RESULT;

  // Fills |form_structure| and |autofill_field| with the cached elements
  // corresponding to |form| and |field|.  This might have the side-effect of
  // updating the cache.  Returns false if the |form| is not autofillable, or if
  // it is not already present in the cache and the cache is full.
  bool GetCachedFormAndField(const FormData& form,
                             const FormFieldData& field,
                             FormStructure** form_structure,
                             AutofillField** autofill_field) WARN_UNUSED_RESULT;

  // Re-parses |live_form| and adds the result to |form_structures_|.
  // |cached_form| should be a pointer to the existing version of the form, or
  // NULL if no cached version exists.  The updated form is then written into
  // |updated_form|.  Returns false if the cache could not be updated.
  bool UpdateCachedForm(const FormData& live_form,
                        const FormStructure* cached_form,
                        FormStructure** updated_form) WARN_UNUSED_RESULT;

  // Returns a list of values from the stored profiles that match |type| and the
  // value of |field| and returns the labels of the matching profiles. |labels|
  // is filled with the Profile label.
  void GetProfileSuggestions(FormStructure* form,
                             const FormFieldData& field,
                             const AutofillType& type,
                             std::vector<base::string16>* values,
                             std::vector<base::string16>* labels,
                             std::vector<base::string16>* icons,
                             std::vector<int>* unique_ids) const;

  // Returns a list of values from the stored credit cards that match |type| and
  // the value of |field| and returns the labels of the matching credit cards.
  void GetCreditCardSuggestions(const FormFieldData& field,
                                const AutofillType& type,
                                std::vector<base::string16>* values,
                                std::vector<base::string16>* labels,
                                std::vector<base::string16>* icons,
                                std::vector<int>* unique_ids) const;

  // Parses the forms using heuristic matching and querying the Autofill server.
  void ParseForms(const std::vector<FormData>& forms);

  // Imports the form data, submitted by the user, into |personal_data_|.
  void ImportFormData(const FormStructure& submitted_form);

  // If |initial_interaction_timestamp_| is unset or is set to a later time than
  // |interaction_timestamp|, updates the cached timestamp.  The latter check is
  // needed because IPC messages can arrive out of order.
  void UpdateInitialInteractionTimestamp(
      const base::TimeTicks& interaction_timestamp);

  // Shared code to determine if |form| should be uploaded.
  bool ShouldUploadForm(const FormStructure& form);

  // Provides driver-level context to the shared code of the component. Must
  // outlive this object.
  AutofillDriver* driver_;

  AutofillClient* const client_;

  std::string app_locale_;

  // The personal data manager, used to save and load personal data to/from the
  // web database.  This is overridden by the AutofillManagerTest.
  // Weak reference.
  // May be NULL.  NULL indicates OTR.
  PersonalDataManager* personal_data_;

  std::list<std::string> autofilled_form_signatures_;

  // Handles queries and uploads to Autofill servers. Will be NULL if
  // the download manager functionality is disabled.
  scoped_ptr<AutofillDownloadManager> download_manager_;

  // Handles single-field autocomplete form data.
  scoped_ptr<AutocompleteHistoryManager> autocomplete_history_manager_;

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
  // When the form finished loading.
  std::map<FormData, base::TimeTicks> forms_loaded_timestamps_;
  // When the user first interacted with a potentially fillable form on this
  // page.
  base::TimeTicks initial_interaction_timestamp_;

  // Our copy of the form data.
  ScopedVector<FormStructure> form_structures_;

  // GUID to ID mapping.  We keep two maps to convert back and forth.
  mutable std::map<PersonalDataManager::GUIDPair, int> guid_id_map_;
  mutable std::map<int, PersonalDataManager::GUIDPair> id_guid_map_;

  // Delegate to perform external processing (display, selection) on
  // our behalf.  Weak.
  AutofillExternalDelegate* external_delegate_;

  // Delegate used in test to get notifications on certain events.
  AutofillManagerTestDelegate* test_delegate_;

  base::WeakPtrFactory<AutofillManager> weak_ptr_factory_;

  friend class AutofillManagerTest;
  friend class FormStructureBrowserTest;
  FRIEND_TEST_ALL_PREFIXES(AutofillManagerTest,
                           DeterminePossibleFieldTypesForUpload);
  FRIEND_TEST_ALL_PREFIXES(AutofillManagerTest,
                           DeterminePossibleFieldTypesForUploadStressTest);
  FRIEND_TEST_ALL_PREFIXES(AutofillManagerTest,
                           DisabledAutofillDispatchesError);
  FRIEND_TEST_ALL_PREFIXES(AutofillMetricsTest, AddressSuggestionsCount);
  FRIEND_TEST_ALL_PREFIXES(AutofillMetricsTest, AutofillIsEnabledAtPageLoad);
  FRIEND_TEST_ALL_PREFIXES(AutofillMetricsTest, DeveloperEngagement);
  FRIEND_TEST_ALL_PREFIXES(AutofillMetricsTest, FormFillDuration);
  FRIEND_TEST_ALL_PREFIXES(AutofillMetricsTest,
                           NoQualityMetricsForNonAutofillableForms);
  FRIEND_TEST_ALL_PREFIXES(AutofillMetricsTest, QualityMetrics);
  FRIEND_TEST_ALL_PREFIXES(AutofillMetricsTest, QualityMetricsForFailure);
  FRIEND_TEST_ALL_PREFIXES(AutofillMetricsTest, QualityMetricsWithExperimentId);
  FRIEND_TEST_ALL_PREFIXES(AutofillMetricsTest, SaneMetricsWithCacheMismatch);
  FRIEND_TEST_ALL_PREFIXES(AutofillManagerTest, TestExternalDelegate);
  FRIEND_TEST_ALL_PREFIXES(AutofillManagerTest,
                           TestTabContentsWithExternalDelegate);
  FRIEND_TEST_ALL_PREFIXES(AutofillMetricsTest,
                           UserHappinessFormLoadAndSubmission);
  FRIEND_TEST_ALL_PREFIXES(AutofillMetricsTest, UserHappinessFormInteraction);
  FRIEND_TEST_ALL_PREFIXES(AutofillManagerTest,
                           FormSubmittedAutocompleteEnabled);
  DISALLOW_COPY_AND_ASSIGN(AutofillManager);
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_MANAGER_H_
