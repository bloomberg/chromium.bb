// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/views/payments/payment_request_browsertest_base.h"
#include "chrome/browser/ui/views/payments/payment_request_dialog_view_ids.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/autofill/core/browser/autofill_profile.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/credit_card.h"
#include "components/payments/content/payment_request.h"
#include "components/payments/content/payment_request_web_contents_manager.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "content/public/test/browser_test_utils.h"
#include "ui/views/controls/link.h"
#include "ui/views/controls/styled_label.h"

namespace payments {

class PaymentRequestWebContentsManagerTest
    : public PaymentRequestBrowserTestBase {
 protected:
  PaymentRequestWebContentsManagerTest()
      : PaymentRequestBrowserTestBase(
            "/payment_request_multiple_requests.html") {}

 private:
  DISALLOW_COPY_AND_ASSIGN(PaymentRequestWebContentsManagerTest);
};

// If the page creates multiple PaymentRequest objects, it should not crash.
IN_PROC_BROWSER_TEST_F(PaymentRequestWebContentsManagerTest, MultipleRequests) {
  const std::vector<PaymentRequest*> payment_requests =
      GetPaymentRequests(GetActiveWebContents());
  EXPECT_EQ(5U, payment_requests.size());
}

class PaymentRequestNoShippingTest : public PaymentRequestBrowserTestBase {
 protected:
  PaymentRequestNoShippingTest()
      : PaymentRequestBrowserTestBase(
            "/payment_request_no_shipping_test.html") {}

 private:
  DISALLOW_COPY_AND_ASSIGN(PaymentRequestNoShippingTest);
};

IN_PROC_BROWSER_TEST_F(PaymentRequestNoShippingTest, OpenAndNavigateTo404) {
  InvokePaymentRequestUI();

  ResetEventObserver(DialogEvent::DIALOG_CLOSED);

  NavigateTo("/non-existent.html");

  WaitForObservedEvent();
}

IN_PROC_BROWSER_TEST_F(PaymentRequestNoShippingTest, OpenAndNavigateToSame) {
  InvokePaymentRequestUI();

  ResetEventObserver(DialogEvent::DIALOG_CLOSED);

  NavigateTo("/payment_request_no_shipping_test.html");

  WaitForObservedEvent();
}

IN_PROC_BROWSER_TEST_F(PaymentRequestNoShippingTest, OpenAndReload) {
  InvokePaymentRequestUI();

  ResetEventObserver(DialogEvent::DIALOG_CLOSED);

  chrome::Reload(browser(), WindowOpenDisposition::CURRENT_TAB);

  WaitForObservedEvent();
}

IN_PROC_BROWSER_TEST_F(PaymentRequestNoShippingTest, OpenAndClickCancel) {
  InvokePaymentRequestUI();

  ResetEventObserver(DialogEvent::DIALOG_CLOSED);

  ClickOnDialogViewAndWait(DialogViewID::CANCEL_BUTTON,
                           /*wait_for_animation=*/false);
}

IN_PROC_BROWSER_TEST_F(PaymentRequestNoShippingTest,
                       OrderSummaryAndClickCancel) {
  InvokePaymentRequestUI();

  OpenOrderSummaryScreen();

  ResetEventObserver(DialogEvent::DIALOG_CLOSED);

  ClickOnDialogViewAndWait(DialogViewID::CANCEL_BUTTON,
                           /*wait_for_animation=*/false);
}

IN_PROC_BROWSER_TEST_F(PaymentRequestNoShippingTest, PayWithVisa) {
  autofill::AutofillProfile billing_address = autofill::test::GetFullProfile();
  AddAutofillProfile(billing_address);
  autofill::CreditCard card = autofill::test::GetCreditCard();
  card.set_billing_address_id(billing_address.guid());
  AddCreditCard(card);  // Visa.

  InvokePaymentRequestUI();

  ResetEventObserver(DialogEvent::DIALOG_CLOSED);

  PayWithCreditCardAndWait(base::ASCIIToUTF16("123"));

  WaitForObservedEvent();

  // The actual structure of the card response is unit-tested.
  ExpectBodyContains({card.GetRawInfo(autofill::CREDIT_CARD_NUMBER),
                      card.GetRawInfo(autofill::CREDIT_CARD_NAME_FULL),
                      card.GetRawInfo(autofill::CREDIT_CARD_EXP_MONTH),
                      card.GetRawInfo(autofill::CREDIT_CARD_EXP_4_DIGIT_YEAR)});
  ExpectBodyContains(
      {billing_address.GetRawInfo(autofill::NAME_FIRST),
       billing_address.GetRawInfo(autofill::NAME_LAST),
       billing_address.GetRawInfo(autofill::ADDRESS_HOME_LINE1),
       billing_address.GetRawInfo(autofill::ADDRESS_HOME_LINE2),
       billing_address.GetRawInfo(autofill::ADDRESS_HOME_COUNTRY),
       billing_address.GetRawInfo(autofill::ADDRESS_HOME_ZIP),
       billing_address.GetRawInfo(autofill::ADDRESS_HOME_CITY),
       billing_address.GetRawInfo(autofill::ADDRESS_HOME_STATE)});
}

IN_PROC_BROWSER_TEST_F(PaymentRequestNoShippingTest, InvalidSSL) {
  SetInvalidSsl();

  autofill::AutofillProfile billing_address = autofill::test::GetFullProfile();
  AddAutofillProfile(billing_address);
  autofill::CreditCard card = autofill::test::GetCreditCard();
  card.set_billing_address_id(billing_address.guid());
  AddCreditCard(card);  // Visa.

  ResetEventObserver(DialogEvent::NOT_SUPPORTED_ERROR);

  EXPECT_TRUE(content::ExecuteScript(
      GetActiveWebContents(),
      "(function() { document.getElementById('buy').click(); })();"));

  WaitForObservedEvent();

  ExpectBodyContains({"NotSupportedError"});
}

class PaymentRequestAbortTest : public PaymentRequestBrowserTestBase {
 protected:
  PaymentRequestAbortTest()
      : PaymentRequestBrowserTestBase("/payment_request_abort_test.html") {}

