// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/autofill_dialog_controller_impl.h"

#include <string>

#include "base/logging.h"
#include "base/string_split.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/autofill/autofill_country.h"
#include "chrome/browser/autofill/autofill_manager.h"
#include "chrome/browser/autofill/autofill_type.h"
#include "chrome/browser/autofill/personal_data_manager.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/autofill/wallet/wallet_service_url.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/autofill/autofill_dialog_view.h"
#include "chrome/common/form_data.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "grit/generated_resources.h"
#include "net/base/cert_status_flags.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {

namespace {

// Returns true if |input| should be shown when |field| has been requested.
bool InputTypeMatchesFieldType(const DetailInput& input,
                               const AutofillField& field) {
  // If any credit card expiration info is asked for, show both month and year
  // inputs.
  if (field.type() == CREDIT_CARD_EXP_4_DIGIT_YEAR ||
      field.type() == CREDIT_CARD_EXP_2_DIGIT_YEAR ||
      field.type() == CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR ||
      field.type() == CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR ||
      field.type() == CREDIT_CARD_EXP_MONTH) {
    return input.type == CREDIT_CARD_EXP_4_DIGIT_YEAR ||
           input.type == CREDIT_CARD_EXP_MONTH;
  }

  return input.type == field.type();
}

// Returns true if |input| should be used for a site-requested |field|.
bool DetailInputMatchesField(const DetailInput& input,
                             const AutofillField& field) {
  bool right_section = !input.section_suffix ||
      EndsWith(field.section(), input.section_suffix, false);
  return InputTypeMatchesFieldType(input, field) && right_section;
}

// Returns true if |input| should be used to fill a site-requested |field| which
// is notated with a "shipping" tag, for use when the user has decided to use
// the billing address as the shipping address.
bool DetailInputMatchesShippingField(const DetailInput& input,
                                     const AutofillField& field) {
  if (input.section_suffix &&
      std::string(input.section_suffix) == "billing") {
    return InputTypeMatchesFieldType(input, field);
  }

  if (field.type() == NAME_FULL)
    return input.type == CREDIT_CARD_NAME;

  return DetailInputMatchesField(input, field);
}

// Looks through |input_template| for the types in |requested_data|. Appends
// DetailInput values to |inputs|.
void FilterInputs(const FormStructure& form_structure,
                  const DetailInput* input_template,
                  size_t template_size,
                  DetailInputs* inputs) {
  for (size_t i = 0; i < template_size; ++i) {
    const DetailInput* input = &input_template[i];
    for (size_t j = 0; j < form_structure.field_count(); ++j) {
      if (DetailInputMatchesField(*input, *form_structure.field(j))) {
        inputs->push_back(*input);
        break;
      }
    }
  }
}

// Uses |group| to fill in the |autofilled_value| for all inputs in |all_inputs|
// (an out-param).
void FillInputFromFormGroup(FormGroup* group, DetailInputs* inputs) {
  const std::string app_locale = AutofillCountry::ApplicationLocale();
  for (size_t j = 0; j < inputs->size(); ++j) {
    (*inputs)[j].autofilled_value =
        group->GetInfo((*inputs)[j].type, app_locale);
  }
}

// Initializes |form_group| from user-entered data.
void FillFormGroupFromOutputs(const DetailOutputMap& detail_outputs,
                              FormGroup* form_group) {
  for (DetailOutputMap::const_iterator iter = detail_outputs.begin();
       iter != detail_outputs.end(); ++iter) {
    if (!iter->second.empty())
      form_group->SetRawInfo(iter->first->type, iter->second);
  }
}

}  // namespace

AutofillDialogController::~AutofillDialogController() {}

