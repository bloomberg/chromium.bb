// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/country_combobox_model.h"

#include "components/autofill/core/browser/test_personal_data_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

namespace {
const char kTestCountry[] = "AQ";
}

TEST(CountryComboboxModel, RespectsManagerDefaultCountry) {
  TestPersonalDataManager manager;
  manager.set_timezone_country_code(kTestCountry);

  CountryComboboxModel model(manager);
  EXPECT_EQ(kTestCountry, model.GetDefaultCountryCode());
}

}  // namespace autofill
