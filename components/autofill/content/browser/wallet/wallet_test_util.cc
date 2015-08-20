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
  scoped_ptr<FullWallet> wallet(new FullWallet(FutureYear(), 12, "528512",
                                               "5ec4feecf9d6", GetTestAddress(),
                                               GetTestShippingAddress()));
  std::vector<uint8> one_time_pad;
  base::HexStringToBytes("5F04A8704183", &one_time_pad);
  wallet->set_one_time_pad(one_time_pad);
  return wallet.Pass();
}

scoped_ptr<FullWallet> GetTestFullWalletInstrumentOnly() {
  scoped_ptr<FullWallet> wallet(new FullWallet(FutureYear(), 12, "528512",
                                               "5ec4feecf9d6", GetTestAddress(),
                                               scoped_ptr<Address>()));
  std::vector<uint8> one_time_pad;
  base::HexStringToBytes("5F04A8704183", &one_time_pad);
  wallet->set_one_time_pad(one_time_pad);
  return wallet.Pass();
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

}  // namespace wallet
}  // namespace autofill
