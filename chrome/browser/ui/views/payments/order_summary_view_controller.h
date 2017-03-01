// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAYMENTS_ORDER_SUMMARY_VIEW_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_PAYMENTS_ORDER_SUMMARY_VIEW_CONTROLLER_H_

#include "base/macros.h"
#include "chrome/browser/ui/views/payments/payment_request_sheet_controller.h"
#include "components/payments/content/payment_request.h"

namespace payments {

class PaymentRequest;
class PaymentRequestDialogView;

// The PaymentRequestSheetController subtype for the Order Summary screen of the
// Payment Request flow.
class OrderSummaryViewController : public PaymentRequestSheetController,
                                   public PaymentRequest::Observer {
 public:
  // Does not take ownership of the arguments, which should outlive this object.
  OrderSummaryViewController(PaymentRequest* request,
                             PaymentRequestDialogView* dialog);
  ~OrderSummaryViewController() override;

  // PaymentRequestSheetController:
  std::unique_ptr<views::View> CreateView() override;
  std::unique_ptr<views::Button> CreatePrimaryButton() override;

  // PaymentRequest::Observer:
  void OnSelectedInformationChanged() override;

 private:
  void UpdatePayButtonState(bool enabled);

  views::Button* pay_button_;

  DISALLOW_COPY_AND_ASSIGN(OrderSummaryViewController);
};

}  // namespace payments

#endif  // CHROME_BROWSER_UI_VIEWS_PAYMENTS_ORDER_SUMMARY_VIEW_CONTROLLER_H_
