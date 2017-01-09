// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAYMENTS_PAYMENT_REQUEST_DIALOG_H_
#define CHROME_BROWSER_UI_VIEWS_PAYMENTS_PAYMENT_REQUEST_DIALOG_H_

#include <map>
#include <memory>

#include "base/macros.h"
#include "chrome/browser/ui/views/payments/view_stack.h"
#include "ui/views/window/dialog_delegate.h"

namespace payments {

class PaymentRequest;
class PaymentRequestSheetController;

// Maps views owned by PaymentRequestDialog::view_stack_ to their controller.
// PaymentRequestDialog is responsible for listening for those views being
// removed from the hierarchy and delete the associated controllers.
using ControllerMap =
    std::map<views::View*, std::unique_ptr<PaymentRequestSheetController>>;

// The dialog delegate that represents a desktop WebPayments dialog. This class
// is responsible for displaying the view associated with the current state of
// the WebPayments flow and managing the transition between those states.
class PaymentRequestDialog : public views::DialogDelegateView {
 public:
  explicit PaymentRequestDialog(PaymentRequest* request);
  ~PaymentRequestDialog() override;

  // views::WidgetDelegate
  ui::ModalType GetModalType() const override;

  // views::DialogDelegate
  bool Cancel() override;
  bool ShouldShowCloseButton() const override;
  int GetDialogButtons() const override;

  void GoBack();
  void ShowOrderSummary();
  void CloseDialog();

 private:
  void ShowInitialPaymentSheet();

  // views::View
  gfx::Size GetPreferredSize() const override;
  void ViewHierarchyChanged(const ViewHierarchyChangedDetails& details)
      override;

  // Non-owned reference to the PaymentRequest that initiated this dialog. Since
  // the PaymentRequest object always outlives this one, the pointer should
  // always be valid even though there is no direct ownership relationship
  // between the two.
  PaymentRequest* request_;
  ControllerMap controller_map_;
  ViewStack view_stack_;

  DISALLOW_COPY_AND_ASSIGN(PaymentRequestDialog);
};

}  // namespace payments

#endif  // CHROME_BROWSER_UI_VIEWS_PAYMENTS_PAYMENT_REQUEST_DIALOG_H_
