// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CONTENT_PAYMENT_REQUEST_DIALOG_H_
#define COMPONENTS_PAYMENTS_CONTENT_PAYMENT_REQUEST_DIALOG_H_

namespace payments {

// Used to interact with a cross-platform Payment Request dialog.
class PaymentRequestDialog {
 public:
  virtual ~PaymentRequestDialog() {}

  virtual void ShowDialog() = 0;

  virtual void CloseDialog() = 0;
};

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CONTENT_PAYMENT_REQUEST_DIALOG_H_
