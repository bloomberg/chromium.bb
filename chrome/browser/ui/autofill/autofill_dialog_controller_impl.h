// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_DIALOG_CONTROLLER_IMPL_H_
#define CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_DIALOG_CONTROLLER_IMPL_H_

#include <map>
#include <vector>

#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/string16.h"
#include "base/time.h"
#include "chrome/browser/ui/autofill/autofill_dialog_controller.h"
#include "chrome/browser/ui/autofill/autofill_dialog_models.h"
#include "chrome/browser/ui/autofill/autofill_dialog_types.h"
#include "chrome/browser/ui/autofill/autofill_popup_controller_impl.h"
#include "chrome/browser/ui/autofill/country_combobox_model.h"
#include "components/autofill/browser/autofill_manager_delegate.h"
#include "components/autofill/browser/autofill_metrics.h"
#include "components/autofill/browser/autofill_popup_delegate.h"
#include "components/autofill/browser/field_types.h"
#include "components/autofill/browser/form_structure.h"
#include "components/autofill/browser/personal_data_manager.h"
#include "components/autofill/browser/personal_data_manager_observer.h"
#include "components/autofill/browser/wallet/required_action.h"
#include "components/autofill/browser/wallet/wallet_client.h"
#include "components/autofill/browser/wallet/wallet_client_delegate.h"
#include "components/autofill/browser/wallet/wallet_signin_helper_delegate.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/common/ssl_status.h"
#include "googleurl/src/gurl.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/base/ui_base_types.h"

class AutofillPopupControllerImpl;
class FormGroup;
class Profile;
class PrefRegistrySyncable;

namespace content {
class WebContents;
}

