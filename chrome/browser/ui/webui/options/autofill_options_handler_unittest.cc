// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options/autofill_options_handler.h"

#include "base/values.h"
#include "components/autofill/core/browser/autofill_profile.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace options {

TEST(AutofillOptionsHandlerTest, AddressToDictionary) {
  autofill::AutofillProfile profile;
  autofill::test::SetProfileInfoWithGuid(&profile,
                                         "guid",
                                         "First",
                                         "Middle",
                                         "Last",
                                         "fml@example.com",
                                         "Acme inc",
                                         "123 Main",
                                         "Apt 2",
                                         "Laredo",
                                         "TX",
                                         "77300",
                                         "US",
                                         "832-555-1000");

  base::DictionaryValue dictionary;
  options::AutofillOptionsHandler::AutofillProfileToDictionary(profile,
                                                               &dictionary);

  std::string value;
  EXPECT_TRUE(dictionary.GetString("addrLines", &value));
  EXPECT_EQ("123 Main\nApt 2", value);
  EXPECT_TRUE(dictionary.GetString("city", &value));
  EXPECT_EQ("Laredo", value);
  EXPECT_TRUE(dictionary.GetString("country", &value));
  EXPECT_EQ("US", value);
  EXPECT_TRUE(dictionary.GetString("dependentLocality", &value));
  EXPECT_EQ("", value);
  EXPECT_TRUE(dictionary.GetString("guid", &value));
  EXPECT_EQ("guid", value);
  EXPECT_TRUE(dictionary.GetString("languageCode", &value));
  EXPECT_EQ("", value);
  EXPECT_TRUE(dictionary.GetString("postalCode", &value));
  EXPECT_EQ("77300", value);
  EXPECT_TRUE(dictionary.GetString("sortingCode", &value));
  EXPECT_EQ("", value);
  EXPECT_TRUE(dictionary.GetString("state", &value));
  EXPECT_EQ("TX", value);

  base::ListValue* list_value;
  EXPECT_TRUE(dictionary.GetList("email", &list_value));
  EXPECT_EQ(1U, list_value->GetSize());
  EXPECT_TRUE(list_value->GetString(0, &value));
  EXPECT_EQ("fml@example.com", value);

  EXPECT_TRUE(dictionary.GetList("fullName", &list_value));
  EXPECT_EQ(1U, list_value->GetSize());
  EXPECT_TRUE(list_value->GetString(0, &value));
  EXPECT_EQ("First Middle Last", value);

  EXPECT_TRUE(dictionary.GetList("phone", &list_value));
  EXPECT_EQ(1U, list_value->GetSize());
  EXPECT_TRUE(list_value->GetString(0, &value));
  EXPECT_EQ("832-555-1000", value);
}

}  // namespace options
