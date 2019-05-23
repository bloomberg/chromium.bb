// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/credit_card_accessory_controller_impl.h"

#include <iterator>
#include <vector>

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/autofill/manual_filling_controller.h"
#include "chrome/browser/autofill/manual_filling_utils.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {

namespace {

base::string16 GetTitle(bool unused) {
  return l10n_util::GetStringUTF16(IDS_MANUAL_FILLING_CREDIT_CARD_SHEET_TITLE);
}

void AddField(const base::string16& data, UserInfo* user_info) {
  user_info->add_field(UserInfo::Field(data, data,
                                       /*is_password=*/false,
                                       /*selectable=*/true));
}

UserInfo Translate(const CreditCard* data) {
  DCHECK(data);

  UserInfo user_info;

  AddField(data->ObfuscatedLastFourDigits(), &user_info);

  if (data->HasValidExpirationDate()) {
    // TOOD(crbug.com/902425): Pass expiration date as grouped values
    AddField(data->ExpirationMonthAsString(), &user_info);
    AddField(data->Expiration4DigitYearAsString(), &user_info);
  }

  if (data->HasNameOnCard()) {
    AddField(data->GetRawInfo(autofill::CREDIT_CARD_NAME_FULL), &user_info);
  }

  return user_info;
}

}  // namespace

CreditCardAccessoryControllerImpl::CreditCardAccessoryControllerImpl(
    autofill::PersonalDataManager* personal_data_manager,
    content::WebContents* web_contents)
    : personal_data_manager_(personal_data_manager),
      web_contents_(web_contents) {}

CreditCardAccessoryControllerImpl::~CreditCardAccessoryControllerImpl() =
    default;

void CreditCardAccessoryControllerImpl::RefreshSuggestionsForField() {
  const std::vector<CreditCard*> suggestions = GetSuggestions();
  std::vector<UserInfo> info_to_add;
  std::transform(suggestions.begin(), suggestions.end(),
                 std::back_inserter(info_to_add), &Translate);

  // TODO(crbug.com/902425): Add "Manage payment methods" footer command
  std::vector<FooterCommand> footer_commands;

  bool has_suggestions = !info_to_add.empty();
  GetManualFillingController()->RefreshSuggestionsForField(
      /*is_fillable=*/true,
      autofill::CreateAccessorySheetData(
          FallbackSheetType::CREDIT_CARD, GetTitle(has_suggestions),
          std::move(info_to_add), std::move(footer_commands)));
}

void CreditCardAccessoryControllerImpl::SetManualFillingControllerForTesting(
    base::WeakPtr<ManualFillingController> controller) {
  mf_controller_ = controller;
}

const std::vector<CreditCard*>
CreditCardAccessoryControllerImpl::GetSuggestions() {
  DCHECK(personal_data_manager_);
  return personal_data_manager_->GetCreditCardsToSuggest(
      /*include_server_cards=*/true);
}

base::WeakPtr<ManualFillingController>
CreditCardAccessoryControllerImpl::GetManualFillingController() {
  if (!mf_controller_)
    mf_controller_ = ManualFillingController::GetOrCreate(web_contents_);
  DCHECK(mf_controller_);
  return mf_controller_;
}

}  // namespace autofill
