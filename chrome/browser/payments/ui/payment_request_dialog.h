// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAYMENTS_UI_PAYMENT_REQUEST_DIALOG_H_
#define CHROME_BROWSER_PAYMENTS_UI_PAYMENT_REQUEST_DIALOG_H_

#include "components/payments/payment_request.mojom.h"
#include "ui/views/controls/label.h"
#include "ui/views/window/dialog_delegate.h"

namespace payments {

class PaymentRequestDialog : public views::DialogDelegateView {
 public:
  explicit PaymentRequestDialog(
      payments::mojom::PaymentRequestClientPtr client);
  ~PaymentRequestDialog() override;

  // views::WidgetDelegate
  ui::ModalType GetModalType() const override;

  // views::View
  gfx::Size GetPreferredSize() const override;

  // views::DialogDelegate
  bool Cancel() override;

 private:
  payments::mojom::PaymentRequestClientPtr client_;
  std::unique_ptr<views::Label> label_;

  DISALLOW_COPY_AND_ASSIGN(PaymentRequestDialog);
};

}  // namespace payments

#endif  // CHROME_BROWSER_PAYMENTS_UI_PAYMENT_REQUEST_DIALOG_H_
