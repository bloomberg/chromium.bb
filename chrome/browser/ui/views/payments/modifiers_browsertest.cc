// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/views/payments/payment_request_browsertest_base.h"
#include "chrome/browser/ui/views/payments/payment_request_dialog_view_ids.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/payments/core/features.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"

namespace payments {

class PaymentRequestModifiersTest : public PaymentRequestBrowserTestBase {
 protected:
  PaymentRequestModifiersTest()
      : PaymentRequestBrowserTestBase(
            "/payment_request_bobpay_and_basic_card_with_basic_card_modifiers_"
            "test.html") {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    PaymentRequestBrowserTestBase::SetUpCommandLine(command_line);
    command_line->AppendSwitch(
        switches::kEnableExperimentalWebPlatformFeatures);
  }

  void SetUpInProcessBrowserTestFixture() override {
    PaymentRequestBrowserTestBase::SetUpInProcessBrowserTestFixture();

    // Enable browser-side modifiers support.
    feature_list_.InitAndEnableFeature(features::kWebPaymentsModifiers);
  }

 private:
  base::test::ScopedFeatureList feature_list_;

  DISALLOW_COPY_AND_ASSIGN(PaymentRequestModifiersTest);
};

IN_PROC_BROWSER_TEST_F(PaymentRequestModifiersTest,
                       NoModifierAppliedIfNoSelectedInstrument) {
  InvokePaymentRequestUI();
  OpenOrderSummaryScreen();

  EXPECT_EQ(base::ASCIIToUTF16("$5.00"),
            GetLabelText(DialogViewID::ORDER_SUMMARY_TOTAL_AMOUNT_LABEL));
  // There's only the total line.
  EXPECT_EQ(1, dialog_view()
                   ->view_stack_for_testing()
                   ->top()
                   ->GetViewByID(static_cast<int>(DialogViewID::CONTENT_VIEW))
                   ->child_count());
}

IN_PROC_BROWSER_TEST_F(
    PaymentRequestModifiersTest,
    ModifierAppliedIfApplicableSelectedInstrumentWithoutTypeOrNetwork) {
  autofill::AutofillProfile profile(autofill::test::GetFullProfile());
  AddAutofillProfile(profile);
  autofill::CreditCard card(
      autofill::test::GetMaskedServerCard());  // Mastercard card.
  card.set_billing_address_id(profile.guid());
  card.set_card_type(autofill::CreditCard::CardType::CARD_TYPE_CREDIT);
  AddCreditCard(card);

  InvokePaymentRequestUI();
  OpenOrderSummaryScreen();

  EXPECT_EQ(base::ASCIIToUTF16("$4.00"),
            GetLabelText(DialogViewID::ORDER_SUMMARY_TOTAL_AMOUNT_LABEL));
  // A line for the discount and one for the total.
  EXPECT_EQ(2, dialog_view()
                   ->view_stack_for_testing()
                   ->top()
                   ->GetViewByID(static_cast<int>(DialogViewID::CONTENT_VIEW))
                   ->child_count());
}

IN_PROC_BROWSER_TEST_F(
    PaymentRequestModifiersTest,
    ModifierAppliedIfApplicableSelectedInstrumentWithCreditSupportedType) {
  autofill::AutofillProfile profile(autofill::test::GetFullProfile());
  AddAutofillProfile(profile);
  autofill::CreditCard card(
      autofill::test::GetMaskedServerCard());  // Mastercard card.
  card.set_billing_address_id(profile.guid());
  card.set_card_type(autofill::CreditCard::CardType::CARD_TYPE_CREDIT);
  AddCreditCard(card);

  ResetEventObserver(DialogEvent::DIALOG_OPENED);
  content::WebContents* web_contents = GetActiveWebContents();
  const std::string click_buy_button_js =
      "(function() { "
      "document.getElementById('credit_supported_type')."
      "click(); })();";
  ASSERT_TRUE(content::ExecuteScript(web_contents, click_buy_button_js));
  WaitForObservedEvent();
  // The web-modal dialog should be open.
  web_modal::WebContentsModalDialogManager* web_contents_modal_dialog_manager =
      web_modal::WebContentsModalDialogManager::FromWebContents(web_contents);
  EXPECT_TRUE(web_contents_modal_dialog_manager->IsDialogActive());

  OpenOrderSummaryScreen();

  EXPECT_EQ(base::ASCIIToUTF16("$4.00"),
            GetLabelText(DialogViewID::ORDER_SUMMARY_TOTAL_AMOUNT_LABEL));
  // A line for the discount and one for the total.
  EXPECT_EQ(2, dialog_view()
                   ->view_stack_for_testing()
                   ->top()
                   ->GetViewByID(static_cast<int>(DialogViewID::CONTENT_VIEW))
                   ->child_count());
}

IN_PROC_BROWSER_TEST_F(
    PaymentRequestModifiersTest,
    ModifierNotAppliedIfSelectedInstrumentWithDebitSupportedType) {
  autofill::AutofillProfile profile(autofill::test::GetFullProfile());
  AddAutofillProfile(profile);
  autofill::CreditCard card(
      autofill::test::GetMaskedServerCard());  // Mastercard card.
  card.set_billing_address_id(profile.guid());
  card.set_card_type(autofill::CreditCard::CardType::CARD_TYPE_CREDIT);
  AddCreditCard(card);

  ResetEventObserver(DialogEvent::DIALOG_OPENED);
  content::WebContents* web_contents = GetActiveWebContents();
  const std::string click_buy_button_js =
      "(function() { "
      "document.getElementById('debit_supported_type').click("
      "); })();";
  ASSERT_TRUE(content::ExecuteScript(web_contents, click_buy_button_js));
  WaitForObservedEvent();
  // The web-modal dialog should be open.
  web_modal::WebContentsModalDialogManager* web_contents_modal_dialog_manager =
      web_modal::WebContentsModalDialogManager::FromWebContents(web_contents);
  EXPECT_TRUE(web_contents_modal_dialog_manager->IsDialogActive());

  OpenOrderSummaryScreen();

  EXPECT_EQ(base::ASCIIToUTF16("$5.00"),
            GetLabelText(DialogViewID::ORDER_SUMMARY_TOTAL_AMOUNT_LABEL));
  // There's only the total line.
  EXPECT_EQ(1, dialog_view()
                   ->view_stack_for_testing()
                   ->top()
                   ->GetViewByID(static_cast<int>(DialogViewID::CONTENT_VIEW))
                   ->child_count());
}

IN_PROC_BROWSER_TEST_F(
    PaymentRequestModifiersTest,
    ModifierAppliedIfApplicableSelectedInstrumentWithMatchingNetwork) {
  autofill::AutofillProfile profile(autofill::test::GetFullProfile());
  AddAutofillProfile(profile);
  autofill::CreditCard card(
      autofill::test::GetMaskedServerCard());  // Mastercard card.
  card.set_billing_address_id(profile.guid());
  card.set_card_type(autofill::CreditCard::CardType::CARD_TYPE_CREDIT);
  AddCreditCard(card);

  ResetEventObserver(DialogEvent::DIALOG_OPENED);
  content::WebContents* web_contents = GetActiveWebContents();
  const std::string click_buy_button_js =
      "(function() { "
      "document.getElementById('mastercard_supported_network'"
      ").click(); })();";
  ASSERT_TRUE(content::ExecuteScript(web_contents, click_buy_button_js));
  WaitForObservedEvent();
  // The web-modal dialog should be open.
  web_modal::WebContentsModalDialogManager* web_contents_modal_dialog_manager =
      web_modal::WebContentsModalDialogManager::FromWebContents(web_contents);
  EXPECT_TRUE(web_contents_modal_dialog_manager->IsDialogActive());

  OpenOrderSummaryScreen();

  EXPECT_EQ(base::ASCIIToUTF16("$4.00"),
            GetLabelText(DialogViewID::ORDER_SUMMARY_TOTAL_AMOUNT_LABEL));
  // A line for the discount and one for the total.
  EXPECT_EQ(2, dialog_view()
                   ->view_stack_for_testing()
                   ->top()
                   ->GetViewByID(static_cast<int>(DialogViewID::CONTENT_VIEW))
                   ->child_count());
}

IN_PROC_BROWSER_TEST_F(
    PaymentRequestModifiersTest,
    ModifierNotAppliedIfSelectedInstrumentWithoutMatchingNetwork) {
  autofill::AutofillProfile profile(autofill::test::GetFullProfile());
  AddAutofillProfile(profile);
  autofill::CreditCard card(
      autofill::test::GetMaskedServerCard());  // Mastercard card.
  card.set_billing_address_id(profile.guid());
  card.set_card_type(autofill::CreditCard::CardType::CARD_TYPE_CREDIT);
  AddCreditCard(card);

  ResetEventObserver(DialogEvent::DIALOG_OPENED);
  content::WebContents* web_contents = GetActiveWebContents();
  const std::string click_buy_button_js =
      "(function() { "
      "document.getElementById('visa_supported_network')."
      "click(); })();";
  ASSERT_TRUE(content::ExecuteScript(web_contents, click_buy_button_js));
  WaitForObservedEvent();
  // The web-modal dialog should be open.
  web_modal::WebContentsModalDialogManager* web_contents_modal_dialog_manager =
      web_modal::WebContentsModalDialogManager::FromWebContents(web_contents);
  EXPECT_TRUE(web_contents_modal_dialog_manager->IsDialogActive());

  OpenOrderSummaryScreen();

  EXPECT_EQ(base::ASCIIToUTF16("$5.00"),
            GetLabelText(DialogViewID::ORDER_SUMMARY_TOTAL_AMOUNT_LABEL));
  // There's only the total line.
  EXPECT_EQ(1, dialog_view()
                   ->view_stack_for_testing()
                   ->top()
                   ->GetViewByID(static_cast<int>(DialogViewID::CONTENT_VIEW))
                   ->child_count());
}

IN_PROC_BROWSER_TEST_F(PaymentRequestModifiersTest,
                       ModifierNotAppliedToUnknownType) {
  autofill::AutofillProfile profile(autofill::test::GetFullProfile());
  AddAutofillProfile(profile);
  autofill::CreditCard card(autofill::test::GetCreditCard());  // Visa card.
  card.set_billing_address_id(profile.guid());
  AddCreditCard(card);

  InvokePaymentRequestUI();
  OpenOrderSummaryScreen();

  EXPECT_EQ(base::ASCIIToUTF16("$5.00"),
            GetLabelText(DialogViewID::ORDER_SUMMARY_TOTAL_AMOUNT_LABEL));
  // There's only the total line.
  EXPECT_EQ(1, dialog_view()
                   ->view_stack_for_testing()
                   ->top()
                   ->GetViewByID(static_cast<int>(DialogViewID::CONTENT_VIEW))
                   ->child_count());
}

}  // namespace payments