AutofillDialogControllerImpl::AutofillDialogControllerImpl(
    content::WebContents* contents,
    const FormData& form,
    const GURL& source_url,
    const content::SSLStatus& ssl_status,
    const base::Callback<void(const FormStructure*)>& callback)
    : profile_(Profile::FromBrowserContext(contents->GetBrowserContext())),
      contents_(contents),
      form_structure_(form),
      source_url_(source_url),
      ssl_status_(ssl_status),
      callback_(callback),
      ALLOW_THIS_IN_INITIALIZER_LIST(suggested_email_(this)),
      ALLOW_THIS_IN_INITIALIZER_LIST(suggested_cc_(this)),
      ALLOW_THIS_IN_INITIALIZER_LIST(suggested_billing_(this)),
      ALLOW_THIS_IN_INITIALIZER_LIST(suggested_shipping_(this)),
      popup_controller_(NULL),
      section_showing_popup_(SECTION_BILLING) {
  // TODO(estade): |this| should observe PersonalDataManager.
  // TODO(estade): remove duplicates from |form|?

  content::NavigationEntry* entry = contents->GetController().GetActiveEntry();
  const GURL& active_url = entry ? entry->GetURL() : web_contents()->GetURL();
  invoked_from_same_origin_ = active_url.GetOrigin() == source_url_.GetOrigin();
}

AutofillDialogControllerImpl::~AutofillDialogControllerImpl() {
  if (popup_controller_)
    popup_controller_->Hide();
}

void AutofillDialogControllerImpl::Show() {
  bool has_types = false;
  bool has_sections = false;
  form_structure_.ParseFieldTypesFromAutocompleteAttributes(&has_types,
                                                            &has_sections);
  // Fail if the author didn't specify autocomplete types.
  if (!has_types) {
    callback_.Run(NULL);
    delete this;
    return;
  }

  int row_id = 0;

  const DetailInput kEmailInputs[] = {
    { ++row_id, EMAIL_ADDRESS, "Email address" },
  };

  const DetailInput kCCInputs[] = {
    { ++row_id, CREDIT_CARD_NUMBER, "Card number" },
    { ++row_id, CREDIT_CARD_EXP_MONTH },
    {   row_id, CREDIT_CARD_EXP_4_DIGIT_YEAR },
    {   row_id, CREDIT_CARD_VERIFICATION_CODE, "CVC" },
    { ++row_id, CREDIT_CARD_NAME, "Cardholder name" },
  };

  const DetailInput kBillingInputs[] = {
    { ++row_id, ADDRESS_HOME_LINE1, "Street address", "billing" },
    { ++row_id, ADDRESS_HOME_LINE2, "Street address (optional)", "billing" },
    { ++row_id, ADDRESS_HOME_CITY, "City", "billing" },
    { ++row_id, ADDRESS_HOME_STATE, "State", "billing" },
    {   row_id, ADDRESS_HOME_ZIP, "ZIP code", "billing", 0.5 },
  };

  const DetailInput kShippingInputs[] = {
    { ++row_id, NAME_FULL, "Full name", "shipping" },
    { ++row_id, ADDRESS_HOME_LINE1, "Street address", "shipping" },
    { ++row_id, ADDRESS_HOME_LINE2, "Street address (optional)", "shipping" },
    { ++row_id, ADDRESS_HOME_CITY, "City", "shipping" },
    { ++row_id, ADDRESS_HOME_STATE, "State", "shipping" },
    {   row_id, ADDRESS_HOME_ZIP, "ZIP code", "shipping", 0.5 },
  };

  FilterInputs(form_structure_,
               kEmailInputs,
               arraysize(kEmailInputs),
               &requested_email_fields_);

  FilterInputs(form_structure_,
               kCCInputs,
               arraysize(kCCInputs),
               &requested_cc_fields_);

  FilterInputs(form_structure_,
               kBillingInputs,
               arraysize(kBillingInputs),
               &requested_billing_fields_);

  FilterInputs(form_structure_,
               kShippingInputs,
               arraysize(kShippingInputs),
               &requested_shipping_fields_);

  GenerateSuggestionsModels();

  // TODO(estade): don't show the dialog if the site didn't specify the right
  // fields. First we must figure out what the "right" fields are.
  view_.reset(AutofillDialogView::Create(this));
  view_->Show();
}

