// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "chrome/browser/ui/views/payments/payment_request_dialog_view_ids.h"
#include "chrome/browser/ui/views/payments/payment_request_interactive_uitest_base.h"

namespace payments {

class PaymentRequestCreditCardEditorTest
    : public PaymentRequestInteractiveTestBase {
 protected:
  PaymentRequestCreditCardEditorTest()
      : PaymentRequestInteractiveTestBase(
            "/payment_request_no_shipping_test.html") {}

 private:
  DISALLOW_COPY_AND_ASSIGN(PaymentRequestCreditCardEditorTest);
};

IN_PROC_BROWSER_TEST_F(PaymentRequestCreditCardEditorTest,
                       OpenCreditCardEditor) {
  InvokePaymentRequestUI();

  OpenPaymentMethodScreen();

  OpenCreditCardEditorScreen();

  // TODO(mathp): Test input and validation.

  ResetEventObserver(DialogEvent::BACK_NAVIGATION);

  ClickOnDialogViewAndWait(DialogViewID::EDITOR_SAVE_BUTTON);

  WaitForObservedEvent();
}

}  // namespace payments
