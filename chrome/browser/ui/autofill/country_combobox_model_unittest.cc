// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/country_combobox_model.h"

#include "components/autofill/core/browser/autofill_country.h"
#include "components/autofill/core/browser/test_personal_data_manager.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gmock_mutant.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/models/combobox_model_observer.h"

namespace autofill {

namespace {

const char kTestCountry[] = "AQ";

class MockObserver : public ui::ComboboxModelObserver {
 public:
  MOCK_METHOD1(OnComboboxModelChanged, void(ui::ComboboxModel* model));
};

}  // namespace

TEST(CountryComboboxModel, SetAndResetDefaultCountry) {
  TestPersonalDataManager manager;
  CountryComboboxModel model(manager);

  MockObserver observer;
  model.AddObserver(&observer);
  EXPECT_CALL(observer, OnComboboxModelChanged(&model)).Times(2);

  std::string default_country = model.GetDefaultCountryCode();
  ASSERT_NE(default_country, kTestCountry);

  model.SetDefaultCountry(kTestCountry);
  EXPECT_EQ(kTestCountry, model.GetDefaultCountryCode());

  model.ResetDefault();
  EXPECT_EQ(default_country, model.GetDefaultCountryCode());
}

TEST(CountryComboboxModel, RespectsManagerDefaultCountry) {
  TestPersonalDataManager manager;
  manager.set_timezone_country_code(kTestCountry);

  CountryComboboxModel model(manager);
  EXPECT_EQ(kTestCountry, model.GetDefaultCountryCode());
}

}  // namespace autofill
