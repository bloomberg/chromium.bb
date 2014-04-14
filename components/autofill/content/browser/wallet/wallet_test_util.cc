// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/browser/wallet/wallet_test_util.h"

#include <string>
#include <vector>

#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "components/autofill/content/browser/wallet/full_wallet.h"
#include "components/autofill/content/browser/wallet/gaia_account.h"
#include "components/autofill/content/browser/wallet/instrument.h"
#include "components/autofill/content/browser/wallet/required_action.h"
#include "components/autofill/content/browser/wallet/wallet_address.h"

using base::ASCIIToUTF16;

namespace autofill {
namespace wallet {

namespace {

int FutureYear() {
  // "In the Year 3000." - Richie "LaBamba" Rosenberg
  return 3000;
}

}  // namespace

scoped_ptr<WalletItems::MaskedInstrument> GetTestMaskedInstrumentWithDetails(
    const std::string& id,
    scoped_ptr<Address> address,
    WalletItems::MaskedInstrument::Type type,
    WalletItems::MaskedInstrument::Status status) {
  return make_scoped_ptr(
      new WalletItems::MaskedInstrument(ASCIIToUTF16("descriptive_name"),
                                        type,
                                        ASCIIToUTF16("1111"),
                                        12,
                                        FutureYear(),
                                        address.Pass(),
                                        status,
                                        id));
}

scoped_ptr<WalletItems::MaskedInstrument> GetTestMaskedInstrumentWithId(
    const std::string& id) {
  return GetTestMaskedInstrumentWithDetails(
      id,
      GetTestAddress(),
      WalletItems::MaskedInstrument::VISA,
      WalletItems::MaskedInstrument::VALID);
}

scoped_ptr<WalletItems::MaskedInstrument>
GetTestMaskedInstrumentWithIdAndAddress(
    const std::string& id, scoped_ptr<Address> address) {
  return GetTestMaskedInstrumentWithDetails(
      id,
      address.Pass(),
      WalletItems::MaskedInstrument::VISA,
      WalletItems::MaskedInstrument::VALID);
}

scoped_ptr<GaiaAccount> GetTestGaiaAccount() {
  return GaiaAccount::CreateForTesting("user@chromium.org",
                                       "obfuscated_id",
                                       0,
                                       true);
}

std::vector<base::string16> StreetAddress(const std::string& line1,
                                          const std::string& line2) {
  std::vector<base::string16> street_address;
  street_address.push_back(ASCIIToUTF16(line1));
  street_address.push_back(ASCIIToUTF16(line2));
  return street_address;
}

scoped_ptr<Address> GetTestAddress() {
  return make_scoped_ptr(
      new Address("US",
                  ASCIIToUTF16("recipient_name"),
                  StreetAddress("address_line_1", "address_line_2"),
                  ASCIIToUTF16("locality_name"),
                  ASCIIToUTF16("dependent_locality_name"),
                  ASCIIToUTF16("admin_area_name"),
                  ASCIIToUTF16("postal_code_number"),
                  ASCIIToUTF16("sorting_code"),
                  ASCIIToUTF16("phone_number"),
                  std::string(),
                  "language_code"));
}

scoped_ptr<Address> GetTestMinimalAddress() {
  scoped_ptr<Address> address = GetTestAddress();
  address->set_is_complete_address(false);
  return address.Pass();
}

scoped_ptr<FullWallet> GetTestFullWallet() {
  return GetTestFullWalletWithRequiredActions(std::vector<RequiredAction>());
}

scoped_ptr<FullWallet> GetTestFullWalletWithRequiredActions(
    const std::vector<RequiredAction>& actions) {
  scoped_ptr<FullWallet> wallet(new FullWallet(FutureYear(),
                                               12,
                                               "528512",
                                               "5ec4feecf9d6",
                                               GetTestAddress(),
                                               GetTestShippingAddress(),
                                               actions));
  std::vector<uint8> one_time_pad;
  base::HexStringToBytes("5F04A8704183", &one_time_pad);
  wallet->set_one_time_pad(one_time_pad);
  return wallet.Pass();
}

scoped_ptr<FullWallet> GetTestFullWalletInstrumentOnly() {
  scoped_ptr<FullWallet> wallet(new FullWallet(FutureYear(),
                                               12,
                                               "528512",
                                               "5ec4feecf9d6",
                                               GetTestAddress(),
                                               scoped_ptr<Address>(),
                                               std::vector<RequiredAction>()));
  std::vector<uint8> one_time_pad;
  base::HexStringToBytes("5F04A8704183", &one_time_pad);
  wallet->set_one_time_pad(one_time_pad);
  return wallet.Pass();
}

scoped_ptr<Instrument> GetTestInstrument() {
  return make_scoped_ptr(new Instrument(ASCIIToUTF16("4444444444444448"),
                                        ASCIIToUTF16("123"),
                                        12,
                                        FutureYear(),
                                        Instrument::VISA,
                                        GetTestAddress()));
}

scoped_ptr<Instrument> GetTestAddressUpgradeInstrument() {
  return make_scoped_ptr(new Instrument(base::string16(),
                                        base::string16(),
                                        12,
                                        FutureYear(),
                                        Instrument::UNKNOWN,
                                        GetTestAddress()));
}

scoped_ptr<Instrument> GetTestExpirationDateChangeInstrument() {
  return make_scoped_ptr(new Instrument(base::string16(),
                                        ASCIIToUTF16("123"),
                                        12,
                                        FutureYear() + 1,
                                        Instrument::UNKNOWN,
                                        scoped_ptr<Address>()));
}

scoped_ptr<Instrument> GetTestAddressNameChangeInstrument() {
  return make_scoped_ptr(new Instrument(base::string16(),
                                        ASCIIToUTF16("123"),
                                        12,
                                        FutureYear(),
                                        Instrument::UNKNOWN,
                                        GetTestAddress()));
}

scoped_ptr<WalletItems::LegalDocument> GetTestLegalDocument() {
  base::DictionaryValue dict;
  dict.SetString("legal_document_id", "document_id");
  dict.SetString("display_name", "display_name");
  return wallet::WalletItems::LegalDocument::CreateLegalDocument(dict);
}

scoped_ptr<WalletItems::MaskedInstrument> GetTestMaskedInstrument() {
  return GetTestMaskedInstrumentWithId("default_instrument_id");
}

scoped_ptr<WalletItems::MaskedInstrument> GetTestMaskedInstrumentExpired() {
  return GetTestMaskedInstrumentWithDetails(
      "default_instrument_id",
      GetTestAddress(),
      WalletItems::MaskedInstrument::VISA,
      WalletItems::MaskedInstrument::EXPIRED);
}

scoped_ptr<WalletItems::MaskedInstrument> GetTestMaskedInstrumentInvalid() {
  return GetTestMaskedInstrumentWithDetails(
      "default_instrument_id",
      GetTestAddress(),
      WalletItems::MaskedInstrument::VISA,
      WalletItems::MaskedInstrument::DECLINED);
}

scoped_ptr<WalletItems::MaskedInstrument> GetTestMaskedInstrumentAmex(
    AmexPermission amex_permission) {
  return GetTestMaskedInstrumentWithDetails(
      "default_instrument_id",
      GetTestAddress(),
      WalletItems::MaskedInstrument::AMEX,
      amex_permission == AMEX_ALLOWED ?
          WalletItems::MaskedInstrument::VALID :
          WalletItems::MaskedInstrument::AMEX_NOT_SUPPORTED);
}

scoped_ptr<WalletItems::MaskedInstrument> GetTestNonDefaultMaskedInstrument() {
  return GetTestMaskedInstrumentWithId("instrument_id");
}

scoped_ptr<Address> GetTestSaveableAddress() {
  return make_scoped_ptr(
      new Address("US",
                  ASCIIToUTF16("save_recipient_name"),
                  StreetAddress("save_address_line_1", "save_address_line_2"),
                  ASCIIToUTF16("save_locality_name"),
                  ASCIIToUTF16("save_dependent_locality_name"),
                  ASCIIToUTF16("save_admin_area_name"),
                  ASCIIToUTF16("save_postal_code_number"),
                  ASCIIToUTF16("save_sorting_code"),
                  ASCIIToUTF16("save_phone_number"),
                  std::string(),
                  "save_language_code"));
}

scoped_ptr<Address> GetTestShippingAddress() {
  return make_scoped_ptr(
      new Address("US",
                  ASCIIToUTF16("ship_recipient_name"),
                  StreetAddress("ship_address_line_1", "ship_address_line_2"),
                  ASCIIToUTF16("ship_locality_name"),
                  ASCIIToUTF16("ship_dependent_locality_name"),
                  ASCIIToUTF16("ship_admin_area_name"),
                  ASCIIToUTF16("ship_postal_code_number"),
                  ASCIIToUTF16("ship_sorting_code"),
                  ASCIIToUTF16("ship_phone_number"),
                  "default_address_id",
                  "ship_language_code"));
}

scoped_ptr<Address> GetTestNonDefaultShippingAddress() {
  scoped_ptr<Address> address = GetTestShippingAddress();
  address->set_object_id("address_id");
  return address.Pass();
}

scoped_ptr<WalletItems> GetTestWalletItemsWithDetails(
    const std::vector<RequiredAction>& required_actions,
    const std::string& default_instrument_id,
    const std::string& default_address_id,
    AmexPermission amex_permission) {
  return make_scoped_ptr(new wallet::WalletItems(required_actions,
                                                 "google_transaction_id",
                                                 default_instrument_id,
                                                 default_address_id,
                                                 amex_permission));
}

scoped_ptr<WalletItems> GetTestWalletItemsWithRequiredAction(
    RequiredAction action) {
  std::vector<RequiredAction> required_actions(1, action);
  scoped_ptr<WalletItems> items =
      GetTestWalletItemsWithDetails(required_actions,
                                    "default_instrument_id",
                                    "default_address_id",
                                    AMEX_ALLOWED);

  if (action != GAIA_AUTH)
    items->AddAccount(GetTestGaiaAccount());

  return items.Pass();
}

scoped_ptr<WalletItems> GetTestWalletItems(AmexPermission amex_permission) {
  return GetTestWalletItemsWithDefaultIds("default_instrument_id",
                                          "default_address_id",
                                          amex_permission);
}

scoped_ptr<WalletItems> GetTestWalletItemsWithUsers(
    const std::vector<std::string>& users, size_t active_index) {
  scoped_ptr<WalletItems> items =
      GetTestWalletItemsWithDetails(std::vector<RequiredAction>(),
                                    "default_instrument_id",
                                    "default_address_id",
                                    AMEX_ALLOWED);
  for (size_t i = 0; i < users.size(); ++i) {
    scoped_ptr<GaiaAccount> account(GaiaAccount::CreateForTesting(
        users[i], "obfuscated_id", i, i == active_index));
    items->AddAccount(account.Pass());
  }
  return items.Pass();
}

scoped_ptr<WalletItems> GetTestWalletItemsWithDefaultIds(
    const std::string& default_instrument_id,
    const std::string& default_address_id,
    AmexPermission amex_permission) {
  scoped_ptr<WalletItems> items =
      GetTestWalletItemsWithDetails(std::vector<RequiredAction>(),
                                    default_instrument_id,
                                    default_address_id,
                                    amex_permission);
  items->AddAccount(GetTestGaiaAccount());
  return items.Pass();
}

}  // namespace wallet
}  // namespace autofill
