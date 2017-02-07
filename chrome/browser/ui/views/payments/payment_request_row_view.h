// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAYMENTS_PAYMENT_REQUEST_ROW_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PAYMENTS_PAYMENT_REQUEST_ROW_VIEW_H_

#include "base/macros.h"
#include "ui/views/controls/button/custom_button.h"

namespace payments {

// This class implements a clickable row of the Payment Request dialog that
// darkens on hover and displays a horizontal ruler on its lower bound.
class PaymentRequestRowView : public views::CustomButton {
 public:
  explicit PaymentRequestRowView(views::ButtonListener* listener);
  ~PaymentRequestRowView() override;

  // views::CustomButton:
  void StateChanged(ButtonState old_state) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(PaymentRequestRowView);
};

}  // namespace payments

#endif  // CHROME_BROWSER_UI_VIEWS_PAYMENTS_PAYMENT_REQUEST_ROW_VIEW_H_
