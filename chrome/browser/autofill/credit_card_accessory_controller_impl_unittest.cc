// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/credit_card_accessory_controller_impl.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/autofill/mock_manual_filling_controller.h"
#include "chrome/test/base/testing_profile.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/test_personal_data_manager.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::SaveArg;

namespace autofill {

class CreditCardAccessoryControllerTest : public testing::Test {
 public:
  CreditCardAccessoryControllerTest()
      : controller_(&data_manager_, /*web_contents=*/nullptr) {}

  void SetUp() override {
    controller_.SetManualFillingControllerForTesting(
        mock_mf_controller_.AsWeakPtr());
    data_manager_.SetPrefService(profile_.GetPrefs());
  }

  void TearDown() override {
    data_manager_.SetPrefService(nullptr);
    data_manager_.ClearCreditCards();
  }

 protected:
  content::TestBrowserThreadBundle
      test_browser_thread_bundle_;  // for |profile_|
  CreditCardAccessoryControllerImpl controller_;
  testing::StrictMock<MockManualFillingController> mock_mf_controller_;
  autofill::TestPersonalDataManager data_manager_;
  TestingProfile profile_;
};

TEST_F(CreditCardAccessoryControllerTest, RefreshSuggestionsForField) {
  autofill::CreditCard card;
  card.SetNumber(base::ASCIIToUTF16("4111111111111111"));
  card.SetExpirationMonth(04);
  card.SetExpirationYear(39);
  card.SetRawInfo(autofill::CREDIT_CARD_NAME_FULL,
                  base::ASCIIToUTF16("Kirby Puckett"));

  data_manager_.AddCreditCard(card);

  autofill::AccessorySheetData result(autofill::FallbackSheetType::PASSWORD,
                                      base::string16());

  EXPECT_CALL(mock_mf_controller_, RefreshSuggestionsForField(
                                       /*is_fillable=*/true, _))
      .WillOnce(SaveArg<1>(&result));

  controller_.RefreshSuggestionsForField();

  ASSERT_EQ(1u, result.user_info_list().size());
  auto info = result.user_info_list().at(0);
  ASSERT_EQ(4u, info.fields().size());
  base::string16 expected_cc_string =
      autofill::internal::GetObfuscatedStringForCardDigits(
          base::ASCIIToUTF16("1111"));
  EXPECT_EQ(expected_cc_string, info.fields().at(0).display_text());
  EXPECT_EQ(base::ASCIIToUTF16("04"), info.fields().at(1).display_text());
  EXPECT_EQ(base::ASCIIToUTF16("2039"), info.fields().at(2).display_text());
  EXPECT_EQ(base::ASCIIToUTF16("Kirby Puckett"),
            info.fields().at(3).display_text());
}

}  // namespace autofill
