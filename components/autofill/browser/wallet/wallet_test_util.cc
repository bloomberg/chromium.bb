// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/browser/wallet/wallet_test_util.h"

#include <string>
#include <vector>

#include "base/string16.h"
#include "base/utf_string_conversions.h"
#include "components/autofill/browser/wallet/instrument.h"
#include "components/autofill/browser/wallet/wallet_address.h"

namespace autofill {
namespace wallet {

scoped_ptr<Address> GetTestAddress() {
  return scoped_ptr<Address>(new Address("country_name_code",
                                         ASCIIToUTF16("recipient_name"),
                                         ASCIIToUTF16("address_line_1"),
                                         ASCIIToUTF16("address_line_2"),
                                         ASCIIToUTF16("locality_name"),
                                         ASCIIToUTF16("admin_area_name"),
                                         ASCIIToUTF16("postal_code_number"),
                                         ASCIIToUTF16("phone_number"),
                                         std::string()));
}

scoped_ptr<Instrument> GetTestInstrument() {
  return scoped_ptr<Instrument>(new Instrument(ASCIIToUTF16("4444444444444448"),
                                               ASCIIToUTF16("123"),
                                               12,
                                               2012,
                                               Instrument::VISA,
                                               GetTestAddress().Pass()));
}

scoped_ptr<WalletItems::MaskedInstrument> GetTestMaskedInstrument() {
  return scoped_ptr<WalletItems::MaskedInstrument>(
      new WalletItems::MaskedInstrument(ASCIIToUTF16("descriptive_name"),
                                        WalletItems::MaskedInstrument::UNKNOWN,
                                        std::vector<string16>(),
                                        ASCIIToUTF16("last_four_digits"),
                                        12,
                                        2012,
                                        GetTestAddress(),
                                        WalletItems::MaskedInstrument::EXPIRED,
                                        std::string()));
}

scoped_ptr<Address> GetTestShippingAddress() {
  return scoped_ptr<Address>(new Address(
      "ship_country_name_code",
      ASCIIToUTF16("ship_recipient_name"),
      ASCIIToUTF16("ship_address_line_1"),
      ASCIIToUTF16("ship_address_line_2"),
      ASCIIToUTF16("ship_locality_name"),
      ASCIIToUTF16("ship_admin_area_name"),
      ASCIIToUTF16("ship_postal_code_number"),
      ASCIIToUTF16("ship_phone_number"),
      std::string()));
}

}  // namespace wallet
}  // namespace autofill
