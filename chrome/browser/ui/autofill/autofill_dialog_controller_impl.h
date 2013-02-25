// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_DIALOG_CONTROLLER_IMPL_H_
#define CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_DIALOG_CONTROLLER_IMPL_H_

#include <vector>

#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/prefs/public/pref_change_registrar.h"
#include "base/string16.h"
#include "base/time.h"
#include "chrome/browser/autofill/autofill_manager_delegate.h"
#include "chrome/browser/autofill/autofill_metrics.h"
#include "chrome/browser/autofill/field_types.h"
#include "chrome/browser/autofill/form_structure.h"
#include "chrome/browser/autofill/personal_data_manager.h"
#include "chrome/browser/autofill/personal_data_manager_observer.h"
#include "chrome/browser/autofill/wallet/required_action.h"
#include "chrome/browser/autofill/wallet/wallet_client.h"
#include "chrome/browser/autofill/wallet/wallet_client_observer.h"
#include "chrome/browser/ui/autofill/autofill_dialog_controller.h"
#include "chrome/browser/ui/autofill/autofill_dialog_models.h"
#include "chrome/browser/ui/autofill/autofill_dialog_types.h"
#include "chrome/browser/ui/autofill/autofill_popup_controller_impl.h"
#include "chrome/browser/ui/autofill/autofill_popup_delegate.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/common/ssl_status.h"
#include "googleurl/src/gurl.h"
#include "ui/base/models/simple_menu_model.h"

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