 private:
  DISALLOW_COPY_AND_ASSIGN(PaymentRequestAbortTest);
};

// Testing the use of the abort() JS API.
IN_PROC_BROWSER_TEST_F(PaymentRequestAbortTest, OpenThenAbort) {
  InvokePaymentRequestUI();

  ResetEventObserverForSequence(
      {DialogEvent::ABORT_CALLED, DialogEvent::DIALOG_CLOSED});

  content::WebContents* web_contents = GetActiveWebContents();
  const std::string click_buy_button_js =
      "(function() { document.getElementById('abort').click(); })();";
  ASSERT_TRUE(content::ExecuteScript(web_contents, click_buy_button_js));

  WaitForObservedEvent();

  ExpectBodyContains({"Aborted"});

  // The web-modal dialog should now be closed.
  web_modal::WebContentsModalDialogManager* web_contents_modal_dialog_manager =
      web_modal::WebContentsModalDialogManager::FromWebContents(web_contents);
  EXPECT_FALSE(web_contents_modal_dialog_manager->IsDialogActive());
}

IN_PROC_BROWSER_TEST_F(PaymentRequestAbortTest,
                       AbortUnsuccessfulAfterCVCPromptShown) {
  autofill::AutofillProfile billing_address = autofill::test::GetFullProfile();
  AddAutofillProfile(billing_address);
  autofill::CreditCard card = autofill::test::GetCreditCard();
  card.set_billing_address_id(billing_address.guid());
  AddCreditCard(card);  // Visa.

  InvokePaymentRequestUI();
  OpenCVCPromptWithCVC(base::UTF8ToUTF16("123"));

  ResetEventObserver(DialogEvent::ABORT_CALLED);

  content::WebContents* web_contents = GetActiveWebContents();
  const std::string click_buy_button_js =
      "(function() { document.getElementById('abort').click(); })();";
  ASSERT_TRUE(content::ExecuteScript(web_contents, click_buy_button_js));

  WaitForObservedEvent();

  ExpectBodyContains({"Cannot abort"});
}

class PaymentRequestBasicCardTest : public PaymentRequestBrowserTestBase {
 protected:
  PaymentRequestBasicCardTest()
      : PaymentRequestBrowserTestBase("/payment_request_basic_card_test.html") {
  }

