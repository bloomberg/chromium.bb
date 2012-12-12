// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/autofill_dialog_controller.h"

#include "base/logging.h"
#include "base/string_split.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/autofill/autofill_country.h"
#include "chrome/browser/autofill/autofill_manager.h"
#include "chrome/browser/autofill/autofill_type.h"
#include "chrome/browser/autofill/personal_data_manager.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/autofill/autofill_dialog_view.h"
#include "chrome/common/form_data.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "googleurl/src/gurl.h"
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

// Finds the instance of T which has the most data for filling in the inputs in
// |all_inputs|. T should be a FormGroup type.
template<class T> FormGroup* GetBestDataSource(
    const std::vector<T*>& sources,
    const DetailInputs* const* all_inputs,
    size_t all_inputs_length) {
  const std::string app_locale = AutofillCountry::ApplicationLocale();

  int best_field_fill_count = 0;
  FormGroup* best_source = NULL;

  for (size_t i = 0; i < sources.size(); ++i) {
    int field_fill_count = 0;

    for (size_t j = 0; j < all_inputs_length; ++j) {
      const DetailInputs& inputs = *all_inputs[j];
      for (size_t k = 0; k < inputs.size(); ++k) {
        if (!sources[i]->GetInfo(inputs[k].type, app_locale).empty())
          field_fill_count++;
      }
    }

    // TODO(estade): should there be a better tiebreaker?
    if (field_fill_count > best_field_fill_count) {
      best_field_fill_count = field_fill_count;
      best_source = sources[i];
    }
  }

  return best_source;
}

