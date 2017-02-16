// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/payments/credit_card_editor_view_controller.h"

#include <memory>

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/views/payments/editor_view_controller.h"
#include "components/autofill/core/browser/test_autofill_clock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/controls/combobox/combobox.h"

namespace payments {

namespace {

const base::Time kJanuary2017 = base::Time::FromDoubleT(1484505871);
const base::Time kJune2017 = base::Time::FromDoubleT(1497552271);

}  // namespace

// Test that if we are in January, the returned months are as expected, and the
// default selected month is January.
TEST(CreditCardEditorViewControllerTest, ExpirationMonth_FromJanuary) {
  autofill::TestAutofillClock test_clock;
  test_clock.SetNow(kJanuary2017);

  std::unique_ptr<CreditCardEditorViewController> view_controller(
      new CreditCardEditorViewController(nullptr, nullptr));

  std::unique_ptr<ui::ComboboxModel> model =
      view_controller->GetComboboxModelForType(autofill::CREDIT_CARD_EXP_MONTH);
  EXPECT_EQ(12, model->GetItemCount());
  EXPECT_EQ(base::ASCIIToUTF16("01"), model->GetItemAt(0));
  EXPECT_EQ(base::ASCIIToUTF16("02"), model->GetItemAt(1));
  EXPECT_EQ(base::ASCIIToUTF16("03"), model->GetItemAt(2));
  EXPECT_EQ(base::ASCIIToUTF16("04"), model->GetItemAt(3));
  EXPECT_EQ(base::ASCIIToUTF16("05"), model->GetItemAt(4));
  EXPECT_EQ(base::ASCIIToUTF16("06"), model->GetItemAt(5));
  EXPECT_EQ(base::ASCIIToUTF16("07"), model->GetItemAt(6));
  EXPECT_EQ(base::ASCIIToUTF16("08"), model->GetItemAt(7));
  EXPECT_EQ(base::ASCIIToUTF16("09"), model->GetItemAt(8));
  EXPECT_EQ(base::ASCIIToUTF16("10"), model->GetItemAt(9));
  EXPECT_EQ(base::ASCIIToUTF16("11"), model->GetItemAt(10));
  EXPECT_EQ(base::ASCIIToUTF16("12"), model->GetItemAt(11));

  // Selected item is the current month.
  EXPECT_EQ(0, model->GetDefaultIndex());
}

// Test that if we are in January, the returned months are as expected, and the
// default selected month is June.
TEST(CreditCardEditorViewControllerTest, ExpirationMonth_FromJune) {
  autofill::TestAutofillClock test_clock;
  test_clock.SetNow(kJune2017);

  std::unique_ptr<CreditCardEditorViewController> view_controller(
      new CreditCardEditorViewController(nullptr, nullptr));

  std::unique_ptr<ui::ComboboxModel> model =
      view_controller->GetComboboxModelForType(autofill::CREDIT_CARD_EXP_MONTH);
  EXPECT_EQ(12, model->GetItemCount());
  EXPECT_EQ(base::ASCIIToUTF16("01"), model->GetItemAt(0));
  EXPECT_EQ(base::ASCIIToUTF16("02"), model->GetItemAt(1));
  EXPECT_EQ(base::ASCIIToUTF16("03"), model->GetItemAt(2));
  EXPECT_EQ(base::ASCIIToUTF16("04"), model->GetItemAt(3));
  EXPECT_EQ(base::ASCIIToUTF16("05"), model->GetItemAt(4));
  EXPECT_EQ(base::ASCIIToUTF16("06"), model->GetItemAt(5));
  EXPECT_EQ(base::ASCIIToUTF16("07"), model->GetItemAt(6));
  EXPECT_EQ(base::ASCIIToUTF16("08"), model->GetItemAt(7));
  EXPECT_EQ(base::ASCIIToUTF16("09"), model->GetItemAt(8));
  EXPECT_EQ(base::ASCIIToUTF16("10"), model->GetItemAt(9));
  EXPECT_EQ(base::ASCIIToUTF16("11"), model->GetItemAt(10));
  EXPECT_EQ(base::ASCIIToUTF16("12"), model->GetItemAt(11));

  EXPECT_EQ(5, model->GetDefaultIndex());
}

// Tests that we show the correct amount of years in the year dropdown, starting
// with the current one.
TEST(CreditCardEditorViewControllerTest, ExpirationYear_From2017) {
  autofill::TestAutofillClock test_clock;
  test_clock.SetNow(kJune2017);

  std::unique_ptr<CreditCardEditorViewController> view_controller(
      new CreditCardEditorViewController(nullptr, nullptr));

  std::unique_ptr<ui::ComboboxModel> model =
      view_controller->GetComboboxModelForType(
          autofill::CREDIT_CARD_EXP_4_DIGIT_YEAR);
  EXPECT_EQ(10, model->GetItemCount());
  EXPECT_EQ(base::ASCIIToUTF16("2017"), model->GetItemAt(0));
  EXPECT_EQ(base::ASCIIToUTF16("2018"), model->GetItemAt(1));
  EXPECT_EQ(base::ASCIIToUTF16("2019"), model->GetItemAt(2));
  EXPECT_EQ(base::ASCIIToUTF16("2020"), model->GetItemAt(3));
  EXPECT_EQ(base::ASCIIToUTF16("2021"), model->GetItemAt(4));
  EXPECT_EQ(base::ASCIIToUTF16("2022"), model->GetItemAt(5));
  EXPECT_EQ(base::ASCIIToUTF16("2023"), model->GetItemAt(6));
  EXPECT_EQ(base::ASCIIToUTF16("2024"), model->GetItemAt(7));
  EXPECT_EQ(base::ASCIIToUTF16("2025"), model->GetItemAt(8));
  EXPECT_EQ(base::ASCIIToUTF16("2026"), model->GetItemAt(9));
}

}  // namespace payments