string16 AutofillDialogControllerImpl::DialogTitle() const {
  return l10n_util::GetStringUTF16(IDS_AUTOFILL_DIALOG_TITLE);
}

string16 AutofillDialogControllerImpl::EditSuggestionText() const {
  return l10n_util::GetStringUTF16(IDS_AUTOFILL_DIALOG_EDIT);
}

string16 AutofillDialogControllerImpl::UseBillingForShippingText() const {
  return l10n_util::GetStringUTF16(
      IDS_AUTOFILL_DIALOG_USE_BILLING_FOR_SHIPPING);
}

string16 AutofillDialogControllerImpl::WalletOptionText() const {
  // TODO(estade): real strings and l10n.
  return string16(ASCIIToUTF16("I love lamp."));
}

string16 AutofillDialogControllerImpl::CancelButtonText() const {
  return l10n_util::GetStringUTF16(IDS_CANCEL);
}

string16 AutofillDialogControllerImpl::ConfirmButtonText() const {
  return l10n_util::GetStringUTF16(IDS_AUTOFILL_DIALOG_SUBMIT_BUTTON);
}

string16 AutofillDialogControllerImpl::SignInText() const {
  // TODO(abodenha): real strings and l10n.
  return string16(ASCIIToUTF16("Sign in to use Google Wallet"));
}

string16 AutofillDialogControllerImpl::CancelSignInText() const {
  // TODO(abodenha): real strings and l10n.
  return string16(ASCIIToUTF16("Don't sign in."));
}

const DetailInputs& AutofillDialogControllerImpl::RequestedFieldsForSection(
    DialogSection section) const {
  switch (section) {
    case SECTION_EMAIL:
      return requested_email_fields_;
    case SECTION_CC:
      return requested_cc_fields_;
    case SECTION_BILLING:
      return requested_billing_fields_;
    case SECTION_SHIPPING:
      return requested_shipping_fields_;
  }

  NOTREACHED();
  return requested_billing_fields_;
}

ui::ComboboxModel* AutofillDialogControllerImpl::ComboboxModelForAutofillType(
    AutofillFieldType type) {
  switch (type) {
    case CREDIT_CARD_EXP_MONTH:
      return &cc_exp_month_combobox_model_;

    case CREDIT_CARD_EXP_4_DIGIT_YEAR:
      return &cc_exp_year_combobox_model_;

    default:
      return NULL;
  }
}

ui::MenuModel* AutofillDialogControllerImpl::MenuModelForSection(
    DialogSection section) {
  return SuggestionsMenuModelForSection(section);
}

string16 AutofillDialogControllerImpl::LabelForSection(DialogSection section)
    const {
  switch (section) {
    case SECTION_EMAIL:
      return l10n_util::GetStringUTF16(IDS_AUTOFILL_DIALOG_SECTION_EMAIL);
    case SECTION_CC:
      return l10n_util::GetStringUTF16(IDS_AUTOFILL_DIALOG_SECTION_CC);
    case SECTION_BILLING:
      return l10n_util::GetStringUTF16(IDS_AUTOFILL_DIALOG_SECTION_BILLING);
    case SECTION_SHIPPING:
      return l10n_util::GetStringUTF16(IDS_AUTOFILL_DIALOG_SECTION_SHIPPING);
    default:
      NOTREACHED();
      return string16();
  }
}

string16 AutofillDialogControllerImpl::SuggestionTextForSection(
    DialogSection section) {
  // When the user has clicked 'edit', don't show a suggestion (even though
  // there is a profile selected in the model).
  if (section_editing_state_[section])
    return string16();

  SuggestionsMenuModel* model = SuggestionsMenuModelForSection(section);
  std::string item_key = model->GetItemKeyAt(model->checked_item());
  if (item_key.empty())
    return string16();

  if (section == SECTION_EMAIL)
    return model->GetLabelAt(model->checked_item());

  if (section == SECTION_CC) {
    CreditCard* card = GetManager()->GetCreditCardByGUID(item_key);
    if (card)
      return card->Label();
  } else {
    // TODO(estade): This doesn't display as much info as it should (for
    // example, it should show full addresses rather than just a summary).
    AutofillProfile* profile = GetManager()->GetProfileByGUID(item_key);
    if (profile)
      return profile->Label();
  }

  // TODO(estade): The FormGroup was likely deleted while menu was showing. We
  // should not let this happen.
  return string16();
}

