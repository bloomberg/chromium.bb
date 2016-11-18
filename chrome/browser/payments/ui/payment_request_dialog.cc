// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/payments/ui/payment_request_dialog.h"
#include "content/public/browser/browser_thread.h"
#include "ui/views/layout/fill_layout.h"

namespace payments {

PaymentRequestDialog::PaymentRequestDialog(
    payments::mojom::PaymentRequestClientPtr client)
    : client_(std::move(client)),
      label_(new views::Label(base::ASCIIToUTF16("Payments dialog"))) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  SetLayoutManager(new views::FillLayout());
  AddChildView(label_.get());
}

PaymentRequestDialog::~PaymentRequestDialog() {}

ui::ModalType PaymentRequestDialog::GetModalType() const {
  return ui::MODAL_TYPE_CHILD;
}

gfx::Size PaymentRequestDialog::GetPreferredSize() const {
  gfx::Size ps = label_->GetPreferredSize();
  ps.Enlarge(200, 200);
  return ps;
}

bool PaymentRequestDialog::Cancel() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  client_->OnError(payments::mojom::PaymentErrorReason::USER_CANCEL);
  return true;
}

}  // namespace payments