  void InvokePaymentRequestWithJs(const std::string& js) {
    ResetEventObserver(DialogEvent::DIALOG_OPENED);

    ASSERT_TRUE(content::ExecuteScript(GetActiveWebContents(), js));

    WaitForObservedEvent();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(PaymentRequestBasicCardTest);
};

// One network is specified in 'basic-card' data, one in supportedMethods.
IN_PROC_BROWSER_TEST_F(PaymentRequestBasicCardTest,
                       BasicCard_NetworksSpecified) {
  InvokePaymentRequestWithJs("buy();");

  std::vector<PaymentRequest*> requests =
      GetPaymentRequests(GetActiveWebContents());
  EXPECT_EQ(1u, requests.size());
  std::vector<std::string> supported_card_networks =
      requests[0]->spec()->supported_card_networks();
  EXPECT_EQ(2u, supported_card_networks.size());
  // The networks appear in the order in which they were specified by the
  // merchant.
  EXPECT_EQ("mastercard", supported_card_networks[0]);
  EXPECT_EQ("visa", supported_card_networks[1]);
}

// Only specifying 'basic-card' with no supportedNetworks means all networks are
// supported.
IN_PROC_BROWSER_TEST_F(PaymentRequestBasicCardTest,
                       BasicCard_NoNetworksSpecified) {
  InvokePaymentRequestWithJs("buyBasicCard();");

  std::vector<PaymentRequest*> requests =
      GetPaymentRequests(GetActiveWebContents());
  EXPECT_EQ(1u, requests.size());
  std::vector<std::string> supported_card_networks =
      requests[0]->spec()->supported_card_networks();
  // The default ordering is alphabetical.
  EXPECT_EQ(8u, supported_card_networks.size());
  EXPECT_EQ("amex", supported_card_networks[0]);
  EXPECT_EQ("diners", supported_card_networks[1]);
  EXPECT_EQ("discover", supported_card_networks[2]);
  EXPECT_EQ("jcb", supported_card_networks[3]);
  EXPECT_EQ("mastercard", supported_card_networks[4]);
  EXPECT_EQ("mir", supported_card_networks[5]);
  EXPECT_EQ("unionpay", supported_card_networks[6]);
  EXPECT_EQ("visa", supported_card_networks[7]);
}

// Specifying 'basic-card' after having explicitely included a network yields
// the expected order when in different supportedMethods lists.
IN_PROC_BROWSER_TEST_F(PaymentRequestBasicCardTest,
                       BasicCard_NetworkThenBasicCard_DifferentList) {
  InvokePaymentRequestWithJs(
      "buyHelper([{"
      "  supportedMethods: ['mastercard'],"
      "}, {"
      "  supportedMethods: ['basic-card']"
      "}]);");

  std::vector<PaymentRequest*> requests =
      GetPaymentRequests(GetActiveWebContents());
  EXPECT_EQ(1u, requests.size());
  std::vector<std::string> supported_card_networks =
      requests[0]->spec()->supported_card_networks();
  // 'mastercard' is first because it was explicitely specified first. The rest
  // is alphabetical.
  EXPECT_EQ(8u, supported_card_networks.size());
  EXPECT_EQ("mastercard", supported_card_networks[0]);
  EXPECT_EQ("amex", supported_card_networks[1]);
  EXPECT_EQ("diners", supported_card_networks[2]);
  EXPECT_EQ("discover", supported_card_networks[3]);
  EXPECT_EQ("jcb", supported_card_networks[4]);
  EXPECT_EQ("mir", supported_card_networks[5]);
  EXPECT_EQ("unionpay", supported_card_networks[6]);
  EXPECT_EQ("visa", supported_card_networks[7]);
}

// Specifying 'basic-card' after having explicitely included a network yields
// the expected order when in the same supportedMethods list.
IN_PROC_BROWSER_TEST_F(PaymentRequestBasicCardTest,
                       BasicCard_NetworkThenBasicCard_SameList) {
  InvokePaymentRequestWithJs(
      "buyHelper([{"
      "  supportedMethods: ['visa', 'basic-card']"
      "}]);");

  std::vector<PaymentRequest*> requests =
      GetPaymentRequests(GetActiveWebContents());
  EXPECT_EQ(1u, requests.size());
  std::vector<std::string> supported_card_networks =
      requests[0]->spec()->supported_card_networks();
  // 'visa' is first because it was explicitely specified first. The rest
  // is alphabetical.
  EXPECT_EQ(8u, supported_card_networks.size());
  EXPECT_EQ("visa", supported_card_networks[0]);
  EXPECT_EQ("amex", supported_card_networks[1]);
  EXPECT_EQ("diners", supported_card_networks[2]);
  EXPECT_EQ("discover", supported_card_networks[3]);
  EXPECT_EQ("jcb", supported_card_networks[4]);
  EXPECT_EQ("mastercard", supported_card_networks[5]);
  EXPECT_EQ("mir", supported_card_networks[6]);
  EXPECT_EQ("unionpay", supported_card_networks[7]);
}

// Specifying 'basic-card' with some networks after having explicitely included
// the same networks does not yield duplicates and has the expected order.
IN_PROC_BROWSER_TEST_F(PaymentRequestBasicCardTest,
                       BasicCard_NetworkThenBasicCardWithSameNetwork) {
  InvokePaymentRequestWithJs(
      "buyHelper([{"
      "  supportedMethods: ['mastercard', 'visa']"
      "}, {"
      "  supportedMethods: ['basic-card'],"
      "  data: {"
      "    supportedNetworks: ['visa', 'mastercard', 'jcb'],"
      "  }"
      "}]);");

  std::vector<PaymentRequest*> requests =
      GetPaymentRequests(GetActiveWebContents());
  EXPECT_EQ(1u, requests.size());
  std::vector<std::string> supported_card_networks =
      requests[0]->spec()->supported_card_networks();
  EXPECT_EQ(3u, supported_card_networks.size());
  EXPECT_EQ("mastercard", supported_card_networks[0]);
  EXPECT_EQ("visa", supported_card_networks[1]);
  EXPECT_EQ("jcb", supported_card_networks[2]);
}

// Test harness integrating with DialogBrowserTest to present the dialog in an
// interactive manner for visual testing.
class PaymentsRequestVisualTest
    : public SupportsTestDialog<PaymentRequestNoShippingTest> {
 protected:
  PaymentsRequestVisualTest() {}

  // TestBrowserDialog:
  void ShowDialog(const std::string& name) override {
    InvokePaymentRequestUI();
  }

  bool AlwaysCloseAsynchronously() override {
    // Bypassing Widget::CanClose() causes payments::JourneyLogger to see the
    // show, but not the close, resulting in a DCHECK in its destructor.
    return true;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(PaymentsRequestVisualTest);
};

IN_PROC_BROWSER_TEST_F(PaymentsRequestVisualTest, InvokeDialog_NoShipping) {
  RunDialog();
}

class PaymentRequestSettingsLinkTest : public PaymentRequestBrowserTestBase {
 protected:
  PaymentRequestSettingsLinkTest()
      : PaymentRequestBrowserTestBase(
            "/payment_request_no_shipping_test.html") {}

 private:
  DISALLOW_COPY_AND_ASSIGN(PaymentRequestSettingsLinkTest);
};

// Tests that clicking the settings link brings the user to settings.
IN_PROC_BROWSER_TEST_F(PaymentRequestSettingsLinkTest, ClickSettingsLink) {
  // Setup a credit card with an associated billing address.
  autofill::AutofillProfile billing_address = autofill::test::GetFullProfile();
  AddAutofillProfile(billing_address);
  autofill::CreditCard card = autofill::test::GetCreditCard();
  card.set_billing_address_id(billing_address.guid());
  AddCreditCard(card);  // Visa.

  // Click on the settings link in the payment request dialog window.
  InvokePaymentRequestUI();
  views::StyledLabel* styled_label =
      static_cast<views::StyledLabel*>(dialog_view()->GetViewByID(
          static_cast<int>(DialogViewID::DATA_SOURCE_LABEL)));
  EXPECT_TRUE(styled_label);
  // The Link is the only child of the StyledLabel.
  content::WebContentsAddedObserver web_contents_added_observer;
  styled_label->LinkClicked(
      static_cast<views::Link*>(styled_label->child_at(0)), 0);
  content::WebContents* new_tab_contents =
      web_contents_added_observer.GetWebContents();

  EXPECT_EQ("chrome://settings/autofill",
            new_tab_contents->GetVisibleURL().spec());
}

}  // namespace payments