void AutofillDialogControllerImpl::EditClickedForSection(
    DialogSection section) {
  SuggestionsMenuModel* model = SuggestionsMenuModelForSection(section);
  DetailInputs* inputs = MutableRequestedFieldsForSection(section);

  if (section == SECTION_EMAIL) {
    // TODO(estade): shouldn't need to make this check.
    if (inputs->empty())
      return;

    (*inputs)[0].autofilled_value = model->GetLabelAt(model->checked_item());
  } else {
    std::string guid = model->GetItemKeyAt(model->checked_item());
    DCHECK(!guid.empty());

    FormGroup* form_group = section == SECTION_CC ?
        static_cast<FormGroup*>(GetManager()->GetCreditCardByGUID(guid)) :
        static_cast<FormGroup*>(GetManager()->GetProfileByGUID(guid));

    // TODO(estade): we shouldn't let this happen.
    if (!form_group)
      return;

    FillInputFromFormGroup(form_group, inputs);
  }

  section_editing_state_[section] = true;
  view_->UpdateSection(section);
}

bool AutofillDialogControllerImpl::InputIsValid(const DetailInput* input,
                                                const string16& value) {
  // TODO(estade): do more complicated checks.
  return !value.empty();
}

void AutofillDialogControllerImpl::UserEditedOrActivatedInput(
    const DetailInput* input,
    DialogSection section,
    gfx::NativeView parent_view,
    const gfx::Rect& content_bounds,
    const string16& field_contents,
    bool was_edit) {
  // If the field is edited down to empty, don't show a popup.
  if (was_edit && field_contents.empty()) {
    HidePopup();
    return;
  }

  // If the user clicks while the popup is already showing, be sure to hide
  // it.
  if (!was_edit && popup_controller_) {
    HidePopup();
    return;
  }

  std::vector<string16> popup_values, popup_labels, popup_icons;
  if (section == SECTION_CC) {
    GetManager()->GetCreditCardSuggestions(input->type,
                                           field_contents,
                                           &popup_values,
                                           &popup_labels,
                                           &popup_icons,
                                           &popup_guids_);
  } else {
    // TODO(estade): add field types from email section?
    const DetailInputs& inputs = RequestedFieldsForSection(section);
    std::vector<AutofillFieldType> field_types;
    field_types.reserve(inputs.size());
    for (DetailInputs::const_iterator iter = inputs.begin();
         iter != inputs.end(); ++iter) {
      field_types.push_back(iter->type);
    }
    GetManager()->GetProfileSuggestions(input->type,
                                        field_contents,
                                        false,
                                        field_types,
                                        &popup_values,
                                        &popup_labels,
                                        &popup_icons,
                                        &popup_guids_);
  }

  if (popup_values.empty())
    return;

  // TODO(estade): do we need separators and control rows like 'Clear
  // Form'?
  std::vector<int> popup_ids;
  for (size_t i = 0; i < popup_guids_.size(); ++i) {
    popup_ids.push_back(i);
  }

  popup_controller_ = AutofillPopupControllerImpl::GetOrCreate(
      popup_controller_, this, parent_view, content_bounds);
  popup_controller_->Show(popup_values,
                          popup_labels,
                          popup_icons,
                          popup_ids);
  section_showing_popup_ = section;
}

void AutofillDialogControllerImpl::FocusMoved() {
  HidePopup();
}