// This class drives the dialog that appears when a site uses the imperative
// autocomplete API to fill out a form.
class AutofillDialogControllerImpl : public AutofillDialogController,
                                     public AutofillPopupDelegate,
                                     public content::NotificationObserver,
                                     public SuggestionsMenuModelDelegate,
                                     public wallet::WalletClientObserver,
                                     public PersonalDataManagerObserver {
 public:
  AutofillDialogControllerImpl(
      content::WebContents* contents,
      const FormData& form_structure,
      const GURL& source_url,
      const content::SSLStatus& ssl_status,
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

  // AutofillDialogController implementation.
  virtual string16 DialogTitle() const OVERRIDE;
  virtual string16 AccountChooserText() const OVERRIDE;
  virtual string16 EditSuggestionText() const OVERRIDE;
  virtual string16 UseBillingForShippingText() const OVERRIDE;
  virtual string16 CancelButtonText() const OVERRIDE;
  virtual string16 ConfirmButtonText() const OVERRIDE;
  virtual string16 SaveLocallyText() const OVERRIDE;
  virtual string16 CancelSignInText() const OVERRIDE;
  virtual string16 ProgressBarText() const OVERRIDE;
  virtual DialogSignedInState SignedInState() const OVERRIDE;
  virtual bool CanPayWithWallet() const OVERRIDE;
  virtual bool AccountChooserEnabled() const OVERRIDE;
  virtual bool ShouldOfferToSaveInChrome() const OVERRIDE;
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
  virtual void UserEditedOrActivatedInput(const DetailInput* input,
                                          DialogSection section,
                                          gfx::NativeView parent_view,
                                          const gfx::Rect& content_bounds,
                                          const string16& field_contents,
                                          bool was_edit) OVERRIDE;
  virtual bool HandleKeyPressEventInInput(
      const content::NativeWebKeyboardEvent& event) OVERRIDE;
  virtual void FocusMoved() OVERRIDE;
  virtual void ViewClosed(DialogAction action) OVERRIDE;
  virtual DialogNotification CurrentNotification() const OVERRIDE;
  virtual void StartSignInFlow() OVERRIDE;
  virtual void EndSignInFlow() OVERRIDE;
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

  // wallet::WalletClientObserver implementation.
  virtual void OnDidAcceptLegalDocuments() OVERRIDE;
  virtual void OnDidAuthenticateInstrument(bool success) OVERRIDE;
  virtual void OnDidGetFullWallet(
      scoped_ptr<wallet::FullWallet> full_wallet) OVERRIDE;
  virtual void OnDidGetWalletItems(
      scoped_ptr<wallet::WalletItems> wallet_items) OVERRIDE;
  virtual void OnDidSaveAddress(const std::string& address_id) OVERRIDE;
  virtual void OnDidSaveInstrument(const std::string& instrument_id) OVERRIDE;
  virtual void OnDidSaveInstrumentAndAddress(
      const std::string& instrument_id,
      const std::string& address_id) OVERRIDE;
  virtual void OnDidSendAutocheckoutStatus() OVERRIDE;
  virtual void OnDidUpdateInstrument(const std::string& instrument_id) OVERRIDE;
  virtual void OnWalletError() OVERRIDE;
  virtual void OnMalformedResponse() OVERRIDE;
  virtual void OnNetworkError(int response_code) OVERRIDE;

  // PersonalDataManagerObserver implementation.
  virtual void OnPersonalDataChanged() OVERRIDE;

  DialogType dialog_type() const { return dialog_type_; }

 protected:
  // Exposed for testing.
  AutofillDialogView* view() { return view_.get(); }

 private:
  // Refresh wallet items immediately if there's no refresh currently in
  // progress, otherwise wait until the current refresh completes.
  void ScheduleRefreshWalletItems();

  // Called when any type of request to Online Wallet completes. |success| is
  // true when there was no network error, the response wasn't malformed, and no
  // Wallet error occurred.
  void WalletRequestCompleted(bool success);

  // Whether or not the current request wants credit info back.
  bool RequestingCreditCardInfo() const;

  // Whether the information input in this dialog will be securely transmitted
  // to the requesting site.
  bool TransmissionWillBeSecure() const;

  // Convenience method to tell whether we need to address |action|.
  bool HasRequiredAction(wallet::RequiredAction action) const;

  // Whether the user has ever seen this dialog before. Cancels don't count.
  bool IsFirstRun() const;

  // A callback for pref changes, used by |pref_change_registrar_|.
  void PrefChanged(const std::string& pref);

  // Initializes |suggested_email_| et al.
  void GenerateSuggestionsModels();

  // Returns whether |profile| is complete, i.e. can fill out all the relevant
  // address info. Incomplete profiles will not be displayed in the dropdown
  // menu.
  bool IsCompleteProfile(const AutofillProfile& profile);

  // Creates a DataModelWrapper item for the item that's checked in the
  // suggestion model for |section|. This may represent Autofill
  // data or Wallet data, depending on whether Wallet is currently enabled.
  scoped_ptr<DataModelWrapper> CreateWrapper(DialogSection section);

  // Fills in |section|-related fields in |output_| according to the state of
  // |view_|.
  void FillOutputForSection(DialogSection section);
  // As above, but uses |compare| to determine whether a DetailInput matches
  // a field.
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

  // Returns the PersonalDataManager for |profile_|.
  PersonalDataManager* GetManager();

  // Like RequestedFieldsForSection, but returns a pointer.
  DetailInputs* MutableRequestedFieldsForSection(DialogSection section);

  // Hides |popup_controller_|'s popup view, if it exists.
  void HidePopup();

  // Asks risk module to asynchronously load fingerprint data. Data will be
  // returned via OnDidLoadRiskFingerprintData.
  void LoadRiskFingerprintData();
  void OnDidLoadRiskFingerprintData(scoped_ptr<risk::Fingerprint> fingerprint);

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

  // A client to talk to the Online Wallet API.
  wallet::WalletClient wallet_client_;

  // Whether another refresh for WalletItems should be started when the current
  // one is done.
  bool refresh_wallet_items_queued_;

  // Whether there has been a Wallet error while this dialog has been open.
  bool had_wallet_error_;

  // The most recently received WalletItems retrieved via |wallet_client_|.
  scoped_ptr<wallet::WalletItems> wallet_items_;

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

  // A PrefChangeRegistrar for tracking prefs useful to this dialog.
  PrefChangeRegistrar pref_change_registrar_;

  // For logging UMA metrics.
  const AutofillMetrics& metric_logger_;
  base::Time dialog_shown_timestamp_;
  DialogType dialog_type_;

  DISALLOW_COPY_AND_ASSIGN(AutofillDialogControllerImpl);
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_DIALOG_CONTROLLER_IMPL_H_
