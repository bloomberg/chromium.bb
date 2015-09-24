// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_DIALOG_CONTROLLER_IMPL_H_
#define CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_DIALOG_CONTROLLER_IMPL_H_

#include <set>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"
#include "base/time/time.h"
#include "chrome/browser/ui/autofill/autofill_dialog_controller.h"
#include "chrome/browser/ui/autofill/autofill_dialog_models.h"
#include "chrome/browser/ui/autofill/autofill_dialog_types.h"
#include "chrome/browser/ui/autofill/autofill_dialog_view_delegate.h"
#include "chrome/browser/ui/autofill/autofill_popup_controller_impl.h"
#include "chrome/browser/ui/autofill/country_combobox_model.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/autofill_metrics.h"
#include "components/autofill/core/browser/autofill_popup_delegate.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/personal_data_manager_observer.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/ssl_status.h"
#include "third_party/libaddressinput/chromium/chrome_address_validator.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/gfx/animation/linear_animation.h"
#include "url/gurl.h"

class Profile;

namespace content {
class WebContents;
}

namespace i18n {
namespace addressinput {
struct AddressData;
}
}

namespace autofill {

class AutofillDataModel;
class AutofillDialogView;
class AutofillPopupControllerImpl;
class DataModelWrapper;

namespace risk {
class Fingerprint;
}

namespace wallet {
class WalletSigninHelper;
}

// This class drives the dialog that appears when a site uses the imperative
// autocomplete API to fill out a form.
class AutofillDialogControllerImpl
    : public AutofillDialogViewDelegate,
      public AutofillDialogController,
      public AutofillPopupDelegate,
      public content::WebContentsObserver,
      public SuggestionsMenuModelDelegate,
      public PersonalDataManagerObserver,
      public LoadRulesListener {
 public:
  ~AutofillDialogControllerImpl() override;

  static base::WeakPtr<AutofillDialogControllerImpl> Create(
      content::WebContents* contents,
      const FormData& form_structure,
      const GURL& source_url,
      const AutofillClient::ResultCallback& callback);

  // AutofillDialogController implementation.
  void Show() override;
  void Hide() override;
  void TabActivated() override;

  // AutofillDialogViewDelegate implementation.
  base::string16 DialogTitle() const override;
  base::string16 EditSuggestionText() const override;
  base::string16 CancelButtonText() const override;
  base::string16 ConfirmButtonText() const override;
  base::string16 SaveLocallyText() const override;
  base::string16 SaveLocallyTooltip() const override;
  bool ShouldOfferToSaveInChrome() const override;
  bool ShouldSaveInChrome() const override;
  int GetDialogButtons() const override;
  bool IsDialogButtonEnabled(ui::DialogButton button) const override;
  bool SectionIsActive(DialogSection section) const override;
  const DetailInputs& RequestedFieldsForSection(
      DialogSection section) const override;
  ui::ComboboxModel* ComboboxModelForAutofillType(
      ServerFieldType type) override;
  ui::MenuModel* MenuModelForSection(DialogSection section) override;
  base::string16 LabelForSection(DialogSection section) const override;
  SuggestionState SuggestionStateForSection(DialogSection section) override;
  FieldIconMap IconsForFields(const FieldValueMap& user_inputs) const override;
  bool FieldControlsIcons(ServerFieldType type) const override;
  base::string16 TooltipForField(ServerFieldType type) const override;
  base::string16 InputValidityMessage(DialogSection section,
                                      ServerFieldType type,
                                      const base::string16& value) override;
  ValidityMessages InputsAreValid(DialogSection section,
                                  const FieldValueMap& inputs) override;
  void UserEditedOrActivatedInput(DialogSection section,
                                  ServerFieldType type,
                                  gfx::NativeView parent_view,
                                  const gfx::Rect& content_bounds,
                                  const base::string16& field_contents,
                                  bool was_edit) override;
  bool HandleKeyPressEventInInput(
      const content::NativeWebKeyboardEvent& event) override;
  void FocusMoved() override;
  bool ShouldShowErrorBubble() const override;
  void ViewClosed() override;
  std::vector<DialogNotification> CurrentNotifications() override;
  void LinkClicked(const GURL& url) override;
  void OnCancel() override;
  void OnAccept() override;
  Profile* profile() override;
  content::WebContents* GetWebContents() override;

  // AutofillPopupDelegate implementation.
  void OnPopupShown() override;
  void OnPopupHidden() override;
  void DidSelectSuggestion(const base::string16& value,
                           int identifier) override;
  void DidAcceptSuggestion(const base::string16& value,
                           int identifier,
                           int position) override;
  bool GetDeletionConfirmationText(const base::string16& value,
                                   int identifier,
                                   base::string16* title,
                                   base::string16* body) override;
  bool RemoveSuggestion(const base::string16& value, int identifier) override;
  void ClearPreviewedForm() override;


  // SuggestionsMenuModelDelegate implementation.
  void SuggestionItemSelected(SuggestionsMenuModel* model,
                              size_t index) override;

  // PersonalDataManagerObserver implementation.
  void OnPersonalDataChanged() override;

  // LoadRulesListener implementation.
  void OnAddressValidationRulesLoaded(const std::string& country_code,
                                      bool success) override;

 protected:
  // Exposed for testing.
  AutofillDialogControllerImpl(content::WebContents* contents,
                               const FormData& form_structure,
                               const GURL& source_url,
                               const AutofillClient::ResultCallback& callback);

  // Exposed for testing.
  AutofillDialogView* view() { return view_.get(); }
  virtual AutofillDialogView* CreateView();
  ServerFieldType popup_input_type() const {
    return popup_input_type_;
  }

  // Returns the PersonalDataManager for |profile_|.
  virtual PersonalDataManager* GetManager() const;

  // Returns an address validation helper. May be NULL during tests.
  virtual AddressValidator* GetValidator();

  // Opens the given URL in a new foreground tab.
  virtual void OpenTabWithUrl(const GURL& url);

  // The active billing section for the current state of the dialog (e.g. when
  // paying for wallet, the combined credit card + billing address section).
  DialogSection ActiveBillingSection() const;

  // Whether |section| was sent into edit mode based on existing data. This
  // happens when a user clicks "Edit" or a suggestion is invalid.
  virtual bool IsEditingExistingData(DialogSection section) const;

  // Whether the user has chosen to enter all new data in |section|. This
  // happens via choosing "Add a new X..." from a section's suggestion menu.
  bool IsManuallyEditingSection(DialogSection section) const;

  // Shows a new credit card saved bubble and passes ownership of |new_card| and
  // |billing_profile| to the bubble. Exposed for testing.
  virtual void ShowNewCreditCardBubble(
      scoped_ptr<CreditCard> new_card,
      scoped_ptr<AutofillProfile> billing_profile);

  // Delays enabling submit button for a short period of time. Exposed for
  // testing.
  virtual void SubmitButtonDelayBegin();

  void FinishSubmit();

  // Ends the delay for enabling the submit button. Called only from tests.
  // Without this method, the tests would have to wait for the delay timer to
  // finish, which would be flaky.
  void SubmitButtonDelayEndForTesting();

 private:
  FRIEND_TEST_ALL_PREFIXES(AutofillDialogControllerTest,
                           TransactionAmount);
  FRIEND_TEST_ALL_PREFIXES(AutofillDialogControllerTest,
                           TransactionAmountReadonly);

  // Initializes or updates |suggested_cc_| et al.
  void SuggestionsUpdated();

  // Starts fetching the wallet items from Online Wallet.
  void GetWalletItems();

  // Clears previously entered manual input and removes |section| from
  // |section_editing_state_|. Does not update the view.
  void ResetSectionInput(DialogSection section);

  // Force |section| into edit mode if the current suggestion is invalid.
  void ShowEditUiIfBadSuggestion(DialogSection section);

  // Whether the |value| of |input| should be preserved on account change.
  bool InputWasEdited(ServerFieldType type,
                      const base::string16& value);

  // Takes a snapshot of the newly inputted user data in |view_| (if it exists).
  FieldValueMap TakeUserInputSnapshot();

  // Fills the detail inputs from a previously taken user input snapshot. Does
  // not update the view.
  void RestoreUserInputFromSnapshot(const FieldValueMap& snapshot);

  // Tells the view to update |section|.
  void UpdateSection(DialogSection section);

  // Tells |view_| to update the validity status of its detail inputs (if
  // |view_| is non-null). Currently this is used solely for highlighting
  // invalid suggestions, so if no sections are based on existing data,
  // |view_->UpdateForErrors()| is not called.
  void UpdateForErrors();

  // Creates a DataModelWrapper item for the item that's checked in the
  // suggestion model for |section|. This may represent Autofill
  // data or Wallet data, depending on whether Wallet is currently enabled.
  scoped_ptr<DataModelWrapper> CreateWrapper(DialogSection section);

  // Fills in |section|-related fields in |output_| according to the state of
  // |view_|.
  void FillOutputForSection(DialogSection section);
  // As above, but uses |compare| to determine whether a DetailInput matches
  // a field. Saves any new Autofill data to the PersonalDataManager.
  void FillOutputForSectionWithComparator(
      DialogSection section,
      const FormStructure::InputFieldComparator& compare);

  // Returns whether |form_structure|_| has any fields that match the fieldset
  // represented by |section|.
  bool FormStructureCaresAboutSection(DialogSection section) const;

  // Finds all fields of the given |type| in |form_structure_|, if any, and sets
  // each field's value to |output|.
  void SetOutputForFieldsOfType(ServerFieldType type,
                                const base::string16& output);

  // Gets the value for |type| in |section|, whether it comes from manual user
  // input or the active suggestion.
  base::string16 GetValueFromSection(DialogSection section,
                                     ServerFieldType type);

  // Returns whether the given section can accept an address with the given
  // country code.
  bool CanAcceptCountry(DialogSection section, const std::string& country_code);

  // Whether |profile| should be suggested for |section|.
  bool ShouldSuggestProfile(DialogSection section,
                            const AutofillProfile& profile);

  // Gets the SuggestionsMenuModel for |section|.
  SuggestionsMenuModel* SuggestionsMenuModelForSection(DialogSection section);
  const SuggestionsMenuModel* SuggestionsMenuModelForSection(
      DialogSection section) const;
  // And the reverse.
  DialogSection SectionForSuggestionsMenuModel(
      const SuggestionsMenuModel& model);

  // Gets the CountryComboboxModel for |section|.
  CountryComboboxModel* CountryComboboxModelForSection(DialogSection section);

  // Clears and builds the inputs in |section| for |country_name|.
  // When |should_clobber| is false, and the view's country value matches
  // |country_name|, the inputs won't be rebuilt.
  bool RebuildInputsForCountry(DialogSection section,
                               const base::string16& country_name,
                               bool should_clobber);

  // Suggested text and icons for sections. Suggestion text is used to show an
  // abridged overview of the currently used suggestion. Extra text is used when
  // part of a section is suggested but part must be manually input (e.g. during
  // a CVC challenge or when using Autofill's CC section [never stores CVC]).
  bool SuggestionTextForSection(DialogSection section,
                                base::string16* vertically_compact,
                                base::string16* horizontally_compact);
  gfx::Image SuggestionIconForSection(DialogSection section);
  base::string16 ExtraSuggestionTextForSection(DialogSection section) const;
  gfx::Image ExtraSuggestionIconForSection(DialogSection section);

  // Suggests address completions using the downloaded i18n validation rules.
  // Stores the suggestions in |i18n_validator_suggestions_|.
  void GetI18nValidatorSuggestions(
      DialogSection section,
      ServerFieldType type,
      std::vector<autofill::Suggestion>* popup_suggestions);

  // Like RequestedFieldsForSection, but returns a pointer.
  DetailInputs* MutableRequestedFieldsForSection(DialogSection section);

  // Returns a pointer to the language code that should be used for formatting
  // the address in |section| for display. Returns NULL for a non-address
  // |section|.
  std::string* MutableAddressLanguageCodeForSection(DialogSection section);

  // Returns the language code that should be used for formatting the address in
  // |section|. Returns an empty string for a non-address |section|.
  std::string AddressLanguageCodeForSection(DialogSection section);

  // Returns just the |type| attributes of RequestedFieldsForSection(section).
  std::vector<ServerFieldType> RequestedTypesForSection(DialogSection section)
      const;

  // Returns the country code (e.g. "US") for |section|.
  std::string CountryCodeForSection(DialogSection section);

  // Hides |popup_controller_|'s popup view, if it exists.
  void HidePopup();

  // Set whether the currently editing |section| was originally based on
  // existing Wallet or Autofill data.
  void SetEditingExistingData(DialogSection section, bool editing);

  // Whether the user has chosen to enter all new data in at least one section.
  bool IsManuallyEditingAnySection() const;

  // Returns validity message for a given credit card number.
  base::string16 CreditCardNumberValidityMessage(
      const base::string16& number) const;

  // Whether all of the input fields currently showing in the dialog have valid
  // contents. This validates only by checking "sure" messages, i.e. messages
  // that would have been displayed to the user during editing, as opposed to
  // submission.
  bool AllSectionsAreValid();

  // Whether all of the input fields currently showing in the given |section| of
  // the dialog have valid contents. This validates only by checking "sure"
  // messages - see AllSectionsAreValid.
  bool SectionIsValid(DialogSection section);

  // Whether validation rules for |section| are loaded.
  bool RulesAreLoaded(DialogSection section);

  // Whether the currently active credit card expiration date is valid.
  bool IsCreditCardExpirationValid(const base::string16& year,
                                   const base::string16& month) const;

  // Returns true if we should reject the given credit card brand. |type| should
  // be a display string, such as "Visa".
  bool ShouldDisallowCcType(const base::string16& type) const;

  // Returns true if |profile| has an address we can be sure is invalid.
  // Profiles with invalid addresses are not suggested in the dropdown menu for
  // billing and shipping addresses.
  bool HasInvalidAddress(const AutofillProfile& profile);

  // Returns true if |key| refers to a suggestion, as opposed to some control
  // menu item.
  bool IsASuggestionItemKey(const std::string& key) const;

  // Returns whether Autofill is enabled for |profile_|.
  bool IsAutofillEnabled() const;

  // Whether the billing section should be used to fill in the shipping details.
  bool ShouldUseBillingForShipping();

  // Whether the user wishes to save information locally to Autofill.
  bool ShouldSaveDetailsLocally();

  // Writes to prefs the choice of AutofillDataModel for |section|.
  void PersistAutofillChoice(DialogSection section,
                             const std::string& guid);

  // Sets the outparams to the default AutofillDataModel for |section| (which is
  // the first one in the menu that is a suggestion item).
  void GetDefaultAutofillChoice(DialogSection section,
                                std::string* guid);

  // Reads from prefs the choice of AutofillDataModel for |section|. Returns
  // whether there was a setting to read.
  bool GetAutofillChoice(DialogSection section,
                         std::string* guid);

  // Logs metrics when the dialog is submitted.
  void LogOnFinishSubmitMetrics();

  // Logs metrics when the dialog is canceled.
  void LogOnCancelMetrics();

  // Logs metrics when the edit ui is shown for the given |section|.
  void LogEditUiShownMetric(DialogSection section);

  // Logs metrics when a suggestion item from the given |model| is selected.
  void LogSuggestionItemSelectedMetric(const SuggestionsMenuModel& model);

  // Logs the time elapsed from when the dialog was shown to when the user could
  // interact with it.
  void LogDialogLatencyToShow();

  // Returns the metric corresponding to the user's initial state when
  // interacting with this dialog.
  AutofillMetrics::DialogInitialUserStateMetric GetInitialUserState() const;

  // Shows an educational bubble if a new credit card was saved or the first few
  // times an Online Wallet fronting card was generated.
  void MaybeShowCreditCardBubble();

  // Called when the delay for enabling the submit button ends.
  void OnSubmitButtonDelayEnd();

  // The |profile| for |contents_|.
  Profile* const profile_;

  // For logging UMA metrics.
  base::Time dialog_shown_timestamp_;
  AutofillMetrics::DialogInitialUserStateMetric initial_user_state_;

  FormStructure form_structure_;

  // Whether the URL visible to the user when this dialog was requested to be
  // invoked is the same as |source_url_|.
  bool invoked_from_same_origin_;

  // The URL of the invoking site.
  GURL source_url_;

  // The callback via which we return the collected data.
  AutofillClient::ResultCallback callback_;

  // A helper to validate international address input.
  scoped_ptr<AddressValidator> validator_;

  // The default active instrument and shipping address object IDs as of the
  // last time Wallet items were fetched. These variables are only set
  // (i.e. non-empty) when the Wallet items are being re-fetched.
  std::string previous_default_instrument_id_;
  std::string previous_default_shipping_address_id_;
  // The last active instrument and shipping address object IDs. These
  // variables are only set (i.e. non-empty) when the Wallet items are being
  // re-fetched.
  std::string previously_selected_instrument_id_;
  std::string previously_selected_shipping_address_id_;

  // When the Wallet items were last fetched.
  base::TimeTicks last_wallet_items_fetch_timestamp_;

  // Local machine signals to pass along on each request to trigger (or
  // discourage) risk challenges; sent if the user is up to date on legal docs.
  std::string risk_data_;

  // The text to display when the user is accepting new terms of service, etc.
  base::string16 legal_documents_text_;
  // The ranges within |legal_documents_text_| to linkify.
  std::vector<gfx::Range> legal_document_link_ranges_;

  // The instrument and address IDs from the Online Wallet server to be used
  // when getting a full wallet.
  std::string active_instrument_id_;
  std::string active_address_id_;

  // The fields for billing and shipping which the page has actually requested.
  DetailInputs requested_cc_fields_;
  DetailInputs requested_billing_fields_;
  DetailInputs requested_cc_billing_fields_;
  DetailInputs requested_shipping_fields_;

  // The BCP 47 language codes used for formatting the addresses for display.
  std::string billing_address_language_code_;
  std::string shipping_address_language_code_;

  // Models for the credit card expiration inputs.
  MonthComboboxModel cc_exp_month_combobox_model_;
  YearComboboxModel cc_exp_year_combobox_model_;

  // Models for country input.
  scoped_ptr<CountryComboboxModel> billing_country_combobox_model_;
  scoped_ptr<CountryComboboxModel> shipping_country_combobox_model_;

  // Models for the suggestion views.
  SuggestionsMenuModel suggested_cc_;
  SuggestionsMenuModel suggested_billing_;
  SuggestionsMenuModel suggested_shipping_;

  // The set of values for cc-type that the site accepts. Empty means all types
  // are accepted.
  std::set<base::string16> acceptable_cc_types_;

  // |DialogSection|s that are in edit mode that are based on existing data.
  std::set<DialogSection> section_editing_state_;

  // Sections that need to be validated when their validation rules load.
  std::set<DialogSection> needs_validation_;

  // Whether |form_structure_| has asked for any details that would indicate
  // we should show a shipping section.
  bool cares_about_shipping_;

  // Site-provided transaction amount and currency. No attempt to validate this
  // input; it's passed directly to Wallet.
  base::string16 transaction_amount_;
  base::string16 transaction_currency_;

  // The IDs for the currently showing unverified profiles popup. This will
  // be the first section in the list. The rest of the items will be the
  // i18n_validator_suggestions_.
  std::vector<std::string> popup_suggestion_ids_;

  // The autofill suggestions based on downloaded i18n validation rules.
  std::vector< ::i18n::addressinput::AddressData> i18n_validator_suggestions_;

  // The controller for the currently showing popup (which helps users when
  // they're manually filling the dialog).
  base::WeakPtr<AutofillPopupControllerImpl> popup_controller_;

  // The type of the visible Autofill popup input (or UNKNOWN_TYPE if none).
  ServerFieldType popup_input_type_;

  // The section of the dialog that's showing a popup, undefined if no popup
  // is showing.
  DialogSection popup_section_;

  scoped_ptr<AutofillDialogView> view_;

  // A NotificationRegistrar for tracking the completion of sign-in.
  content::NotificationRegistrar signin_registrar_;

  // The countries the form structure can accept for shipping.
  std::set<base::string16> acceptable_shipping_countries_;

  // Whether |callback_| was Run() with a filled |form_structure_|.
  bool data_was_passed_back_;

  typedef std::map<ServerFieldType,
      std::pair<base::string16, base::string16> > TypeErrorInputMap;
  // Whether the latency to display to the UI was logged to UMA yet.
  bool was_ui_latency_logged_;

  // A map from dialog sections to the GUID of a newly saved Autofill data
  // models for that section. No entries present that don't have newly saved
  // data models.
  std::map<DialogSection, std::string> newly_saved_data_model_guids_;

  // Populated if the user chose to save a newly inputted credit card. Used to
  // show a bubble as the dialog closes to confirm a user's new card info was
  // saved. Never populated while incognito (as nothing's actually saved).
  scoped_ptr<CreditCard> newly_saved_card_;

  // The timer that delays enabling submit button for a short period of time on
  // startup.
  base::OneShotTimer submit_button_delay_timer_;

  base::WeakPtrFactory<AutofillDialogControllerImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(AutofillDialogControllerImpl);
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_DIALOG_CONTROLLER_IMPL_H_