void AutofillDialogControllerImpl::ViewClosed(DialogAction action) {
  if (action == ACTION_SUBMIT) {
    FillOutputForSection(SECTION_EMAIL);
    FillOutputForSection(SECTION_CC);
    FillOutputForSection(SECTION_BILLING);
    if (view_->UseBillingForShipping()) {
      FillOutputForSectionWithComparator(
          SECTION_BILLING,
          base::Bind(DetailInputMatchesShippingField));
      FillOutputForSectionWithComparator(
          SECTION_CC,
          base::Bind(DetailInputMatchesShippingField));
    } else {
      FillOutputForSection(SECTION_SHIPPING);
    }
    callback_.Run(&form_structure_);
  } else {
    callback_.Run(NULL);
  }

  delete this;
}

DialogNotification AutofillDialogControllerImpl::CurrentNotification() const {
  if (RequestingCreditCardInfo() && !TransmissionWillBeSecure()) {
    return DialogNotification(
        DialogNotification::SECURITY_WARNING,
        l10n_util::GetStringUTF16(IDS_AUTOFILL_DIALOG_SECURITY_WARNING));
  }

  if (!invoked_from_same_origin_) {
    return DialogNotification(
        DialogNotification::SECURITY_WARNING,
        l10n_util::GetStringFUTF16(
            IDS_AUTOFILL_DIALOG_SITE_WARNING, UTF8ToUTF16(source_url_.host())));
  }

  return DialogNotification();
}

void AutofillDialogControllerImpl::StartSignInFlow() {
  DCHECK(registrar_.IsEmpty());

  content::Source<content::NavigationController> source(
      &view_->ShowSignIn());
  registrar_.Add(this, content::NOTIFICATION_NAV_ENTRY_COMMITTED, source);
}

void AutofillDialogControllerImpl::EndSignInFlow() {
  DCHECK(!registrar_.IsEmpty());
  registrar_.RemoveAll();
  view_->HideSignIn();
}

Profile* AutofillDialogControllerImpl::profile() {
  return profile_;
}

content::WebContents* AutofillDialogControllerImpl::web_contents() {
  return contents_;
}

void AutofillDialogControllerImpl::DidSelectSuggestion(int identifier) {
  // TODO(estade): implement.
}

void AutofillDialogControllerImpl::DidAcceptSuggestion(const string16& value,
                                                       int identifier) {
  const PersonalDataManager::GUIDPair& pair = popup_guids_[identifier];

  FormGroup* form_group = section_showing_popup_ == SECTION_CC ?
      static_cast<FormGroup*>(GetManager()->GetCreditCardByGUID(pair.first)) :
      // TODO(estade): need to use the variant, |pair.second|.
      static_cast<FormGroup*>(GetManager()->GetProfileByGUID(pair.first));

  // TODO(estade): we shouldn't let this happen.
  if (!form_group)
    return;

  FillInputFromFormGroup(
      form_group,
      MutableRequestedFieldsForSection(section_showing_popup_));
  view_->UpdateSection(section_showing_popup_);

  // TODO(estade): not sure why it's necessary to do this explicitly.
  HidePopup();
}

void AutofillDialogControllerImpl::RemoveSuggestion(const string16& value,
                                                    int identifier) {
  // TODO(estade): implement.
}

void AutofillDialogControllerImpl::ClearPreviewedForm() {
  // TODO(estade): implement.
}

void AutofillDialogControllerImpl::ControllerDestroyed() {
  popup_controller_ = NULL;
}

void AutofillDialogControllerImpl::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK_EQ(type, content::NOTIFICATION_NAV_ENTRY_COMMITTED);
  content::LoadCommittedDetails* load_details =
      content::Details<content::LoadCommittedDetails>(details).ptr();
  if (wallet::IsSignInContinueUrl(load_details->entry->GetVirtualURL()))
    EndSignInFlow();
}

void AutofillDialogControllerImpl::SuggestionItemSelected(
    const SuggestionsMenuModel& model) {
  DialogSection section = SectionForSuggestionsMenuModel(model);
  section_editing_state_[section] = false;
  view_->UpdateSection(section);
}

