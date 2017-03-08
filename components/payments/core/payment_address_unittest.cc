// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/core/payment_address.h"

#include <vector>

#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace payments {

// Tests that two addresses are not equal if their property values differ or
// one is missing a value present in the other, and equal otherwise.
TEST(PaymentRequestTest, PaymentAddressEquality) {
  PaymentAddress address1;
  PaymentAddress address2;
  EXPECT_EQ(address1, address2);

  address1.country = base::ASCIIToUTF16("Madagascar");
  EXPECT_NE(address1, address2);
  address2.country = base::ASCIIToUTF16("Monaco");
  EXPECT_NE(address1, address2);
  address2.country = base::ASCIIToUTF16("Madagascar");
  EXPECT_EQ(address1, address2);

  std::vector<base::string16> address_line1;
  address_line1.push_back(base::ASCIIToUTF16("123 Main St."));
  address_line1.push_back(base::ASCIIToUTF16("Apartment B"));
  address1.address_line = address_line1;
  EXPECT_NE(address1, address2);
  std::vector<base::string16> address_line2;
  address_line2.push_back(base::ASCIIToUTF16("123 Main St."));
  address_line2.push_back(base::ASCIIToUTF16("Apartment C"));
  address2.address_line = address_line2;
  EXPECT_NE(address1, address2);
  address2.address_line = address_line1;
  EXPECT_EQ(address1, address2);

  address1.region = base::ASCIIToUTF16("Quebec");
  EXPECT_NE(address1, address2);
  address2.region = base::ASCIIToUTF16("Newfoundland and Labrador");
  EXPECT_NE(address1, address2);
  address2.region = base::ASCIIToUTF16("Quebec");
  EXPECT_EQ(address1, address2);

  address1.city = base::ASCIIToUTF16("Timbuktu");
  EXPECT_NE(address1, address2);
  address2.city = base::ASCIIToUTF16("Timbuk 3");
  EXPECT_NE(address1, address2);
  address2.city = base::ASCIIToUTF16("Timbuktu");
  EXPECT_EQ(address1, address2);

  address1.dependent_locality = base::ASCIIToUTF16("Manhattan");
  EXPECT_NE(address1, address2);
  address2.dependent_locality = base::ASCIIToUTF16("Queens");
  EXPECT_NE(address1, address2);
  address2.dependent_locality = base::ASCIIToUTF16("Manhattan");
  EXPECT_EQ(address1, address2);

  address1.postal_code = base::ASCIIToUTF16("90210");
  EXPECT_NE(address1, address2);
  address2.postal_code = base::ASCIIToUTF16("89049");
  EXPECT_NE(address1, address2);
  address2.postal_code = base::ASCIIToUTF16("90210");
  EXPECT_EQ(address1, address2);

  address1.sorting_code = base::ASCIIToUTF16("14390");
  EXPECT_NE(address1, address2);
  address2.sorting_code = base::ASCIIToUTF16("09341");
  EXPECT_NE(address1, address2);
  address2.sorting_code = base::ASCIIToUTF16("14390");
  EXPECT_EQ(address1, address2);

  address1.language_code = base::ASCIIToUTF16("fr");
  EXPECT_NE(address1, address2);
  address2.language_code = base::ASCIIToUTF16("zh-HK");
  EXPECT_NE(address1, address2);
  address2.language_code = base::ASCIIToUTF16("fr");
  EXPECT_EQ(address1, address2);

  address1.organization = base::ASCIIToUTF16("The Willy Wonka Candy Company");
  EXPECT_NE(address1, address2);
  address2.organization = base::ASCIIToUTF16("Sears");
  EXPECT_NE(address1, address2);
  address2.organization = base::ASCIIToUTF16("The Willy Wonka Candy Company");
  EXPECT_EQ(address1, address2);

  address1.recipient = base::ASCIIToUTF16("Veruca Salt");
  EXPECT_NE(address1, address2);
  address2.recipient = base::ASCIIToUTF16("Veronica Mars");
  EXPECT_NE(address1, address2);
  address2.recipient = base::ASCIIToUTF16("Veruca Salt");
  EXPECT_EQ(address1, address2);

  address1.phone = base::ASCIIToUTF16("888-867-5309");
  EXPECT_NE(address1, address2);
  address2.phone = base::ASCIIToUTF16("800-984-3672");
  EXPECT_NE(address1, address2);
  address2.phone = base::ASCIIToUTF16("888-867-5309");
  EXPECT_EQ(address1, address2);
}

}  // namespace payments