namespace autofill {

class AutofillDialogView;
class DataModelWrapper;

namespace risk {
class Fingerprint;
}

namespace wallet {
class WalletSigninHelper;
}

// This class drives the dialog that appears when a site uses the imperative
// autocomplete API to fill out a form.
class AutofillDialogControllerImpl : public AutofillDialogController,
                                     public AutofillPopupDelegate,
                                     public content::NotificationObserver,
                                     public SuggestionsMenuModelDelegate,
                                     public wallet::WalletClientDelegate,
                                     public wallet::WalletSigninHelperDelegate,
                                     public PersonalDataManagerObserver,
                                     public AccountChooserModelDelegate {
 public:
  AutofillDialogControllerImpl(
      content::WebContents* contents,
      const FormData& form_structure,
      const GURL& source_url,
      const AutofillMetrics& metric_logger,
      const DialogType dialog_type,
      const base::Callback<void(const FormStructure*)>& callback);
  virtual ~AutofillDialogControllerImpl();

  static void RegisterUserPrefs(PrefRegistrySyncable* registry);

  void Show();
  void Hide();

  // Updates the progress bar based on the Autocheckout progress. |value| should
  // be in [0.0, 1.0].
  void UpdateProgressBar(double value);

  // Called when there is an error in an active Autocheckout flow.
  void OnAutocheckoutError();

  // AutofillDialogController implementation.
  virtual string16 DialogTitle() const OVERRIDE;
  virtual string16 AccountChooserText() const OVERRIDE;
  virtual string16 SignInLinkText() const OVERRIDE;
  virtual string16 EditSuggestionText() const OVERRIDE;
  virtual string16 UseBillingForShippingText() const OVERRIDE;
  virtual string16 CancelButtonText() const OVERRIDE;
  virtual string16 ConfirmButtonText() const OVERRIDE;
  virtual string16 SaveLocallyText() const OVERRIDE;
  virtual string16 CancelSignInText() const OVERRIDE;
  virtual string16 ProgressBarText() const OVERRIDE;
  virtual DialogSignedInState SignedInState() const OVERRIDE;
  virtual bool ShouldShowSpinner() const OVERRIDE;
  virtual bool ShouldOfferToSaveInChrome() const OVERRIDE;
  virtual ui::MenuModel* MenuModelForAccountChooser() OVERRIDE;
  virtual gfx::Image AccountChooserImage() OVERRIDE;
  virtual bool AutocheckoutIsRunning() const OVERRIDE;
  virtual bool HadAutocheckoutError() const OVERRIDE;
  virtual bool IsDialogButtonEnabled(ui::DialogButton button) const OVERRIDE;
  virtual bool SectionIsActive(DialogSection section) const OVERRIDE;
  virtual const DetailInputs& RequestedFieldsForSection(DialogSection section)
      const OVERRIDE;
  virtual ui::ComboboxModel* ComboboxModelForAutofillType(
      AutofillFieldType type) OVERRIDE;
  virtual ui::MenuModel* MenuModelForSection(DialogSection section) OVERRIDE;
  virtual string16 LabelForSection(DialogSection section) const OVERRIDE;
  virtual string16 SuggestionTextForSection(DialogSection section) OVERRIDE;
  virtual gfx::Image SuggestionIconForSection(DialogSection section) OVERRIDE;
  virtual void EditClickedForSection(DialogSection section) OVERRIDE;
  virtual gfx::Image IconForField(AutofillFieldType type,
                                  const string16& user_input) const OVERRIDE;
  virtual bool InputIsValid(AutofillFieldType type, const string16& value)
      OVERRIDE;
  virtual std::vector<AutofillFieldType> InputsAreValid(
      const DetailOutputMap& inputs, ValidationType validation_type) OVERRIDE;
  virtual void UserEditedOrActivatedInput(const DetailInput* input,
                                          DialogSection section,
                                          gfx::NativeView parent_view,
                                          const gfx::Rect& content_bounds,
                                          const string16& field_contents,
                                          bool was_edit) OVERRIDE;
  virtual bool HandleKeyPressEventInInput(
      const content::NativeWebKeyboardEvent& event) OVERRIDE;
  virtual void FocusMoved() OVERRIDE;
  virtual void ViewClosed() OVERRIDE;
  virtual std::vector<DialogNotification> CurrentNotifications() const OVERRIDE;
  virtual void StartSignInFlow() OVERRIDE;
  virtual void EndSignInFlow() OVERRIDE;
  virtual void OnCancel() OVERRIDE;
  virtual void OnSubmit() OVERRIDE;
  virtual Profile* profile() OVERRIDE;
  virtual content::WebContents* web_contents() OVERRIDE;

  // AutofillPopupDelegate implementation.
  virtual void OnPopupShown(content::KeyboardListener* listener) OVERRIDE;
  virtual void OnPopupHidden(content::KeyboardListener* listener) OVERRIDE;
  virtual void DidSelectSuggestion(int identifier) OVERRIDE;
  virtual void DidAcceptSuggestion(const string16& value,
                                   int identifier) OVERRIDE;
  virtual void RemoveSuggestion(const string16& value,
                                int identifier) OVERRIDE;
  virtual void ClearPreviewedForm() OVERRIDE;

  // content::NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // SuggestionsMenuModelDelegate implementation.
  virtual void SuggestionItemSelected(const SuggestionsMenuModel& model)
      OVERRIDE;

  // wallet::WalletClientDelegate implementation.
  virtual const AutofillMetrics& GetMetricLogger() const OVERRIDE;
  virtual DialogType GetDialogType() const OVERRIDE;
  virtual std::string GetRiskData() const OVERRIDE;
  virtual void OnDidAcceptLegalDocuments() OVERRIDE;
  virtual void OnDidAuthenticateInstrument(bool success) OVERRIDE;
  virtual void OnDidGetFullWallet(
      scoped_ptr<wallet::FullWallet> full_wallet) OVERRIDE;
  virtual void OnDidGetWalletItems(
      scoped_ptr<wallet::WalletItems> wallet_items) OVERRIDE;
  virtual void OnDidSaveAddress(
      const std::string& address_id,
      const std::vector<wallet::RequiredAction>& required_actions) OVERRIDE;
  virtual void OnDidSaveInstrument(
      const std::string& instrument_id,
      const std::vector<wallet::RequiredAction>& required_actions) OVERRIDE;
  virtual void OnDidSaveInstrumentAndAddress(
      const std::string& instrument_id,
      const std::string& address_id,
      const std::vector<wallet::RequiredAction>& required_actions) OVERRIDE;
  virtual void OnDidSendAutocheckoutStatus() OVERRIDE;
  virtual void OnDidUpdateAddress(
      const std::string& address_id,
      const std::vector<wallet::RequiredAction>& required_actions) OVERRIDE;
  virtual void OnDidUpdateInstrument(
      const std::string& instrument_id,
      const std::vector<wallet::RequiredAction>& required_actions) OVERRIDE;
  virtual void OnWalletError(
      wallet::WalletClient::ErrorType error_type) OVERRIDE;
  virtual void OnMalformedResponse() OVERRIDE;
  virtual void OnNetworkError(int response_code) OVERRIDE;

  // PersonalDataManagerObserver implementation.
  virtual void OnPersonalDataChanged() OVERRIDE;

  // AccountChooserModelDelegate implementation.
  virtual void AccountChoiceChanged() OVERRIDE;

  // wallet::WalletSigninHelperDelegate implementation.
  virtual void OnPassiveSigninSuccess(
      const std::string& auth_username) OVERRIDE;
  virtual void OnPassiveSigninFailure(
      const GoogleServiceAuthError& error) OVERRIDE;
  virtual void OnAutomaticSigninSuccess(
      const std::string& auth_username) OVERRIDE;
  virtual void OnAutomaticSigninFailure(
      const GoogleServiceAuthError& error) OVERRIDE;
  virtual void OnUserNameFetchSuccess(
      const std::string& auth_username) OVERRIDE;
  virtual void OnUserNameFetchFailure(
      const GoogleServiceAuthError& error) OVERRIDE;

  DialogType dialog_type() const { return dialog_type_; }

 protected:
  // Exposed for testing.
  AutofillDialogView* view() { return view_.get(); }
  virtual AutofillDialogView* CreateView();

  // Returns the PersonalDataManager for |profile_|.
  virtual PersonalDataManager* GetManager();

  // Returns the WalletClient* this class uses to talk to Online Wallet. Exposed
  // for testing.
  virtual wallet::WalletClient* GetWalletClient();

  // Call to disable communication to Online Wallet for this dialog.
  // Exposed for testing.
  void DisableWallet();

  // Returns whether Wallet is the current data source. Exposed for testing.
  virtual bool IsPayingWithWallet() const;

 private:
  // Whether or not the current request wants credit info back.
  bool RequestingCreditCardInfo() const;

  // Whether the information input in this dialog will be securely transmitted
  // to the requesting site.
  bool TransmissionWillBeSecure() const;

  // Whether the user has ever seen this dialog before. Cancels don't count.
  bool IsFirstRun() const;

  // Initializes |suggested_email_| et al.
  void GenerateSuggestionsModels();

  // Returns whether |profile| is complete, i.e. can fill out all the relevant
  // address info. Incomplete profiles will not be displayed in the dropdown
  // menu.
  bool IsCompleteProfile(const AutofillProfile& profile);

  // Whether the user's wallet items have at least one address and instrument.
  bool HasCompleteWallet() const;

  // Starts fetching the wallet items.
  void StartFetchingWalletItems();

  // Refreshes the model on Wallet or sign-in state update.
  void OnWalletOrSigninUpdate();

  // Should be called on the Wallet sign-in error.
  void OnWalletSigninError();

  // Creates a DataModelWrapper item for the item that's checked in the
  // suggestion model for |section|. This may represent Autofill
  // data or Wallet data, depending on whether Wallet is currently enabled.
  scoped_ptr<DataModelWrapper> CreateWrapper(DialogSection section);

  // Fills in |section|-related fields in |output_| according to the state of
  // |view_|.
  void FillOutputForSection(DialogSection section);
  // As above, but uses |compare| to determine whether a DetailInput matches
  // a field. Saves any new Autofill data to the PersonalDataManager.
  void FillOutputForSectionWithComparator(DialogSection section,
                                          const InputFieldComparator& compare);

  // Fills in |form_structure_| using |form_group|. Utility method for
  // FillOutputForSection.
  void FillFormStructureForSection(const FormGroup& form_group,
                                   size_t variant,
                                   DialogSection section,
                                   const InputFieldComparator& compare);

  // Sets the CVC result on |form_structure_| to the value in |cvc|.
  void SetCvcResult(const string16& cvc);

  // Gets the SuggestionsMenuModel for |section|.
  SuggestionsMenuModel* SuggestionsMenuModelForSection(DialogSection section);
  // And the reverse.
  DialogSection SectionForSuggestionsMenuModel(
      const SuggestionsMenuModel& model);

  // Loads profiles that can suggest data for |type|. |field_contents| is the
  // part the user has already typed. |inputs| is the rest of section.
  // Identifying info is loaded into the last three outparams as well as
  // |popup_guids_|.
  void GetProfileSuggestions(
      AutofillFieldType type,
      const string16& field_contents,
      const DetailInputs& inputs,
      std::vector<string16>* popup_values,
      std::vector<string16>* popup_labels,
      std::vector<string16>* popup_icons);

  // Like RequestedFieldsForSection, but returns a pointer.
  DetailInputs* MutableRequestedFieldsForSection(DialogSection section);

  // Hides |popup_controller_|'s popup view, if it exists.
  void HidePopup();

  // Asks risk module to asynchronously load fingerprint data. Data will be
  // returned via OnDidLoadRiskFingerprintData.
  void LoadRiskFingerprintData();
  void OnDidLoadRiskFingerprintData(scoped_ptr<risk::Fingerprint> fingerprint);

  // Whether the user has chosen to enter all new data in |section|. This
  // happens via choosing "Add a new X..." from a section's suggestion menu.
  bool IsManuallyEditingSection(DialogSection section);

  // Whether the billing section should be used to fill in the shipping details.
  bool ShouldUseBillingForShipping();

  // Whether the user wishes to save information locally to Autofill.
  bool ShouldSaveDetailsLocally();

  // Start the submit proccess to interact with Online Wallet (might do various
  // things like accept documents, save details, update details, respond to
  // required actions, etc.).
  void SubmitWithWallet();

  // Gets a full wallet from Online Wallet so the user can purchase something.
  // This information is decoded to reveal a fronting (proxy) card.
  void GetFullWallet();

  // Called when there's nothing left to accept, update, save, or authenticate
  // in order to fill |form_structure_| and pass data back to the invoking page.
  void FinishSubmit();

  AutofillMetrics::DialogInitialUserStateMetric GetInitialUserState() const;

  // The |profile| for |contents_|.
  Profile* const profile_;

  // The WebContents where the Autofill action originated.
  content::WebContents* const contents_;

  FormStructure form_structure_;

  // Whether the URL visible to the user when this dialog was requested to be
  // invoked is the same as |source_url_|.
  bool invoked_from_same_origin_;

  // The URL of the invoking site.
  GURL source_url_;

  // The SSL info from the invoking site.
  content::SSLStatus ssl_status_;

  // The callback via which we return the collected data.
  base::Callback<void(const FormStructure*)> callback_;

  // The AccountChooserModel acts as the MenuModel for the account chooser,
  // and also tracks which data source the dialog is using.
  AccountChooserModel account_chooser_model_;

  // The sign-in helper to fetch the user info and perform passive sign-in.
  // The helper is set only during fetch/sign-in, and NULL otherwise.
  scoped_ptr<wallet::WalletSigninHelper> signin_helper_;

  // The user account name (email).
  // Valid only if PayingWithWallet() and SigninState() == SIGNED_IN.
  // Set whenever the sign-in helper fetches the user info.
  // TODO(aruslan): this should be removed (AccountChooser should deal with it).
  std::string current_username_;

  // A client to talk to the Online Wallet API.
  wallet::WalletClient wallet_client_;

  // Recently received items retrieved via |wallet_client_|.
  scoped_ptr<wallet::WalletItems> wallet_items_;
  scoped_ptr<wallet::FullWallet> full_wallet_;

  // Used to remember the state of Wallet comboboxes when Submit was clicked.
  std::string active_instrument_id_;
  std::string active_address_id_;

  // The fields for billing and shipping which the page has actually requested.
  DetailInputs requested_email_fields_;
  DetailInputs requested_cc_fields_;
  DetailInputs requested_billing_fields_;
  DetailInputs requested_cc_billing_fields_;
  DetailInputs requested_shipping_fields_;

  // Models for the credit card expiration inputs.
  MonthComboboxModel cc_exp_month_combobox_model_;
  YearComboboxModel cc_exp_year_combobox_model_;

  // Model for the country input.
  CountryComboboxModel country_combobox_model_;

  // Models for the suggestion views.
  SuggestionsMenuModel suggested_email_;
  SuggestionsMenuModel suggested_cc_;
  SuggestionsMenuModel suggested_billing_;
  SuggestionsMenuModel suggested_cc_billing_;
  SuggestionsMenuModel suggested_shipping_;

  // A map from DialogSection to editing state (true for editing, false for
  // not editing). This only tracks if the user has clicked the edit link.
  std::map<DialogSection, bool> section_editing_state_;

  // The GUIDs for the currently showing unverified profiles popup.
  std::vector<PersonalDataManager::GUIDPair> popup_guids_;

  // The controller for the currently showing popup (which helps users when
  // they're manually filling the dialog).
  base::WeakPtr<AutofillPopupControllerImpl> popup_controller_;

  // The section for which |popup_controller_| is currently showing a popup
  // (if any).
  DialogSection section_showing_popup_;

  scoped_ptr<AutofillDialogView> view_;

  // A NotificationRegistrar for tracking the completion of sign-in.
  content::NotificationRegistrar registrar_;

  base::WeakPtrFactory<AutofillDialogControllerImpl> weak_ptr_factory_;

  // For logging UMA metrics.
  const AutofillMetrics& metric_logger_;
  base::Time dialog_shown_timestamp_;
  base::Time autocheckout_started_timestamp_;
  AutofillMetrics::DialogInitialUserStateMetric initial_user_state_;

  // Whether this is an Autocheckout or a requestAutocomplete dialog.
  const DialogType dialog_type_;

  // True if the termination action was a submit.
  bool did_submit_;

  // Whether or not an Autocheckout flow is running.
  bool autocheckout_is_running_;

  // Whether or not there was an error in the Autocheckout flow.
  bool had_autocheckout_error_;

  DISALLOW_COPY_AND_ASSIGN(AutofillDialogControllerImpl);
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_DIALOG_CONTROLLER_IMPL_H_