bool AutofillDialogControllerImpl::HandleKeyPressEventInInput(
    const content::NativeWebKeyboardEvent& event) {
  if (popup_controller_)
    return popup_controller_->HandleKeyPressEvent(event);

  return false;
}

bool AutofillDialogControllerImpl::RequestingCreditCardInfo() const {
  DCHECK_GT(form_structure_.field_count(), 0U);

  for (size_t i = 0; i < form_structure_.field_count(); ++i) {
    if (AutofillType(form_structure_.field(i)->type()).group() ==
        AutofillType::CREDIT_CARD) {
      return true;
    }
  }

  return false;
}

bool AutofillDialogControllerImpl::TransmissionWillBeSecure() const {
  return source_url_.SchemeIs(chrome::kHttpsScheme) &&
         !net::IsCertStatusError(ssl_status_.cert_status) &&
         !net::IsCertStatusMinorError(ssl_status_.cert_status);
}

void AutofillDialogControllerImpl::GenerateSuggestionsModels() {
  PersonalDataManager* manager = GetManager();
  const std::vector<CreditCard*>& cards = manager->credit_cards();
  for (size_t i = 0; i < cards.size(); ++i) {
    suggested_cc_.AddKeyedItem(cards[i]->guid(), cards[i]->Label());
  }
  // TODO(estade): real strings and i18n.
  suggested_cc_.AddKeyedItem("", ASCIIToUTF16("Enter new card"));

  const std::vector<AutofillProfile*>& profiles = manager->GetProfiles();
  const std::string app_locale = AutofillCountry::ApplicationLocale();
  for (size_t i = 0; i < profiles.size(); ++i) {
    if (!IsCompleteProfile(*profiles[i]))
      continue;

    // Add all email addresses.
    std::vector<string16> values;
    profiles[i]->GetMultiInfo(EMAIL_ADDRESS, app_locale, &values);
    for (size_t j = 0; j < values.size(); ++j) {
      if (!values[j].empty())
        suggested_email_.AddKeyedItem(profiles[i]->guid(), values[j]);
    }

    // Don't add variants for addresses: the email variants are handled above,
    // name is part of credit card and we'll just ignore phone number variants.
    suggested_billing_.AddKeyedItem(profiles[i]->guid(), profiles[i]->Label());
    suggested_shipping_.AddKeyedItem(profiles[i]->guid(), profiles[i]->Label());
  }

  // TODO(estade): real strings and i18n.
  suggested_billing_.AddKeyedItem("", ASCIIToUTF16("Enter new billing"));
  suggested_email_.AddKeyedItem("", ASCIIToUTF16("Enter new email"));
  suggested_shipping_.AddKeyedItem("", ASCIIToUTF16("Enter new shipping"));
}

bool AutofillDialogControllerImpl::IsCompleteProfile(
    const AutofillProfile& profile) {
  const std::string app_locale = AutofillCountry::ApplicationLocale();
  for (size_t i = 0; i < requested_shipping_fields_.size(); ++i) {
    if (profile.GetInfo(requested_shipping_fields_[i].type,
                        app_locale).empty()) {
      return false;
    }
  }

  return true;
}

