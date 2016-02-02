// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/autofill_private/autofill_util.h"

#include <stddef.h>
#include <utility>

#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/api/settings_private/prefs_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/autofill_private.h"
#include "chrome/common/pref_names.h"
#include "components/autofill/core/browser/autofill_profile.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/credit_card.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/prefs/pref_service.h"
#include "grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill_private = extensions::api::autofill_private;

namespace {

// Get the multi-valued element for |type| and return it as a |vector|.
// TODO(khorimoto): remove this function since multi-valued types are
// deprecated.
scoped_ptr<std::vector<std::string>> GetValueList(
    const autofill::AutofillProfile& profile, autofill::ServerFieldType type) {
  scoped_ptr<std::vector<std::string>> list(new std::vector<std::string>);

  std::vector<base::string16> values;
  if (autofill::AutofillType(type).group() == autofill::NAME) {
    values.push_back(
        profile.GetInfo(autofill::AutofillType(type),
                        g_browser_process->GetApplicationLocale()));
  } else {
    values.push_back(profile.GetRawInfo(type));
  }

  // |Get[Raw]MultiInfo()| always returns at least one, potentially empty, item.
  // If this is the case, there is no info to return, so return an empty vector.
  if (values.size() == 1 && values.front().empty())
    return list;

  for (const base::string16& value16 : values)
    list->push_back(base::UTF16ToUTF8(value16));

  return list;
}

// Gets the string corresponding to |type| from |profile|.
scoped_ptr<std::string> GetStringFromProfile(
    const autofill::AutofillProfile& profile,
    const autofill::ServerFieldType& type) {
  return make_scoped_ptr(
      new std::string(base::UTF16ToUTF8(profile.GetRawInfo(type))));
}

scoped_ptr<autofill_private::AddressEntry> ProfileToAddressEntry(
    const autofill::AutofillProfile& profile,
    const base::string16& label) {
  scoped_ptr<autofill_private::AddressEntry>
      address(new autofill_private::AddressEntry);

  // Add all address fields to the entry.
  address->guid.reset(new std::string(profile.guid()));
  address->full_names = GetValueList(profile, autofill::NAME_FULL);
  address->company_name.reset(GetStringFromProfile(
      profile, autofill::COMPANY_NAME).release());
  address->address_lines.reset(GetStringFromProfile(
      profile, autofill::ADDRESS_HOME_STREET_ADDRESS).release());
  address->address_level1.reset(GetStringFromProfile(
      profile, autofill::ADDRESS_HOME_STATE).release());
  address->address_level2.reset(GetStringFromProfile(
      profile, autofill::ADDRESS_HOME_CITY).release());
  address->address_level3.reset(GetStringFromProfile(
      profile, autofill::ADDRESS_HOME_DEPENDENT_LOCALITY).release());
  address->postal_code.reset(GetStringFromProfile(
      profile, autofill::ADDRESS_HOME_ZIP).release());
  address->sorting_code.reset(GetStringFromProfile(
      profile, autofill::ADDRESS_HOME_SORTING_CODE).release());
  address->country_code.reset(GetStringFromProfile(
      profile, autofill::ADDRESS_HOME_COUNTRY).release());
  address->phone_numbers =
      GetValueList(profile, autofill::PHONE_HOME_WHOLE_NUMBER);
  address->email_addresses =
      GetValueList(profile, autofill::EMAIL_ADDRESS);
  address->language_code.reset(new std::string(profile.language_code()));

  // Parse |label| so that it can be used to create address metadata.
  base::string16 separator =
      l10n_util::GetStringUTF16(IDS_AUTOFILL_ADDRESS_SUMMARY_SEPARATOR);
  std::vector<base::string16> label_pieces;
  base::SplitStringUsingSubstr(label, separator, &label_pieces);

  // Create address metadata and add it to |address|.
  scoped_ptr<autofill_private::AutofillMetadata>
      metadata(new autofill_private::AutofillMetadata);
  metadata->summary_label = base::UTF16ToUTF8(label_pieces[0]);
  metadata->summary_sublabel.reset(new std::string(base::UTF16ToUTF8(
      label.substr(label_pieces[0].size()))));
  metadata->is_local.reset(new bool(
      profile.record_type() == autofill::AutofillProfile::LOCAL_PROFILE));
  address->metadata = std::move(metadata);

  return address;
}

scoped_ptr<autofill_private::CreditCardEntry> CreditCardToCreditCardEntry(
    const autofill::CreditCard& credit_card) {
  scoped_ptr<autofill_private::CreditCardEntry>
      card(new autofill_private::CreditCardEntry);

  // Add all credit card fields to the entry.
  card->guid.reset(new std::string(credit_card.guid()));
  card->name.reset(new std::string(base::UTF16ToUTF8(
      credit_card.GetRawInfo(autofill::CREDIT_CARD_NAME))));
  card->card_number.reset(new std::string(base::UTF16ToUTF8(
      credit_card.GetRawInfo(autofill::CREDIT_CARD_NUMBER))));
  card->expiration_month.reset(new std::string(base::UTF16ToUTF8(
      credit_card.GetRawInfo(autofill::CREDIT_CARD_EXP_MONTH))));
  card->expiration_year.reset(new std::string(base::UTF16ToUTF8(
      credit_card.GetRawInfo(autofill::CREDIT_CARD_EXP_4_DIGIT_YEAR))));

  // Create address metadata and add it to |address|.
  scoped_ptr<autofill_private::AutofillMetadata>
      metadata(new autofill_private::AutofillMetadata);
  std::pair<base::string16, base::string16> label_pieces =
      credit_card.LabelPieces();
  metadata->summary_label = base::UTF16ToUTF8(label_pieces.first);
  metadata->summary_sublabel.reset(new std::string(base::UTF16ToUTF8(
      label_pieces.second)));
  metadata->is_local.reset(new bool(
      credit_card.record_type() == autofill::CreditCard::LOCAL_CARD));
  metadata->is_cached.reset(new bool(
      credit_card.record_type() == autofill::CreditCard::FULL_SERVER_CARD));
  card->metadata = std::move(metadata);

  return card;
}

}  // namespace

namespace extensions {

namespace autofill_util {

scoped_ptr<AddressEntryList> GenerateAddressList(
    const autofill::PersonalDataManager& personal_data) {
  const std::vector<autofill::AutofillProfile*>& profiles =
      personal_data.GetProfiles();
  std::vector<base::string16> labels;
  autofill::AutofillProfile::CreateDifferentiatingLabels(
      profiles,
      g_browser_process->GetApplicationLocale(),
      &labels);
  DCHECK_EQ(labels.size(), profiles.size());

  scoped_ptr<AddressEntryList> list(new AddressEntryList);
  for (size_t i = 0; i < profiles.size(); ++i) {
    autofill_private::AddressEntry* entry =
        ProfileToAddressEntry(*profiles[i], labels[i]).release();
    list->push_back(linked_ptr<autofill_private::AddressEntry>(entry));
  }

  return list;
}

scoped_ptr<CreditCardEntryList> GenerateCreditCardList(
    const autofill::PersonalDataManager& personal_data) {
  const std::vector<autofill::CreditCard*>& cards =
      personal_data.GetCreditCards();

  scoped_ptr<CreditCardEntryList> list(new CreditCardEntryList);
  for (const autofill::CreditCard* card : cards) {
    autofill_private::CreditCardEntry* entry =
        CreditCardToCreditCardEntry(*card).release();
    list->push_back(linked_ptr<autofill_private::CreditCardEntry>(entry));
  }

  return list;
}

}  // namespace autofill_util

}  // namespace extensions
