// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAYMENTS_PAYMENT_REQUEST_SHEET_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_PAYMENTS_PAYMENT_REQUEST_SHEET_CONTROLLER_H_

#include <memory>

#include "base/logging.h"
#include "base/macros.h"

namespace views {
class View;
}

namespace payments {

class PaymentRequestDialog;
class PaymentRequestImpl;

// The base class for objects responsible for the creation and event handling in
// views shown in the PaymentRequestDialog.
class PaymentRequestSheetController {
 public:
  // Objects of this class are owned by |dialog|, so it's a non-owned pointer
  // that should be valid throughout this object's lifetime.
  // |impl| is also not owned by this and is guaranteed to outlive dialog.
  // Neither |impl| or |dialog| should be null.
  PaymentRequestSheetController(PaymentRequestImpl* impl,
                                PaymentRequestDialog* dialog)
    : impl_(impl),
      dialog_(dialog) {
    DCHECK(impl_);
    DCHECK(dialog_);
  }
  virtual ~PaymentRequestSheetController() {}

  virtual std::unique_ptr<views::View> CreateView() = 0;

  // The PaymentRequestImpl object associated with this instance of the dialog.
  // Caller should not take ownership of the result.
  PaymentRequestImpl* impl() { return impl_; }

  // The dialog that contains and owns this object.
  // Caller should not take ownership of the result.
  PaymentRequestDialog* dialog() { return dialog_; }

 private:
  // Not owned. Will outlive this.
  PaymentRequestImpl* impl_;
  // Not owned. Will outlive this.
  PaymentRequestDialog* dialog_;

  DISALLOW_COPY_AND_ASSIGN(PaymentRequestSheetController);
};

}  // namespace payments

#endif  // CHROME_BROWSER_UI_VIEWS_PAYMENTS_PAYMENT_REQUEST_SHEET_CONTROLLER_H_
