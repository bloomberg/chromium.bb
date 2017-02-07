// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/payments/payment_request_row_view.h"

#include "chrome/browser/ui/views/payments/payment_request_views_util.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/views/background.h"
#include "ui/views/border.h"

namespace payments {

PaymentRequestRowView::PaymentRequestRowView(
    views::ButtonListener* listener)
  : views::CustomButton(listener) {
  SetBorder(payments::CreatePaymentRequestRowBorder());
}

PaymentRequestRowView::~PaymentRequestRowView() {}

// views::CustomButton:
void PaymentRequestRowView::StateChanged(ButtonState old_state) {
  if (state() == views::Button::STATE_HOVERED ||
      state() == views::Button::STATE_PRESSED) {
    set_background(views::Background::CreateSolidBackground(SK_ColorLTGRAY));
  } else {
    set_background(nullptr);
  }
}

}  // namespace payments
