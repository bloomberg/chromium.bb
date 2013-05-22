// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/browser/wallet/wallet_test_util.h"

#include <string>
#include <vector>

#include "base/string16.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "components/autofill/browser/wallet/full_wallet.h"
#include "components/autofill/browser/wallet/instrument.h"
#include "components/autofill/browser/wallet/required_action.h"
#include "components/autofill/browser/wallet/wallet_address.h"

namespace autofill {
namespace wallet {

namespace {

int FutureYear() {
  // "In the Year 3000." - Richie "LaBamba" Rosenberg
  return 3000;
}

}  // namespace

scoped_ptr<WalletItems::MaskedInstrument> GetTestMaskedInstrumentWithId(
    const std::string& id) {
  return scoped_ptr<WalletItems::MaskedInstrument>(
      new WalletItems::MaskedInstrument(ASCIIToUTF16("descriptive_name"),
                                        WalletItems::MaskedInstrument::VISA,
                                        std::vector<base::string16>(),
                                        ASCIIToUTF16("1111"),
                                        12,
                                        FutureYear(),
                                        GetTestAddress(),
                                        WalletItems::MaskedInstrument::VALID,
                                        id));
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

scoped_ptr<FullWallet> GetTestFullWallet() {
  base::Time::Exploded exploded;
  base::Time::Now().LocalExplode(&exploded);
  return scoped_ptr<FullWallet>(new FullWallet(FutureYear(),
                                               12,
                                               "iin",
                                               "rest",
                                               GetTestAddress(),
                                               GetTestShippingAddress(),
                                               std::vector<RequiredAction>()));
}

scoped_ptr<Instrument> GetTestInstrument() {
  return scoped_ptr<Instrument>(new Instrument(ASCIIToUTF16("4444444444444448"),
                                               ASCIIToUTF16("123"),
                                               12,
                                               FutureYear(),
                                               Instrument::VISA,
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