// Uses |group| to fill in the |starting_value| for all inputs in |all_inputs|
// (an out-param).
void FillInputsFromFormGroup(FormGroup* group,
                             DetailInputs** all_inputs,
                             size_t all_inputs_length) {
  const std::string app_locale = AutofillCountry::ApplicationLocale();
  for (size_t i = 0; i < all_inputs_length; ++i) {
    DetailInputs& inputs = *all_inputs[i];
    for (size_t j = 0; j < inputs.size(); ++j) {
      inputs[j].starting_value = group->GetInfo(inputs[j].type, app_locale);
    }
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

AutofillDialogController::AutofillDialogController(
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
      callback_(callback) {
  // TODO(estade): |this| should observe PersonalDataManager.
  // TODO(estade): remove duplicates from |form|?
}

AutofillDialogController::~AutofillDialogController() {}

void AutofillDialogController::Show() {
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

  // TODO(estade): make this actually check if it's a first run.
  bool first_run = true;
  if (first_run)
    PopulateInputsWithGuesses();
  else
    GenerateComboboxModels();

  // TODO(estade): don't show the dialog if the site didn't specify the right
  // fields. First we must figure out what the "right" fields are.
  view_.reset(AutofillDialogView::Create(this));
  view_->Show();
}

string16 AutofillDialogController::DialogTitle() const {
  return l10n_util::GetStringUTF16(IDS_AUTOFILL_DIALOG_TITLE);
}

string16 AutofillDialogController::SecurityWarning() const {
  return ShouldShowSecurityWarning() ?
      l10n_util::GetStringUTF16(IDS_AUTOFILL_DIALOG_SECURITY_WARNING) :
      string16();
}

string16 AutofillDialogController::SiteLabel() const {
  return UTF8ToUTF16(source_url_.host());
}

string16 AutofillDialogController::IntroText() const {
  return l10n_util::GetStringFUTF16(IDS_AUTOFILL_DIALOG_SITE_WARNING,
                                    SiteLabel());
}

std::pair<string16, string16>
    AutofillDialogController::GetIntroTextParts() const {
  const char16 kFakeSite = '$';
  std::vector<string16> pieces;
  base::SplitStringDontTrim(
      l10n_util::GetStringFUTF16(IDS_AUTOFILL_DIALOG_SITE_WARNING,
                                 string16(1, kFakeSite)),
      kFakeSite,
      &pieces);
  return std::make_pair(pieces[0], pieces[1]);
}

string16 AutofillDialogController::LabelForSection(DialogSection section)
    const {
  switch (section) {
    case SECTION_EMAIL:
      return l10n_util::GetStringUTF16(IDS_AUTOFILL_DIALOG_SECTION_EMAIL);
    case SECTION_BILLING:
      return l10n_util::GetStringUTF16(IDS_AUTOFILL_DIALOG_SECTION_BILLING);
    case SECTION_SHIPPING:
      return l10n_util::GetStringUTF16(IDS_AUTOFILL_DIALOG_SECTION_SHIPPING);
    default:
      NOTREACHED();
      return string16();
  }
}

string16 AutofillDialogController::UseBillingForShippingText() const {
  return l10n_util::GetStringUTF16(
      IDS_AUTOFILL_DIALOG_USE_BILLING_FOR_SHIPPING);
}

string16 AutofillDialogController::WalletOptionText() const {
  // TODO(estade): real strings and l10n.
  return string16(ASCIIToUTF16("I love lamp."));
}

string16 AutofillDialogController::CancelButtonText() const {
  return l10n_util::GetStringUTF16(IDS_CANCEL);
}

string16 AutofillDialogController::ConfirmButtonText() const {
  return l10n_util::GetStringUTF16(IDS_AUTOFILL_DIALOG_SUBMIT_BUTTON);
}

bool AutofillDialogController::ConfirmButtonEnabled() const {
  // TODO(estade): implement.
  return true;
}

bool AutofillDialogController::RequestingCreditCardInfo() const {
  DCHECK_GT(form_structure_.field_count(), 0U);

  for (size_t i = 0; i < form_structure_.field_count(); ++i) {
    if (AutofillType(form_structure_.field(i)->type()).group() ==
        AutofillType::CREDIT_CARD) {
      return true;
    }
  }

  return false;
}

bool AutofillDialogController::ShouldShowSecurityWarning() const {
  return RequestingCreditCardInfo() &&
         (!source_url_.SchemeIs(chrome::kHttpsScheme) ||
          net::IsCertStatusError(ssl_status_.cert_status) ||
          net::IsCertStatusMinorError(ssl_status_.cert_status));
}

const DetailInputs& AutofillDialogController::RequestedFieldsForSection(
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
  return requested_shipping_fields_;
}

ui::ComboboxModel* AutofillDialogController::ComboboxModelForAutofillType(
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

ui::ComboboxModel* AutofillDialogController::ComboboxModelForSection(
    DialogSection section) {
  return SuggestionsModelForSection(section);
}

void AutofillDialogController::ViewClosed(DialogAction action) {
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

void AutofillDialogController::GenerateComboboxModels() {
  PersonalDataManager* manager =
      PersonalDataManagerFactory::GetForProfile(profile_);
  const std::vector<CreditCard*>& cards = manager->credit_cards();
  for (size_t i = 0; i < cards.size(); ++i) {
    suggested_cc_.AddItem(cards[i]->guid(), cards[i]->Label());
  }
  suggested_cc_.AddItem("", ASCIIToUTF16("Enter new card"));

  const std::vector<AutofillProfile*>& profiles = manager->GetProfiles();
  const std::string app_locale = AutofillCountry::ApplicationLocale();
  for (size_t i = 0; i < profiles.size(); ++i) {
    string16 email = profiles[i]->GetInfo(EMAIL_ADDRESS, app_locale);
    if (!email.empty())
      suggested_email_.AddItem(profiles[i]->guid(), email);
    suggested_billing_.AddItem(profiles[i]->guid(), profiles[i]->Label());
    suggested_shipping_.AddItem(profiles[i]->guid(), profiles[i]->Label());
  }
  suggested_billing_.AddItem("", ASCIIToUTF16("Enter new billing"));
  suggested_email_.AddItem("", ASCIIToUTF16("Enter new email"));
  suggested_shipping_.AddItem("", ASCIIToUTF16("Enter new shipping"));
}

void AutofillDialogController::PopulateInputsWithGuesses() {
  PersonalDataManager* manager =
      PersonalDataManagerFactory::GetForProfile(profile_);

  DetailInputs* profile_inputs[] = { &requested_email_fields_,
                                     &requested_billing_fields_ };
  const std::vector<AutofillProfile*>& profiles = manager->GetProfiles();
  FormGroup* best_profile =
      GetBestDataSource(profiles, profile_inputs, arraysize(profile_inputs));
  if (best_profile) {
    FillInputsFromFormGroup(best_profile,
                            profile_inputs,
                            arraysize(profile_inputs));
  }

  DetailInputs* cc_inputs[] = { &requested_cc_fields_ };
  const std::vector<CreditCard*>& credit_cards = manager->credit_cards();
  FormGroup* best_cc =
      GetBestDataSource(credit_cards, cc_inputs, arraysize(cc_inputs));
  if (best_cc) {
    FillInputsFromFormGroup(best_cc,
                            cc_inputs,
                            arraysize(cc_inputs));
  }
}

void AutofillDialogController::FillOutputForSectionWithComparator(
    DialogSection section, const InputFieldComparator& compare) {
  int suggestion_selection = view_->GetSuggestionSelection(section);
  SuggestionsComboboxModel* model = SuggestionsModelForSection(section);
  if (suggestion_selection < model->GetItemCount() - 1) {
    std::string guid = model->GetItemKeyAt(suggestion_selection);
    PersonalDataManager* manager =
        PersonalDataManagerFactory::GetForProfile(profile_);
    FormGroup* form_group = section == SECTION_CC ?
        static_cast<FormGroup*>(manager->GetCreditCardByGUID(guid)) :
        static_cast<FormGroup*>(manager->GetProfileByGUID(guid));
    // TODO(estade): we shouldn't let this happen.
    if (!form_group)
      return;

    FillFormStructureForSection(*form_group, section, compare);
  } else {
    // The user manually input data.
    DetailOutputMap output;
    view_->GetUserInput(section, &output);

    // Save the info as new or edited data, then fill it into |form_structure_|.
    PersonalDataManager* manager =
        PersonalDataManagerFactory::GetForProfile(profile_);
    if (section == SECTION_CC) {
      CreditCard card;
      FillFormGroupFromOutputs(output, &card);
      manager->SaveImportedCreditCard(card);
      FillFormStructureForSection(card, section, compare);

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
      manager->SaveImportedProfile(profile);
      FillFormStructureForSection(profile, section, compare);
    }
  }
}

void AutofillDialogController::FillOutputForSection(DialogSection section) {
  FillOutputForSectionWithComparator(section,
                                     base::Bind(DetailInputMatchesField));
}

void AutofillDialogController::FillFormStructureForSection(
    const FormGroup& form_group,
    DialogSection section,
    const InputFieldComparator& compare) {
  for (size_t i = 0; i < form_structure_.field_count(); ++i) {
    AutofillField* field = form_structure_.field(i);
    // Only fill in data that is associated with this section.
    const DetailInputs& inputs = RequestedFieldsForSection(section);
    for (size_t j = 0; j < inputs.size(); ++j) {
      if (compare.Run(inputs[j], *field)) {
        form_group.FillFormField(*field, 0, field);
        break;
      }
    }
  }
}

SuggestionsComboboxModel* AutofillDialogController::SuggestionsModelForSection(
    DialogSection section) {
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

}  // namespace autofill
