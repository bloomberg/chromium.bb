// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/views/payments/payment_request_interactive_uitest_base.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/payments/payment_request.h"
#include "components/payments/payment_request_web_contents_manager.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "content/public/test/browser_test_utils.h"

namespace payments {

class PaymentRequestWebContentsManagerTest
    : public PaymentRequestInteractiveTestBase {
 protected:
  PaymentRequestWebContentsManagerTest()
      : PaymentRequestInteractiveTestBase(
            "/payment_request_multiple_requests.html") {}
};

// If the page creates multiple PaymentRequest objects, it should not crash.
IN_PROC_BROWSER_TEST_F(PaymentRequestWebContentsManagerTest, MultipleRequests) {
  const std::vector<PaymentRequest*> payment_requests =
      GetPaymentRequests(GetActiveWebContents());
  EXPECT_EQ(5U, payment_requests.size());
}

class PaymentRequestNoShippingTest : public PaymentRequestInteractiveTestBase {
 protected:
  PaymentRequestNoShippingTest()
      : PaymentRequestInteractiveTestBase(
            "/payment_request_no_shipping_test.html") {}
};

IN_PROC_BROWSER_TEST_F(PaymentRequestNoShippingTest, OpenPaymentRequestSheet) {
  InvokePaymentRequestUI();
}

IN_PROC_BROWSER_TEST_F(PaymentRequestNoShippingTest, OpenAndNavigateTo404) {
  InvokePaymentRequestUI();

  ResetEventObserver(DialogEvent::DIALOG_CLOSED);

  ui_test_utils::NavigateToURL(browser(),
                               https_server()->GetURL("/non-existent.html"));

  WaitForObservedEvent();
}

IN_PROC_BROWSER_TEST_F(PaymentRequestNoShippingTest, OpenAndNavigateToSame) {
  InvokePaymentRequestUI();

  ResetEventObserver(DialogEvent::DIALOG_CLOSED);

  ui_test_utils::NavigateToURL(
      browser(),
      https_server()->GetURL("/payment_request_no_shipping_test.html"));

  WaitForObservedEvent();
}

IN_PROC_BROWSER_TEST_F(PaymentRequestNoShippingTest, OpenAndReload) {
  InvokePaymentRequestUI();

  ResetEventObserver(DialogEvent::DIALOG_CLOSED);

  chrome::Reload(browser(), WindowOpenDisposition::CURRENT_TAB);

  WaitForObservedEvent();
}

class PaymentRequestAbortTest : public PaymentRequestInteractiveTestBase {
 protected:
  PaymentRequestAbortTest()
      : PaymentRequestInteractiveTestBase("/payment_request_abort_test.html") {}
};

// Testing the use of the abort() JS API.
IN_PROC_BROWSER_TEST_F(PaymentRequestAbortTest, OpenThenAbort) {
  InvokePaymentRequestUI();

  ResetEventObserver(DialogEvent::DIALOG_CLOSED);

  content::WebContents* web_contents = GetActiveWebContents();
  const std::string click_buy_button_js =
      "(function() { document.getElementById('abort').click(); })();";
  ASSERT_TRUE(content::ExecuteScript(web_contents, click_buy_button_js));

  WaitForObservedEvent();

  // The web-modal dialog should now be closed.
  web_modal::WebContentsModalDialogManager* web_contents_modal_dialog_manager =
      web_modal::WebContentsModalDialogManager::FromWebContents(web_contents);
  EXPECT_FALSE(web_contents_modal_dialog_manager->IsDialogActive());
}

}  // namespace payments
