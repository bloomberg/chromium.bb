// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/wallet/wallet_test_util.h"

#include "chrome/browser/autofill/wallet/instrument.h"
#include "chrome/browser/autofill/wallet/wallet_address.h"

namespace wallet {

scoped_ptr<Instrument> GetTestInstrument() {
  scoped_ptr<Address> address(new Address("country_name_code",
                                          "recipient_name",
                                          "address_line_1",
                                          "address_line_2",
                                          "locality_name",
                                          "admin_area_name",
                                          "postal_code_number",
                                          "phone_number",
                                          std::string()));

  return scoped_ptr<Instrument>(new Instrument("4444444444444448",
                                               "123",
                                               12,
                                               2012,
                                               Instrument::VISA,
                                               address.Pass()));
}

scoped_ptr<Address> GetTestShippingAddress() {
  return scoped_ptr<Address>(new Address("ship_country_name_code",
                                         "ship_recipient_name",
                                         "ship_address_line_1",
                                         "ship_address_line_2",
                                         "ship_locality_name",
                                         "ship_admin_area_name",
                                         "ship_postal_code_number",
                                         "ship_phone_number",
                                         std::string()));

}

}  // namespace wallet
