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
#include "components/autofill/content/browser/wallet/instrument.h"
#include "components/autofill/content/browser/wallet/required_action.h"
#include "components/autofill/content/browser/wallet/wallet_address.h"

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
  return scoped_ptr<WalletItems::MaskedInstrument>(
      new WalletItems::MaskedInstrument(ASCIIToUTF16("descriptive_name"),
                                        type,
                                        std::vector<base::string16>(),
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

scoped_ptr<Address> GetTestAddress() {
  return scoped_ptr<Address>(new Address("US",
                                         ASCIIToUTF16("recipient_name"),
                                         ASCIIToUTF16("address_line_1"),
                                         ASCIIToUTF16("address_line_2"),
                                         ASCIIToUTF16("locality_name"),
                                         ASCIIToUTF16("admin_area_name"),
                                         ASCIIToUTF16("postal_code_number"),
                                         ASCIIToUTF16("phone_number"),
                                         std::string()));
}

scoped_ptr<Address> GetTestMinimalAddress() {
  scoped_ptr<Address> address = GetTestAddress();
  address->set_is_complete_address(false);
  return address.Pass();
}

scoped_ptr<FullWallet> GetTestFullWallet() {
  scoped_ptr<FullWallet> wallet(new FullWallet(FutureYear(),
                                               12,
                                               "528512",
                                               "5ec4feecf9d6",
                                               GetTestAddress(),
                                               GetTestShippingAddress(),
                                               std::vector<RequiredAction>()));
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
  return scoped_ptr<Instrument>(new Instrument(ASCIIToUTF16("4444444444444448"),
                                               ASCIIToUTF16("123"),
                                               12,
                                               FutureYear(),
                                               Instrument::VISA,
                                               GetTestAddress()));
}

scoped_ptr<Instrument> GetTestAddressUpgradeInstrument() {
  scoped_ptr<Instrument> instrument(new Instrument(base::string16(),
                                                   base::string16(),
                                                   0,
                                                   0,
                                                   Instrument::UNKNOWN,
                                                   GetTestAddress()));
  instrument->set_object_id("instrument_id");
  return instrument.Pass();
}

scoped_ptr<Instrument> GetTestExpirationDateChangeInstrument() {
  scoped_ptr<Instrument> instrument(new Instrument(base::string16(),
                                                   ASCIIToUTF16("123"),
                                                   12,
                                                   FutureYear(),
                                                   Instrument::UNKNOWN,
                                                   scoped_ptr<Address>()));
  instrument->set_object_id("instrument_id");
  return instrument.Pass();
}

scoped_ptr<Instrument> GetTestAddressNameChangeInstrument() {
  scoped_ptr<Instrument> instrument(new Instrument(base::string16(),
                                                   ASCIIToUTF16("123"),
                                                   0,
                                                   0,
                                                   Instrument::UNKNOWN,
                                                   GetTestAddress()));
  instrument->set_object_id("instrument_id");
  return instrument.Pass();
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

scoped_ptr<WalletItems::MaskedInstrument> GetTestMaskedInstrumentAmex() {
  return GetTestMaskedInstrumentWithDetails(
      "default_instrument_id",
      GetTestAddress(),
      WalletItems::MaskedInstrument::AMEX,
      // Amex cards are marked with status AMEX_NOT_SUPPORTED by the server.
      WalletItems::MaskedInstrument::AMEX_NOT_SUPPORTED);
}

scoped_ptr<WalletItems::MaskedInstrument> GetTestNonDefaultMaskedInstrument() {
  return GetTestMaskedInstrumentWithId("instrument_id");
}

scoped_ptr<Address> GetTestSaveableAddress() {
  return scoped_ptr<Address>(new Address(
      "US",
      ASCIIToUTF16("save_recipient_name"),
      ASCIIToUTF16("save_address_line_1"),
      ASCIIToUTF16("save_address_line_2"),
      ASCIIToUTF16("save_locality_name"),
      ASCIIToUTF16("save_admin_area_name"),
      ASCIIToUTF16("save_postal_code_number"),
      ASCIIToUTF16("save_phone_number"),
      std::string()));
}

scoped_ptr<Address> GetTestShippingAddress() {
  return scoped_ptr<Address>(new Address(
      "US",
      ASCIIToUTF16("ship_recipient_name"),
      ASCIIToUTF16("ship_address_line_1"),
      ASCIIToUTF16("ship_address_line_2"),
      ASCIIToUTF16("ship_locality_name"),
      ASCIIToUTF16("ship_admin_area_name"),
      ASCIIToUTF16("ship_postal_code_number"),
      ASCIIToUTF16("ship_phone_number"),
      "default_address_id"));
}

scoped_ptr<Address> GetTestNonDefaultShippingAddress() {
  scoped_ptr<Address> address = GetTestShippingAddress();
  address->set_object_id("address_id");
  return address.Pass();
}

scoped_ptr<WalletItems> GetTestWalletItems() {
  return scoped_ptr<WalletItems>(
      new wallet::WalletItems(std::vector<RequiredAction>(),
                              "google_transaction_id",
                              "default_instrument_id",
                              "default_address_id",
                              "obfuscated_gaia_id"));
}

}  // namespace wallet
}  // namespace autofill
