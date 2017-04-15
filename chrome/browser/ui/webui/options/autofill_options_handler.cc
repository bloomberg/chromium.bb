// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options/autofill_options_handler.h"

#include <stddef.h>

#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/guid.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/autofill/core/browser/autofill_address_util.h"
#include "components/autofill/core/browser/autofill_data_util.h"
#include "components/autofill/core/browser/autofill_profile.h"
#include "components/autofill/core/browser/country_combobox_model.h"
#include "components/autofill/core/browser/credit_card.h"
#include "components/autofill/core/browser/payments/payments_service_url.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/phone_number_i18n.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/autofill_switches.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_ui.h"
#include "third_party/libaddressinput/messages.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/webui/web_ui_util.h"

using autofill::AutofillType;
using autofill::AutofillProfile;
using autofill::CreditCard;
using autofill::PersonalDataManager;

namespace {

static const char kComponents[] = "components";
static const char kLanguageCode[] = "languageCode";

std::unique_ptr<base::DictionaryValue> CreditCardToDictionary(
    const CreditCard& card) {
  std::unique_ptr<base::DictionaryValue> value(new base::DictionaryValue);
  value->SetString("guid", card.guid());
  std::pair<base::string16, base::string16> label_pieces = card.LabelPieces();
  value->SetString("label", label_pieces.first);
  value->SetString("sublabel", label_pieces.second);
  value->SetBoolean("isLocal", card.record_type() == CreditCard::LOCAL_CARD);
  value->SetBoolean("isCached",
                    card.record_type() == CreditCard::FULL_SERVER_CARD);
  return value;
}

}  // namespace

