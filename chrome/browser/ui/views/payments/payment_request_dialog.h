// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAYMENTS_PAYMENT_REQUEST_DIALOG_H_
#define CHROME_BROWSER_UI_VIEWS_PAYMENTS_PAYMENT_REQUEST_DIALOG_H_

#include "ui/views/controls/label.h"
#include "ui/views/window/dialog_delegate.h"

namespace payments {

class PaymentRequestImpl;

// The dialog delegate that represents a desktop WebPayments dialog. This class
// is responsible for displaying the view associated with the current state of
// the WebPayments flow and managing the transition between those states.
class PaymentRequestDialog : public views::DialogDelegateView {
 public:
  explicit PaymentRequestDialog(PaymentRequestImpl* impl);
  ~PaymentRequestDialog() override;

  // views::WidgetDelegate:
  ui::ModalType GetModalType() const override;

  // views::View:
  gfx::Size GetPreferredSize() const override;

  // views::DialogDelegate:
  bool Cancel() override;

 private:
  // Non-owned reference to the PaymentRequestImpl that initiated this dialog.
  // Since the PaymentRequestImpl object always outlives this one, the pointer
  // should always be valid even though there is no direct ownership
  // relationship between the two.
  PaymentRequestImpl* impl_;
  std::unique_ptr<views::Label> label_;

  DISALLOW_COPY_AND_ASSIGN(PaymentRequestDialog);
};

}  // namespace payments

#endif  // CHROME_BROWSER_UI_VIEWS_PAYMENTS_PAYMENT_REQUEST_DIALOG_H_
