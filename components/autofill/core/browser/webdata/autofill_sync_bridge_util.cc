// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/webdata/autofill_sync_bridge_util.h"

#include "base/base64.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_profile.h"
#include "components/autofill/core/browser/credit_card.h"
#include "components/autofill/core/browser/webdata/autofill_table.h"
#include "components/sync/model/entity_data.h"

using sync_pb::AutofillWalletSpecifics;
using syncer::EntityData;

namespace autofill {
namespace {
std::string TruncateUTF8(const std::string& data) {
  std::string trimmed_value;
  base::TruncateUTF8ToByteSize(data, AutofillTable::kMaxDataLength,
                               &trimmed_value);
  return trimmed_value;
}

sync_pb::WalletMaskedCreditCard::WalletCardStatus LocalToServerStatus(
    const CreditCard& card) {
  switch (card.GetServerStatus()) {
    case CreditCard::OK:
      return sync_pb::WalletMaskedCreditCard::VALID;
    case CreditCard::EXPIRED:
    default:
      return sync_pb::WalletMaskedCreditCard::EXPIRED;
  }
}

sync_pb::WalletMaskedCreditCard::WalletCardType WalletCardTypeFromCardNetwork(
    const std::string& network) {
  if (network == kAmericanExpressCard)
    return sync_pb::WalletMaskedCreditCard::AMEX;
  if (network == kDiscoverCard)
    return sync_pb::WalletMaskedCreditCard::DISCOVER;
  if (network == kJCBCard)
    return sync_pb::WalletMaskedCreditCard::JCB;
  if (network == kMasterCard)
    return sync_pb::WalletMaskedCreditCard::MASTER_CARD;
  if (network == kUnionPay)
    return sync_pb::WalletMaskedCreditCard::UNIONPAY;
  if (network == kVisaCard)
    return sync_pb::WalletMaskedCreditCard::VISA;

  // Some cards aren't supported by the client, so just return unknown.
  return sync_pb::WalletMaskedCreditCard::UNKNOWN;
}

sync_pb::WalletMaskedCreditCard::WalletCardClass WalletCardClassFromCardType(
    CreditCard::CardType card_type) {
  switch (card_type) {
    case CreditCard::CARD_TYPE_CREDIT:
      return sync_pb::WalletMaskedCreditCard::CREDIT;
    case CreditCard::CARD_TYPE_DEBIT:
      return sync_pb::WalletMaskedCreditCard::DEBIT;
    case CreditCard::CARD_TYPE_PREPAID:
      return sync_pb::WalletMaskedCreditCard::PREPAID;
    default:
      return sync_pb::WalletMaskedCreditCard::UNKNOWN_CARD_CLASS;
  }
}

}  // namespace

std::string GetSpecificsIdForEntryServerId(const std::string& server_id) {
  std::string specifics_id;
  base::Base64Encode(server_id, &specifics_id);
  return specifics_id;
}

std::string GetStorageKeyForSpecificsId(const std::string& specifics_id) {
  // We use the base64 encoded |specifics_id| directly as the storage key, this
  // function only hides this definition from all its call sites.
  return specifics_id;
}

std::string GetStorageKeyForEntryServerId(const std::string& server_id) {
  return GetStorageKeyForSpecificsId(GetSpecificsIdForEntryServerId(server_id));
}

std::string GetClientTagForSpecificsId(
    AutofillWalletSpecifics::WalletInfoType type,
    const std::string& wallet_data_specifics_id) {
  switch (type) {
    case AutofillWalletSpecifics::POSTAL_ADDRESS:
      return "address-" + wallet_data_specifics_id;
    case AutofillWalletSpecifics::MASKED_CREDIT_CARD:
      return "card-" + wallet_data_specifics_id;
    case sync_pb::AutofillWalletSpecifics::CUSTOMER_DATA:
      return "customer-" + wallet_data_specifics_id;
    case AutofillWalletSpecifics::UNKNOWN:
      NOTREACHED();
      return "";
  }
}

void SetAutofillWalletSpecificsFromServerProfile(
    const AutofillProfile& address,
    AutofillWalletSpecifics* wallet_specifics) {
  wallet_specifics->set_type(AutofillWalletSpecifics::POSTAL_ADDRESS);

  sync_pb::WalletPostalAddress* wallet_address =
      wallet_specifics->mutable_address();

  wallet_address->set_id(address.server_id());
  wallet_address->set_language_code(TruncateUTF8(address.language_code()));

  if (address.HasRawInfo(NAME_FULL)) {
    wallet_address->set_recipient_name(
        TruncateUTF8(base::UTF16ToUTF8(address.GetRawInfo(NAME_FULL))));
  }
  if (address.HasRawInfo(COMPANY_NAME)) {
    wallet_address->set_company_name(
        TruncateUTF8(base::UTF16ToUTF8(address.GetRawInfo(COMPANY_NAME))));
  }
  if (address.HasRawInfo(ADDRESS_HOME_STREET_ADDRESS)) {
    wallet_address->add_street_address(TruncateUTF8(
        base::UTF16ToUTF8(address.GetRawInfo(ADDRESS_HOME_STREET_ADDRESS))));
  }
  if (address.HasRawInfo(ADDRESS_HOME_STATE)) {
    wallet_address->set_address_1(TruncateUTF8(
        base::UTF16ToUTF8(address.GetRawInfo(ADDRESS_HOME_STATE))));
  }
  if (address.HasRawInfo(ADDRESS_HOME_CITY)) {
    wallet_address->set_address_2(
        TruncateUTF8(base::UTF16ToUTF8(address.GetRawInfo(ADDRESS_HOME_CITY))));
  }
  if (address.HasRawInfo(ADDRESS_HOME_DEPENDENT_LOCALITY)) {
    wallet_address->set_address_3(TruncateUTF8(base::UTF16ToUTF8(
        address.GetRawInfo(ADDRESS_HOME_DEPENDENT_LOCALITY))));
  }
  if (address.HasRawInfo(ADDRESS_HOME_ZIP)) {
    wallet_address->set_postal_code(
        TruncateUTF8(base::UTF16ToUTF8(address.GetRawInfo(ADDRESS_HOME_ZIP))));
  }
  if (address.HasRawInfo(ADDRESS_HOME_COUNTRY)) {
    wallet_address->set_country_code(TruncateUTF8(
        base::UTF16ToUTF8(address.GetRawInfo(ADDRESS_HOME_COUNTRY))));
  }
  if (address.HasRawInfo(PHONE_HOME_WHOLE_NUMBER)) {
    wallet_address->set_phone_number(TruncateUTF8(
        base::UTF16ToUTF8(address.GetRawInfo(PHONE_HOME_WHOLE_NUMBER))));
  }
  if (address.HasRawInfo(ADDRESS_HOME_SORTING_CODE)) {
    wallet_address->set_sorting_code(TruncateUTF8(
        base::UTF16ToUTF8(address.GetRawInfo(ADDRESS_HOME_SORTING_CODE))));
  }
}

std::unique_ptr<EntityData> CreateEntityDataFromAutofillServerProfile(
    const AutofillProfile& address) {
  auto entity_data = std::make_unique<EntityData>();

  std::string specifics_id =
      GetSpecificsIdForEntryServerId(address.server_id());
  entity_data->non_unique_name = GetClientTagForSpecificsId(
      AutofillWalletSpecifics::POSTAL_ADDRESS, specifics_id);

  AutofillWalletSpecifics* wallet_specifics =
      entity_data->specifics.mutable_autofill_wallet();

  SetAutofillWalletSpecificsFromServerProfile(address, wallet_specifics);

  return entity_data;
}

void SetAutofillWalletSpecificsFromServerCard(
    const CreditCard& card,
    AutofillWalletSpecifics* wallet_specifics) {
  wallet_specifics->set_type(AutofillWalletSpecifics::MASKED_CREDIT_CARD);

  sync_pb::WalletMaskedCreditCard* wallet_card =
      wallet_specifics->mutable_masked_card();
  wallet_card->set_id(card.server_id());
  wallet_card->set_status(LocalToServerStatus(card));
  if (card.HasRawInfo(CREDIT_CARD_NAME_FULL)) {
    wallet_card->set_name_on_card(TruncateUTF8(
        base::UTF16ToUTF8(card.GetRawInfo(CREDIT_CARD_NAME_FULL))));
  }
  wallet_card->set_type(WalletCardTypeFromCardNetwork(card.network()));
  wallet_card->set_last_four(base::UTF16ToUTF8(card.LastFourDigits()));
  wallet_card->set_exp_month(card.expiration_month());
  wallet_card->set_exp_year(card.expiration_year());
  wallet_card->set_billing_address_id(card.billing_address_id());
  wallet_card->set_card_class(WalletCardClassFromCardType(card.card_type()));
  wallet_card->set_bank_name(card.bank_name());
}

std::unique_ptr<EntityData> CreateEntityDataFromCard(const CreditCard& card) {
  std::string specifics_id = GetSpecificsIdForEntryServerId(card.server_id());

  auto entity_data = std::make_unique<EntityData>();
  entity_data->non_unique_name = GetClientTagForSpecificsId(
      AutofillWalletSpecifics::MASKED_CREDIT_CARD, specifics_id);

  AutofillWalletSpecifics* wallet_specifics =
      entity_data->specifics.mutable_autofill_wallet();

  SetAutofillWalletSpecificsFromServerCard(card, wallet_specifics);

  return entity_data;
}

}  // namespace autofill