namespace options {

AutofillOptionsHandler::AutofillOptionsHandler() : personal_data_(nullptr) {}

AutofillOptionsHandler::~AutofillOptionsHandler() {
  if (personal_data_)
    personal_data_->RemoveObserver(this);
}

/////////////////////////////////////////////////////////////////////////////
// OptionsPageUIHandler implementation:
void AutofillOptionsHandler::GetLocalizedValues(
    base::DictionaryValue* localized_strings) {
  DCHECK(localized_strings);

  static OptionsStringResource resources[] = {
    { "autofillAddresses", IDS_AUTOFILL_ADDRESSES_GROUP_NAME },
    { "autofillCreditCards", IDS_AUTOFILL_CREDITCARDS_GROUP_NAME },
    { "autofillAddAddress", IDS_AUTOFILL_ADD_ADDRESS_BUTTON },
    { "autofillAddCreditCard", IDS_AUTOFILL_ADD_CREDITCARD_BUTTON },
    { "autofillEditProfileButton", IDS_AUTOFILL_EDIT_PROFILE_BUTTON },
    { "autofillFromGoogleAccount", IDS_AUTOFILL_FROM_GOOGLE_ACCOUNT },
    { "autofillDescribeLocalCopy", IDS_AUTOFILL_DESCRIBE_LOCAL_COPY },
    { "autofillClearLocalCopyButton", IDS_AUTOFILL_CLEAR_LOCAL_COPY_BUTTON },
    { "helpButton", IDS_AUTOFILL_HELP_LABEL },
    { "addAddressTitle", IDS_AUTOFILL_ADD_ADDRESS_CAPTION },
    { "editAddressTitle", IDS_AUTOFILL_EDIT_ADDRESS_CAPTION },
    { "addCreditCardTitle", IDS_AUTOFILL_ADD_CREDITCARD_CAPTION },
    { "editCreditCardTitle", IDS_AUTOFILL_EDIT_CREDITCARD_CAPTION },
  };

  RegisterStrings(localized_strings, resources, arraysize(resources));
  RegisterTitle(localized_strings, "autofillOptionsPage",
                IDS_AUTOFILL_OPTIONS_TITLE);

  localized_strings->SetString("helpUrl", autofill::kHelpURL);

  personal_data_ = autofill::PersonalDataManagerFactory::GetForProfile(
      Profile::FromWebUI(web_ui()));

  SetAddressOverlayStrings(localized_strings);
  SetCreditCardOverlayStrings(localized_strings);

  localized_strings->SetString(
      "paymentsManageAddressesUrl",
      autofill::payments::GetManageAddressesUrl(0).spec());
  localized_strings->SetString(
      "paymentsManageInstrumentsUrl",
      autofill::payments::GetManageInstrumentsUrl(0).spec());
}

void AutofillOptionsHandler::InitializeHandler() {
  // personal_data_ is NULL in guest mode on Chrome OS.
  if (personal_data_)
    personal_data_->AddObserver(this);
}

void AutofillOptionsHandler::InitializePage() {
  if (personal_data_)
    LoadAutofillData();
}

void AutofillOptionsHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "removeData",
      base::Bind(&AutofillOptionsHandler::RemoveData,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "loadAddressEditor",
      base::Bind(&AutofillOptionsHandler::LoadAddressEditor,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "loadAddressEditorComponents",
      base::Bind(&AutofillOptionsHandler::LoadAddressEditorComponents,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "loadCreditCardEditor",
      base::Bind(&AutofillOptionsHandler::LoadCreditCardEditor,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "setAddress",
      base::Bind(&AutofillOptionsHandler::SetAddress, base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "setCreditCard",
      base::Bind(&AutofillOptionsHandler::SetCreditCard,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "clearLocalCardCopy",
      base::Bind(&AutofillOptionsHandler::RemaskServerCard,
                 base::Unretained(this)));
}

/////////////////////////////////////////////////////////////////////////////
// PersonalDataManagerObserver implementation:
void AutofillOptionsHandler::OnPersonalDataChanged() {
  LoadAutofillData();
}

void AutofillOptionsHandler::SetAddressOverlayStrings(
    base::DictionaryValue* localized_strings) {
  localized_strings->SetString("autofillEditAddressTitle",
      l10n_util::GetStringUTF16(IDS_AUTOFILL_EDIT_ADDRESS_CAPTION));
  localized_strings->SetString("autofillCountryLabel",
      l10n_util::GetStringUTF16(IDS_LIBADDRESSINPUT_COUNTRY_OR_REGION_LABEL));
  localized_strings->SetString("autofillPhoneLabel",
      l10n_util::GetStringUTF16(IDS_AUTOFILL_FIELD_LABEL_PHONE));
  localized_strings->SetString("autofillEmailLabel",
      l10n_util::GetStringUTF16(IDS_AUTOFILL_FIELD_LABEL_EMAIL));
  SetCountryData(*personal_data_, localized_strings,
                 g_browser_process->GetApplicationLocale());
}

void AutofillOptionsHandler::SetCreditCardOverlayStrings(
    base::DictionaryValue* localized_strings) {
  localized_strings->SetString("autofillEditCreditCardTitle",
      l10n_util::GetStringUTF16(IDS_AUTOFILL_EDIT_CREDITCARD_CAPTION));
  localized_strings->SetString("nameOnCardLabel",
      l10n_util::GetStringUTF16(IDS_AUTOFILL_FIELD_LABEL_NAME_ON_CARD));
  localized_strings->SetString("creditCardNumberLabel",
      l10n_util::GetStringUTF16(IDS_AUTOFILL_FIELD_LABEL_CREDIT_CARD_NUMBER));
  localized_strings->SetString("creditCardExpirationDateLabel",
      l10n_util::GetStringUTF16(IDS_AUTOFILL_FIELD_LABEL_EXPIRATION_DATE));
}

void AutofillOptionsHandler::LoadAutofillData() {
  if (!IsPersonalDataLoaded())
    return;

  const std::vector<AutofillProfile*>& profiles = personal_data_->GetProfiles();
  std::vector<base::string16> labels;
  AutofillProfile::CreateDifferentiatingLabels(
      profiles,
      g_browser_process->GetApplicationLocale(),
      &labels);
  DCHECK_EQ(labels.size(), profiles.size());

  base::ListValue addresses;
  for (size_t i = 0; i < profiles.size(); ++i) {
    // Skip showing auxiliary profiles (e.g. Mac Contacts) for now.
    if (profiles[i]->record_type() == AutofillProfile::AUXILIARY_PROFILE)
      continue;

    base::string16 separator =
        l10n_util::GetStringUTF16(IDS_AUTOFILL_ADDRESS_SUMMARY_SEPARATOR);
    std::vector<base::string16> label_parts = base::SplitStringUsingSubstr(
        labels[i], separator, base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);

    std::unique_ptr<base::DictionaryValue> value(new base::DictionaryValue);
    value->SetString("guid", profiles[i]->guid());
    value->SetString("label", label_parts[0]);
    value->SetString("sublabel", labels[i].substr(label_parts[0].size()));
    value->SetBoolean("isLocal", profiles[i]->record_type() ==
                                     AutofillProfile::LOCAL_PROFILE);
    addresses.Append(std::move(value));
  }

  web_ui()->CallJavascriptFunctionUnsafe("AutofillOptions.setAddressList",
                                         addresses);

  base::ListValue credit_cards;
  const std::vector<CreditCard*>& cards = personal_data_->GetCreditCards();
  for (const CreditCard* card : cards) {
    credit_cards.Append(CreditCardToDictionary(*card));
  }

  web_ui()->CallJavascriptFunctionUnsafe("AutofillOptions.setCreditCardList",
                                         credit_cards);
}

void AutofillOptionsHandler::RemoveData(const base::ListValue* args) {
  DCHECK(IsPersonalDataLoaded());

  std::string guid;
  if (!args->GetString(0, &guid)) {
    NOTREACHED();
    return;
  }

  personal_data_->RemoveByGUID(guid);
}

void AutofillOptionsHandler::LoadAddressEditor(const base::ListValue* args) {
  DCHECK(IsPersonalDataLoaded());

  std::string guid;
  if (!args->GetString(0, &guid)) {
    NOTREACHED();
    return;
  }

  const AutofillProfile* prior_profile = personal_data_->GetProfileByGUID(guid);
  if (!prior_profile) {
    // There is a race where a user can click once on the close button and
    // quickly click again on the list item before the item is removed (since
    // the list is not updated until the model tells the list an item has been
    // removed). This will activate the editor for a profile that has been
    // removed. Do nothing in that case.
    return;
  }

  base::DictionaryValue address;
  AutofillProfileToDictionary(*prior_profile, &address);

  web_ui()->CallJavascriptFunctionUnsafe("AutofillOptions.editAddress",
                                         address);
}

void AutofillOptionsHandler::LoadAddressEditorComponents(
    const base::ListValue* args) {
  std::string country_code;
  if (!args->GetString(0, &country_code)) {
    NOTREACHED();
    return;
  }

  base::DictionaryValue input;
  std::unique_ptr<base::ListValue> components(new base::ListValue);
  std::string language_code;
  autofill::GetAddressComponents(country_code,
                                 g_browser_process->GetApplicationLocale(),
                                 components.get(), &language_code);
  input.Set(kComponents, components.release());
  input.SetString(kLanguageCode, language_code);

  web_ui()->CallJavascriptFunctionUnsafe(
      "AutofillEditAddressOverlay.loadAddressComponents", input);
}

void AutofillOptionsHandler::LoadCreditCardEditor(const base::ListValue* args) {
  DCHECK(IsPersonalDataLoaded());

  std::string guid;
  if (!args->GetString(0, &guid)) {
    NOTREACHED();
    return;
  }

  CreditCard* credit_card = personal_data_->GetCreditCardByGUID(guid);
  if (!credit_card) {
    // There is a race where a user can click once on the close button and
    // quickly click again on the list item before the item is removed (since
    // the list is not updated until the model tells the list an item has been
    // removed). This will activate the editor for a profile that has been
    // removed. Do nothing in that case.
    return;
  }

  base::DictionaryValue credit_card_data;
  credit_card_data.SetString("guid", credit_card->guid());
  credit_card_data.SetString(
      "nameOnCard", credit_card->GetRawInfo(autofill::CREDIT_CARD_NAME_FULL));
  credit_card_data.SetString(
      "creditCardNumber",
      credit_card->GetRawInfo(autofill::CREDIT_CARD_NUMBER));
  credit_card_data.SetString(
      "expirationMonth",
      credit_card->GetRawInfo(autofill::CREDIT_CARD_EXP_MONTH));
  credit_card_data.SetString(
      "expirationYear",
      credit_card->GetRawInfo(autofill::CREDIT_CARD_EXP_4_DIGIT_YEAR));

  web_ui()->CallJavascriptFunctionUnsafe("AutofillOptions.editCreditCard",
                                         credit_card_data);
}

void AutofillOptionsHandler::SetAddress(const base::ListValue* args) {
  if (!IsPersonalDataLoaded())
    return;

  int arg_counter = 0;
  std::string guid;
  if (!args->GetString(arg_counter++, &guid)) {
    NOTREACHED();
    return;
  }

  AutofillProfile profile(guid, autofill::kSettingsOrigin);

  base::string16 full_name;
  if (args->GetString(arg_counter++, &full_name)) {
    // Although First/Middle/Last are not displayed on the form, we transfer
    // this information when they match the full name in the old version of the
    // profile, if one exists. This is because it may not be possible later to
    // correctly tokenize the concatenated full name; e.g., when the last name
    // contains a space, the first word would be treated as a middle name.
    const AutofillProfile* prior_profile =
        base::IsValidGUID(profile.guid())
            ? personal_data_->GetProfileByGUID(guid)
            : nullptr;

    if (prior_profile && autofill::data_util::ProfileMatchesFullName(
                             full_name, *prior_profile)) {
      profile.SetRawInfo(autofill::NAME_FULL, full_name);

      profile.SetRawInfo(autofill::NAME_FIRST,
                         prior_profile->GetRawInfo(autofill::NAME_FIRST));
      profile.SetRawInfo(autofill::NAME_MIDDLE,
                         prior_profile->GetRawInfo(autofill::NAME_MIDDLE));
      profile.SetRawInfo(autofill::NAME_LAST,
                         prior_profile->GetRawInfo(autofill::NAME_LAST));
    } else {
      // In contrast to SetRawInfo, SetInfo will naively attempt to populate the
      // First/Middle/Last fields by tokenization.
      profile.SetInfo(AutofillType(autofill::NAME_FULL), full_name,
                      g_browser_process->GetApplicationLocale());
    }
  }

  base::string16 value;
  if (args->GetString(arg_counter++, &value))
    profile.SetRawInfo(autofill::COMPANY_NAME, value);

  if (args->GetString(arg_counter++, &value))
    profile.SetRawInfo(autofill::ADDRESS_HOME_STREET_ADDRESS, value);

  if (args->GetString(arg_counter++, &value))
    profile.SetRawInfo(autofill::ADDRESS_HOME_DEPENDENT_LOCALITY, value);

  if (args->GetString(arg_counter++, &value))
    profile.SetRawInfo(autofill::ADDRESS_HOME_CITY, value);

  if (args->GetString(arg_counter++, &value))
    profile.SetRawInfo(autofill::ADDRESS_HOME_STATE, value);

  if (args->GetString(arg_counter++, &value))
    profile.SetRawInfo(autofill::ADDRESS_HOME_ZIP, value);

  if (args->GetString(arg_counter++, &value))
    profile.SetRawInfo(autofill::ADDRESS_HOME_SORTING_CODE, value);

  if (args->GetString(arg_counter++, &value))
    profile.SetRawInfo(autofill::ADDRESS_HOME_COUNTRY, value);

  if (args->GetString(arg_counter++, &value))
    profile.SetRawInfo(autofill::PHONE_HOME_WHOLE_NUMBER, value);

  if (args->GetString(arg_counter++, &value))
    profile.SetRawInfo(autofill::EMAIL_ADDRESS, value);

  if (args->GetString(arg_counter++, &value))
    profile.set_language_code(base::UTF16ToUTF8(value));

  if (!base::IsValidGUID(profile.guid())) {
    profile.set_guid(base::GenerateGUID());
    personal_data_->AddProfile(profile);
  } else {
    personal_data_->UpdateProfile(profile);
  }
}

void AutofillOptionsHandler::SetCreditCard(const base::ListValue* args) {
  if (!IsPersonalDataLoaded())
    return;

  std::string guid;
  if (!args->GetString(0, &guid)) {
    NOTREACHED();
    return;
  }

  CreditCard credit_card(guid, autofill::kSettingsOrigin);

  base::string16 value;
  if (args->GetString(1, &value))
    credit_card.SetRawInfo(autofill::CREDIT_CARD_NAME_FULL, value);

  if (args->GetString(2, &value))
    credit_card.SetRawInfo(autofill::CREDIT_CARD_NUMBER, value);

  if (args->GetString(3, &value))
    credit_card.SetRawInfo(autofill::CREDIT_CARD_EXP_MONTH, value);

  if (args->GetString(4, &value))
    credit_card.SetRawInfo(autofill::CREDIT_CARD_EXP_4_DIGIT_YEAR, value);

  if (!base::IsValidGUID(credit_card.guid())) {
    credit_card.set_guid(base::GenerateGUID());
    personal_data_->AddCreditCard(credit_card);
  } else {
    personal_data_->UpdateCreditCard(credit_card);
  }
}

void AutofillOptionsHandler::RemaskServerCard(const base::ListValue* args) {
  std::string guid;
  if (!args->GetString(0, &guid)) {
    NOTREACHED();
    return;
  }

  personal_data_->ResetFullServerCard(guid);
}

bool AutofillOptionsHandler::IsPersonalDataLoaded() const {
  return personal_data_ && personal_data_->IsDataLoaded();
}

// static
void AutofillOptionsHandler::AutofillProfileToDictionary(
    const autofill::AutofillProfile& profile,
    base::DictionaryValue* address) {
  address->SetString("guid", profile.guid());
  address->SetString(
      autofill::kFullNameField,
      profile.GetInfo(AutofillType(autofill::NAME_FULL),
                      g_browser_process->GetApplicationLocale()));
  address->SetString(autofill::kCompanyNameField,
                     profile.GetRawInfo(autofill::COMPANY_NAME));
  address->SetString(autofill::kAddressLineField,
                     profile.GetRawInfo(autofill::ADDRESS_HOME_STREET_ADDRESS));
  address->SetString(autofill::kCityField,
                     profile.GetRawInfo(autofill::ADDRESS_HOME_CITY));
  address->SetString(autofill::kStateField,
                     profile.GetRawInfo(autofill::ADDRESS_HOME_STATE));
  address->SetString(
      autofill::kDependentLocalityField,
      profile.GetRawInfo(autofill::ADDRESS_HOME_DEPENDENT_LOCALITY));
  address->SetString(autofill::kSortingCodeField,
                     profile.GetRawInfo(autofill::ADDRESS_HOME_SORTING_CODE));
  address->SetString(autofill::kPostalCodeField,
                     profile.GetRawInfo(autofill::ADDRESS_HOME_ZIP));
  address->SetString(autofill::kCountryField,
                     profile.GetRawInfo(autofill::ADDRESS_HOME_COUNTRY));
  address->SetString("phone",
                     profile.GetRawInfo(autofill::PHONE_HOME_WHOLE_NUMBER));
  address->SetString("email", profile.GetRawInfo(autofill::EMAIL_ADDRESS));
  address->SetString(kLanguageCode, profile.language_code());

  std::unique_ptr<base::ListValue> components(new base::ListValue);
  autofill::GetAddressComponents(
      base::UTF16ToUTF8(profile.GetRawInfo(autofill::ADDRESS_HOME_COUNTRY)),
      profile.language_code(), components.get(), nullptr);
  address->Set(kComponents, components.release());
}

}  // namespace options