void AutofillDialogControllerImpl::FillOutputForSectionWithComparator(
    DialogSection section,
    const InputFieldComparator& compare) {
  SuggestionsMenuModel* model = SuggestionsMenuModelForSection(section);
  std::string guid = model->GetItemKeyAt(model->checked_item());
  PersonalDataManager* manager = GetManager();
  if (!guid.empty() && !section_editing_state_[section]) {
    FormGroup* form_group = section == SECTION_CC ?
        static_cast<FormGroup*>(manager->GetCreditCardByGUID(guid)) :
        static_cast<FormGroup*>(manager->GetProfileByGUID(guid));
    // TODO(estade): we shouldn't let this happen.
    if (!form_group)
      return;

    // Calculate the variant by looking at how many items come from the same
    // FormGroup. TODO(estade): add a test for this.
    size_t variant = 0;
    for (int i = model->checked_item() - 1; i >= 0; --i) {
      if (model->GetItemKeyAt(i) == guid)
        variant++;
      else
        break;
    }

    FillFormStructureForSection(*form_group, variant, section, compare);
  } else {
    // The user manually input data.
    DetailOutputMap output;
    view_->GetUserInput(section, &output);

    // Save the info as new or edited data, then fill it into |form_structure_|.
    if (section == SECTION_CC) {
      CreditCard card;
      FillFormGroupFromOutputs(output, &card);
      manager->SaveImportedCreditCard(card);
      FillFormStructureForSection(card, 0, section, compare);

      // CVC needs special-casing because the CreditCard class doesn't store
      // or handle them. Fill it in directly from |output|.
      for (size_t i = 0; i < form_structure_.field_count(); ++i) {
        AutofillField* field = form_structure_.field(i);
        if (field->type() != CREDIT_CARD_VERIFICATION_CODE)
          continue;

        for (DetailOutputMap::iterator iter = output.begin();
             iter != output.end(); ++iter) {
          if (!iter->second.empty() && compare.Run(*iter->first, *field)) {
            field->value = iter->second;
            break;
          }
        }
      }
    } else {
      AutofillProfile profile;
      FillFormGroupFromOutputs(output, &profile);
      // TODO(estade): we should probably edit the existing profile in the cases
      // where the input data is based on an existing profile (user clicked
      // "Edit" or autofill popup filled in the form).
      manager->SaveImportedProfile(profile);
      FillFormStructureForSection(profile, 0, section, compare);
    }
  }
}

void AutofillDialogControllerImpl::FillOutputForSection(DialogSection section) {
  FillOutputForSectionWithComparator(section,
                                     base::Bind(DetailInputMatchesField));
}

void AutofillDialogControllerImpl::FillFormStructureForSection(
    const FormGroup& form_group,
    size_t variant,
    DialogSection section,
    const InputFieldComparator& compare) {
  for (size_t i = 0; i < form_structure_.field_count(); ++i) {
    AutofillField* field = form_structure_.field(i);
    // Only fill in data that is associated with this section.
    const DetailInputs& inputs = RequestedFieldsForSection(section);
    for (size_t j = 0; j < inputs.size(); ++j) {
      if (compare.Run(inputs[j], *field)) {
        form_group.FillFormField(*field, variant, field);
        break;
      }
    }
  }
}

SuggestionsMenuModel* AutofillDialogControllerImpl::
    SuggestionsMenuModelForSection(DialogSection section) {
  switch (section) {
    case SECTION_EMAIL:
      return &suggested_email_;
    case SECTION_CC:
      return &suggested_cc_;
    case SECTION_BILLING:
      return &suggested_billing_;
    case SECTION_SHIPPING:
      return &suggested_shipping_;
  }

  NOTREACHED();
  return NULL;
}

DialogSection AutofillDialogControllerImpl::SectionForSuggestionsMenuModel(
    const SuggestionsMenuModel& model) {
  if (&model == &suggested_email_)
    return SECTION_EMAIL;

  if (&model == &suggested_cc_)
    return SECTION_CC;

  if (&model == &suggested_billing_)
    return SECTION_BILLING;

  DCHECK_EQ(&model, &suggested_shipping_);
  return SECTION_SHIPPING;
}

PersonalDataManager* AutofillDialogControllerImpl::GetManager() {
  return PersonalDataManagerFactory::GetForProfile(profile_);
}

DetailInputs* AutofillDialogControllerImpl::MutableRequestedFieldsForSection(
    DialogSection section) {
  return const_cast<DetailInputs*>(&RequestedFieldsForSection(section));
}

void AutofillDialogControllerImpl::HidePopup() {
  if (popup_controller_) {
    popup_controller_->Hide();
    ControllerDestroyed();
  }
}

}  // namespace autofill
