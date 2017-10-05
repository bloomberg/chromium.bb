// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_MANAGER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_MANAGER_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/compiler_specific.h"
#include "base/containers/circular_deque.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/autocomplete_history_manager.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/autofill_download_manager.h"
#include "components/autofill/core/browser/autofill_driver.h"
#include "components/autofill/core/browser/autofill_handler.h"
#include "components/autofill/core/browser/autofill_metrics.h"
#include "components/autofill/core/browser/card_unmask_delegate.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/form_types.h"
#include "components/autofill/core/browser/payments/full_card_request.h"
#include "components/autofill/core/browser/payments/payments_client.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/common/form_data.h"

#if defined(OS_ANDROID) || defined(OS_IOS)
#include "components/autofill/core/browser/autofill_assistant.h"
#endif

// This define protects some debugging code (see DumpAutofillData). This
// is here to make it easier to delete this code when the test is complete,
// and to prevent adding the code on mobile where there is no desktop (the
// debug dump file is written to the desktop) or command-line flags to enable.
#if defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_LINUX)
#define ENABLE_FORM_DEBUG_DUMP
#endif

namespace gfx {
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
class AutofillProfile;
class AutofillType;
class CreditCard;
class FormStructureBrowserTest;

struct FormData;
struct FormFieldData;

// We show the credit card signin promo only a certain number of times.
extern const int kCreditCardSigninPromoImpressionLimit;

// Manages saving and restoring the user's personal information entered into web
// forms. One per frame; owned by the AutofillDriver.
class AutofillManager : public AutofillHandler,
                        public AutofillDownloadManager::Observer,
                        public payments::PaymentsClientDelegate,
                        public payments::FullCardRequest::ResultDelegate,
                        public payments::FullCardRequest::UIDelegate {
 public:
  // Registers our Enable/Disable Autofill pref.
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  AutofillManager(AutofillDriver* driver,
                  AutofillClient* client,
                  const std::string& app_locale,
                  AutofillDownloadManagerState enable_download_manager);
  ~AutofillManager() override;

  // Sets an external delegate.
  void SetExternalDelegate(AutofillExternalDelegate* delegate);

  void ShowAutofillSettings();

  // Whether the |field| should show an entry to scan a credit card.
  virtual bool ShouldShowScanCreditCard(const FormData& form,
                                        const FormFieldData& field);

  // Whether the |field| belongs to CREDIT_CARD |FieldTypeGroup|.
  virtual bool IsCreditCardPopup(const FormData& form,
                                 const FormFieldData& field);

  // Whether we should show the signin promo, based on the triggered |field|
  // inside the |form|.
  virtual bool ShouldShowCreditCardSigninPromo(const FormData& form,
                                               const FormFieldData& field);

  // Called from our external delegate so they cannot be private.
  virtual void FillOrPreviewForm(AutofillDriver::RendererFormDataAction action,
                                 int query_id,
                                 const FormData& form,
                                 const FormFieldData& field,
                                 int unique_id);
  virtual void FillCreditCardForm(int query_id,
                                  const FormData& form,
                                  const FormFieldData& field,
                                  const CreditCard& credit_card,
                                  const base::string16& cvc);
  void DidShowSuggestions(bool is_new_popup,
                          const FormData& form,
                          const FormFieldData& field);

  // Returns true if the value/identifier is deletable. Fills out
  // |title| and |body| with relevant user-facing text.
  bool GetDeletionConfirmationText(const base::string16& value,
                                   int identifier,
                                   base::string16* title,
                                   base::string16* body);

  // Remove the credit card or Autofill profile that matches |unique_id|
  // from the database. Returns true if deletion is allowed.
  bool RemoveAutofillProfileOrCreditCard(int unique_id);

  // Remove the specified Autocomplete entry.
  void RemoveAutocompleteEntry(const base::string16& name,
                               const base::string16& value);

  // Returns true when the Payments card unmask prompt is being displayed.
  bool IsShowingUnmaskPrompt();

  // Returns the present form structures seen by Autofill manager.
  const std::vector<std::unique_ptr<FormStructure>>& GetFormStructures();

  AutofillClient* client() { return client_; }

  AutofillDownloadManager* download_manager() {
    return download_manager_.get();
  }

  payments::FullCardRequest* GetOrCreateFullCardRequest();

  payments::FullCardRequest* CreateFullCardRequest(
      const base::TimeTicks& form_parsed_timestamp);

  base::WeakPtr<payments::FullCardRequest::UIDelegate>
  GetAsFullCardRequestUIDelegate() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  const std::string& app_locale() const { return app_locale_; }

  // Only for testing.
  void SetTestDelegate(AutofillManagerTestDelegate* delegate);

  void set_app_locale(std::string app_locale) { app_locale_ = app_locale; }

  // Will send an upload based on the |form_structure| data and the local
  // Autofill profile data. |observed_submission| is specified if the upload
  // follows an observed submission event.
  virtual void StartUploadProcess(std::unique_ptr<FormStructure> form_structure,
                                  const base::TimeTicks& timestamp,
                                  bool observed_submission);

  // Update the pending form with |form|, possibly processing the current
  // pending form for upload.
  void UpdatePendingForm(const FormData& form);

  // Upload the current pending form.
  void ProcessPendingFormForUpload();

  // AutofillHandler:
  void OnFocusNoLongerOnForm() override;
  void OnDidFillAutofillFormData(const FormData& form,
                                 const base::TimeTicks timestamp) override;
  void OnDidPreviewAutofillFormData() override;
  void OnFormsSeen(const std::vector<FormData>& forms,
                   const base::TimeTicks timestamp) override;
  bool OnFormSubmitted(const FormData& form) override;
  void OnDidEndTextFieldEditing() override;
  void OnHidePopup() override;
  void OnSetDataList(const std::vector<base::string16>& values,
                     const std::vector<base::string16>& labels) override;
  void Reset() override;

  // Returns true if the value of the AutofillEnabled pref is true and the
  // client supports Autofill.
  virtual bool IsAutofillEnabled() const;

  // Returns true if all the conditions for enabling the upload of credit card
  // are satisfied.
  virtual bool IsCreditCardUploadEnabled();

  // Shared code to determine if |form| should be uploaded to the Autofill
  // server. It verifies that uploading is allowed and |form| meets conditions
  // to be uploadable. Exposed for testing.
  bool ShouldUploadForm(const FormStructure& form);

 protected:
  // Test code should prefer to use this constructor.
  AutofillManager(AutofillDriver* driver,
                  AutofillClient* client,
                  PersonalDataManager* personal_data);

  // Uploads the form data to the Autofill server. |observed_submission|
  // indicates that upload is the result of a submission event.
  virtual void UploadFormData(const FormStructure& submitted_form,
                              bool observed_submission);

  // Logs quality metrics for the |submitted_form| and uploads the form data
  // to the crowdsourcing server, if appropriate. |observed_submission|
  // indicates whether the upload is a result of an observed submission event.
  virtual void UploadFormDataAsyncCallback(
      const FormStructure* submitted_form,
      const base::TimeTicks& load_time,
      const base::TimeTicks& interaction_time,
      const base::TimeTicks& submission_time,
      bool observed_submission);

  // Maps suggestion backend ID to and from an integer identifying it. Two of
  // these intermediate integers are packed by MakeFrontendID to make the IDs
  // that this class generates for the UI and for IPC.
  virtual int BackendIDToInt(const std::string& backend_id) const;
  virtual std::string IntToBackendID(int int_id) const;

  // Methods for packing and unpacking credit card and profile IDs for sending
  // and receiving to and from the renderer process.
  int MakeFrontendID(const std::string& cc_backend_id,
                     const std::string& profile_backend_id) const;
  void SplitFrontendID(int frontend_id,
                       std::string* cc_backend_id,
                       std::string* profile_backend_id) const;

  // AutofillHandler:
  bool OnWillSubmitFormImpl(const FormData& form,
                            const base::TimeTicks timestamp) override;
  void OnTextFieldDidChangeImpl(const FormData& form,
                                const FormFieldData& field,
                                const gfx::RectF& bounding_box,
                                const base::TimeTicks timestamp) override;
  void OnQueryFormFieldAutofillImpl(int query_id,
                                    const FormData& form,
                                    const FormFieldData& field,
                                    const gfx::RectF& transformed_box) override;
  void OnFocusOnFormFieldImpl(const FormData& form,
                              const FormFieldData& field,
                              const gfx::RectF& bounding_box) override;

  std::vector<std::unique_ptr<FormStructure>>* form_structures() {
    return &form_structures_;
  }

  AutofillMetrics::FormInteractionsUkmLogger* form_interactions_ukm_logger() {
    return form_interactions_ukm_logger_.get();
  }

  // payments::PaymentsClientDelegate:
  // Exposed for testing.
  void OnDidUploadCard(AutofillClient::PaymentsRpcResult result,
                       const std::string& server_id) override;

  // Exposed for testing.
  AutofillExternalDelegate* external_delegate() {
    return external_delegate_;
  }

  // Exposed for testing.
  void set_download_manager(AutofillDownloadManager* manager) {
    download_manager_.reset(manager);
  }

  // Exposed for testing.
  void set_payments_client(payments::PaymentsClient* payments_client) {
    payments_client_.reset(payments_client);
  }

 private:
  // AutofillDownloadManager::Observer:
  void OnLoadedServerPredictions(
      std::string response,
      const std::vector<std::string>& form_signatures) override;

  // payments::PaymentsClientDelegate:
  IdentityProvider* GetIdentityProvider() override;
  void OnDidGetRealPan(AutofillClient::PaymentsRpcResult result,
                       const std::string& real_pan) override;
  void OnDidGetUploadDetails(
      AutofillClient::PaymentsRpcResult result,
      const base::string16& context_token,
      std::unique_ptr<base::DictionaryValue> legal_message) override;

  // payments::FullCardRequest::ResultDelegate:
  void OnFullCardRequestSucceeded(
      const payments::FullCardRequest& full_card_request,
      const CreditCard& card,
      const base::string16& cvc) override;
  void OnFullCardRequestFailed() override;

  // payments::FullCardRequest::UIDelegate:
  void ShowUnmaskPrompt(const CreditCard& card,
                        AutofillClient::UnmaskCardReason reason,
                        base::WeakPtr<CardUnmaskDelegate> delegate) override;
  void OnUnmaskVerificationResult(
      AutofillClient::PaymentsRpcResult result) override;

  // Sets |user_did_accept_upload_prompt_| and calls UploadCard if the risk data
  // is available.
  void OnUserDidAcceptUpload();

  // Saves risk data in |uploading_risk_data_| and calls UploadCard if the user
  // has accepted the prompt.
  void OnDidGetUploadRiskData(const std::string& risk_data);

  // Returns false if Autofill is disabled or if no Autofill data is available.
  bool RefreshDataModels();

  // Gets the profile referred by |unique_id|. Returns true if the profile
  // exists.
  bool GetProfile(int unique_id, const AutofillProfile** profile);

  // Gets the credit card referred by |unique_id|. Returns true if the credit
  // card exists.
  bool GetCreditCard(int unique_id, const CreditCard** credit_card);

  // Determines whether a fill on |form| initiated from |field| will wind up
  // filling a credit card number. This is useful to determine if we will need
  // to unmask a card.
  bool WillFillCreditCardNumber(const FormData& form,
                                const FormFieldData& field);

  // Fills or previews the credit card form.
  // Assumes the form and field are valid.
  void FillOrPreviewCreditCardForm(
      AutofillDriver::RendererFormDataAction action,
      int query_id,
      const FormData& form,
      const FormFieldData& field,
      const CreditCard& credit_card);

  // Fills or previews the profile form.
  // Assumes the form and field are valid.
  void FillOrPreviewProfileForm(AutofillDriver::RendererFormDataAction action,
                                int query_id,
                                const FormData& form,
                                const FormFieldData& field,
                                const AutofillProfile& profile);

  // TODO(rogerm) here to see if these can be merged. FormData should be a
  // subset of the data in FormStructure and FormFieldData a subset of that in
  // AutofillField.
  // Fills or previews |data_model| in the |form|.
  void FillOrPreviewDataModelForm(AutofillDriver::RendererFormDataAction action,
                                  int query_id,
                                  const FormData& form,
                                  const FormFieldData& field,
                                  const AutofillDataModel& data_model,
                                  bool is_credit_card,
                                  const base::string16& cvc);
  void FillOrPreviewDataModelForm(AutofillDriver::RendererFormDataAction action,
                                  int query_id,
                                  const FormData& form,
                                  const FormFieldData& field,
                                  const AutofillDataModel& data_model,
                                  bool is_credit_card,
                                  const base::string16& cvc,
                                  FormStructure* form_structure,
                                  AutofillField* autofill_field);

  // Creates a FormStructure using the FormData received from the renderer. Will
  // return an empty scoped_ptr if the data should not be processed for upload
  // or personal data.
  std::unique_ptr<FormStructure> ValidateSubmittedForm(const FormData& form);

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

  // Returns the field corresponding to |form| and |field| that can be
  // autofilled. Returns NULL if the field cannot be autofilled.
  AutofillField* GetAutofillField(const FormData& form,
                                  const FormFieldData& field)
      WARN_UNUSED_RESULT;

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
  std::vector<Suggestion> GetProfileSuggestions(
      const FormStructure& form,
      const FormFieldData& field,
      const AutofillField& autofill_field) const;

  // Returns a list of values from the stored credit cards that match |type| and
  // the value of |field| and returns the labels of the matching credit cards.
  std::vector<Suggestion> GetCreditCardSuggestions(
      const FormFieldData& field,
      const AutofillType& type) const;

  // Parses the forms using heuristic matching and querying the Autofill server.
  void ParseForms(const std::vector<FormData>& forms);

  // Parses the form and adds it to |form_structures_|.
  bool ParseForm(const FormData& form, FormStructure** parsed_form_structure);

  // Imports the form data, submitted by the user, into |personal_data_|.
  void ImportFormData(const FormStructure& submitted_form);

  // Examines |card| and the stored profiles and if a candidate set of profiles
  // is found that matches the client-side validation rules, assigns the values
  // to |upload_request.profiles| and returns 0. If no valid set can be found,
  // returns the failure reasons. Appends any experiments that were triggered to
  // |upload_request.active_experiments|. The return value is a bitmask of
  // |AutofillMetrics::CardUploadDecisionMetric|.
  int SetProfilesForCreditCardUpload(
      const CreditCard& card,
      payments::PaymentsClient::UploadRequestDetails* upload_request) const;

  // Returns metric relevant to the CVC field based on values in
  // |found_cvc_field_|, |found_value_in_cvc_field_| and
  // |found_cvc_value_in_non_cvc_field_|.
  AutofillMetrics::CardUploadDecisionMetric GetCVCCardUploadDecisionMetric()
      const;

  // If |initial_interaction_timestamp_| is unset or is set to a later time than
  // |interaction_timestamp|, updates the cached timestamp.  The latter check is
  // needed because IPC messages can arrive out of order.
  void UpdateInitialInteractionTimestamp(
      const base::TimeTicks& interaction_timestamp);

  // Examines |form| and returns true if it is in a non-secure context or
  // its action attribute targets a HTTP url.
  bool IsFormNonSecure(const FormData& form) const;

  // Uses the existing personal data in |profiles| and |credit_cards| to
  // determine possible field types for the |submitted_form|.  This is
  // potentially expensive -- on the order of 50ms even for a small set of
  // |stored_data|. Hence, it should not run on the UI thread -- to avoid
  // locking up the UI -- nor on the IO thread -- to avoid blocking IPC calls.
  static void DeterminePossibleFieldTypesForUpload(
      const std::vector<AutofillProfile>& profiles,
      const std::vector<CreditCard>& credit_cards,
      const std::string& app_locale,
      FormStructure* submitted_form);

  // Uses context about previous and next fields to select the appropriate type
  // for fields with ambiguous upload types.
  static void DisambiguateUploadTypes(FormStructure* form);

  // Disambiguates address field upload types.
  static void DisambiguateAddressUploadTypes(FormStructure* form,
                                             size_t current_index);

  // Disambiguates phone field upload types.
  static void DisambiguatePhoneUploadTypes(FormStructure* form,
                                           size_t current_index);

  // Disambiguates name field upload types.
  static void DisambiguateNameUploadTypes(
      FormStructure* form,
      size_t current_index,
      const ServerFieldTypeSet& upload_types);

#ifdef ENABLE_FORM_DEBUG_DUMP
  // Dumps the cached forms to a file on disk.
  void DumpAutofillData(bool imported_cc) const;
#endif

  // Logs the card upload decisions in UKM and UMA.
  // |upload_decision_metrics| is a bitmask of
  // |AutofillMetrics::CardUploadDecisionMetric|.
  void LogCardUploadDecisions(int upload_decision_metrics);

  AutofillClient* const client_;

  // Handles Payments service requests.
  std::unique_ptr<payments::PaymentsClient> payments_client_;

  std::string app_locale_;

  // The personal data manager, used to save and load personal data to/from the
  // web database.  This is overridden by the AutofillManagerTest.
  // Weak reference.
  // May be NULL.  NULL indicates OTR.
  PersonalDataManager* personal_data_;

  base::circular_deque<std::string> autofilled_form_signatures_;

  // Handles queries and uploads to Autofill servers. Will be NULL if
  // the download manager functionality is disabled.
  std::unique_ptr<AutofillDownloadManager> download_manager_;

  // Handles single-field autocomplete form data.
  std::unique_ptr<AutocompleteHistoryManager> autocomplete_history_manager_;

  // Utility for logging URL keyed metrics.
  std::unique_ptr<AutofillMetrics::FormInteractionsUkmLogger>
      form_interactions_ukm_logger_;

  // Utilities for logging form events.
  std::unique_ptr<AutofillMetrics::FormEventLogger> address_form_event_logger_;
  std::unique_ptr<AutofillMetrics::FormEventLogger>
      credit_card_form_event_logger_;

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
  std::vector<std::unique_ptr<FormStructure>> form_structures_;

  // A copy of the currently interacted form data.
  std::unique_ptr<FormData> pending_form_data_;

  // Responsible for getting the full card details, including the PAN and the
  // CVC.
  std::unique_ptr<payments::FullCardRequest> full_card_request_;

  // Collected information about the autofill form where unmasked card will be
  // filled.
  int unmasking_query_id_;
  FormData unmasking_form_;
  FormFieldData unmasking_field_;
  CreditCard masked_card_;

  // Collected information about a pending upload request.
  payments::PaymentsClient::UploadRequestDetails upload_request_;
  bool user_did_accept_upload_prompt_;

  // |should_cvc_be_requested_| is |true| if we should request CVC from the user
  // in the card upload dialog.
  bool should_cvc_be_requested_;
  // |found_cvc_field_| is |true| if there exists a field that is determined to
  // be a CVC field via heuristics.
  bool found_cvc_field_;
  // |found_value_in_cvc_field_| is |true| if a field that is determined to
  // be a CVC field via heuristics has non-empty |value|.
  // |value| may or may not be a valid CVC.
  bool found_value_in_cvc_field_;
  // |found_cvc_value_in_non_cvc_field_| is |true| if a field that is not
  // determined to be a CVC field via heuristics has a valid CVC |value|.
  bool found_cvc_value_in_non_cvc_field_;

  GURL pending_upload_request_url_;

#ifdef ENABLE_FORM_DEBUG_DUMP
  // The last few autofilled forms (key/value pairs) submitted, for debugging.
  // TODO(brettw) this should be removed. See DumpAutofillData.
  std::vector<std::map<std::string, base::string16>>
      recently_autofilled_forms_;
#endif

  // Suggestion backend ID to ID mapping. We keep two maps to convert back and
  // forth. These should be used only by BackendIDToInt and IntToBackendID.
  // Note that the integers are not frontend IDs.
  mutable std::map<std::string, int> backend_to_int_map_;
  mutable std::map<int, std::string> int_to_backend_map_;

  // Delegate to perform external processing (display, selection) on
  // our behalf.  Weak.
  AutofillExternalDelegate* external_delegate_;

  // Delegate used in test to get notifications on certain events.
  AutofillManagerTestDelegate* test_delegate_;

#if defined(OS_ANDROID) || defined(OS_IOS)
  AutofillAssistant autofill_assistant_;
#endif

  base::WeakPtrFactory<AutofillManager> weak_ptr_factory_;

  friend class AutofillManagerTest;
  friend class FormStructureBrowserTest;
  FRIEND_TEST_ALL_PREFIXES(AutofillManagerTest,
                           DeterminePossibleFieldTypesForUpload);
  FRIEND_TEST_ALL_PREFIXES(AutofillManagerTest,
                           DeterminePossibleFieldTypesForUploadStressTest);
  FRIEND_TEST_ALL_PREFIXES(AutofillManagerTest, DisambiguateUploadTypes);
  FRIEND_TEST_ALL_PREFIXES(AutofillManagerTest,
                           DisabledAutofillDispatchesError);
  FRIEND_TEST_ALL_PREFIXES(AutofillMetricsTest, AddressFilledFormEvents);
  FRIEND_TEST_ALL_PREFIXES(AutofillMetricsTest, AddressSubmittedFormEvents);
  FRIEND_TEST_ALL_PREFIXES(AutofillMetricsTest, AddressWillSubmitFormEvents);
  FRIEND_TEST_ALL_PREFIXES(AutofillMetricsTest, AddressSuggestionsCount);
  FRIEND_TEST_ALL_PREFIXES(AutofillMetricsTest, AutofillFormSubmittedState);
  FRIEND_TEST_ALL_PREFIXES(AutofillMetricsTest, AutofillIsEnabledAtPageLoad);
  FRIEND_TEST_ALL_PREFIXES(AutofillMetricsTest, CreditCardSelectedFormEvents);
  FRIEND_TEST_ALL_PREFIXES(AutofillMetricsTest, CreditCardFilledFormEvents);
  FRIEND_TEST_ALL_PREFIXES(AutofillMetricsTest, CreditCardGetRealPanDuration);
  FRIEND_TEST_ALL_PREFIXES(AutofillMetricsTest, CreditCardWillSubmitFormEvents);
  FRIEND_TEST_ALL_PREFIXES(AutofillMetricsTest, CreditCardSubmittedFormEvents);
  FRIEND_TEST_ALL_PREFIXES(AutofillMetricsTest,
                           CreditCardCheckoutFlowUserActions);
  FRIEND_TEST_ALL_PREFIXES(
      AutofillMetricsTest,
      CreditCardSubmittedWithoutSelectingSuggestionsNoCard);
  FRIEND_TEST_ALL_PREFIXES(
      AutofillMetricsTest,
      CreditCardSubmittedWithoutSelectingSuggestionsUnknownCard);
  FRIEND_TEST_ALL_PREFIXES(
      AutofillMetricsTest,
      CreditCardSubmittedWithoutSelectingSuggestionsKnownCard);
  FRIEND_TEST_ALL_PREFIXES(
      AutofillMetricsTest,
      ShouldNotLogSubmitWithoutSelectingSuggestionsIfSuggestionFilled);
  FRIEND_TEST_ALL_PREFIXES(AutofillMetricsTest, ProfileCheckoutFlowUserActions);
  FRIEND_TEST_ALL_PREFIXES(AutofillMetricsTest, DeveloperEngagement);
  FRIEND_TEST_ALL_PREFIXES(AutofillMetricsTest, FormFillDuration);
  FRIEND_TEST_ALL_PREFIXES(AutofillMetricsTest,
                           NoQualityMetricsForNonAutofillableForms);
  FRIEND_TEST_ALL_PREFIXES(AutofillMetricsTest, QualityMetrics);
  FRIEND_TEST_ALL_PREFIXES(AutofillMetricsTest,
                           QualityMetrics_BasedOnAutocomplete);
  FRIEND_TEST_ALL_PREFIXES(AutofillMetricsTest, SaneMetricsWithCacheMismatch);
  FRIEND_TEST_ALL_PREFIXES(AutofillManagerTest, TestExternalDelegate);
  FRIEND_TEST_ALL_PREFIXES(AutofillManagerTest,
                           TestTabContentsWithExternalDelegate);
  FRIEND_TEST_ALL_PREFIXES(AutofillMetricsTest,
                           UserHappinessFormLoadAndSubmission);
  FRIEND_TEST_ALL_PREFIXES(AutofillMetricsTest,
                           UserHappinessFormInteraction_AddressForm);
  FRIEND_TEST_ALL_PREFIXES(AutofillMetricsTest,
                           UserHappinessFormInteraction_CreditCardForm);
  FRIEND_TEST_ALL_PREFIXES(AutofillManagerTest,
                           OnLoadedServerPredictions);
  FRIEND_TEST_ALL_PREFIXES(AutofillManagerTest,
                           OnLoadedServerPredictions_ResetManager);
  FRIEND_TEST_ALL_PREFIXES(AutofillManagerTest, DontOfferToSavePaymentsCard);
  FRIEND_TEST_ALL_PREFIXES(AutofillManagerTest, FillInUpdatedExpirationDate);
  DISALLOW_COPY_AND_ASSIGN(AutofillManager);
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_MANAGER_H_
