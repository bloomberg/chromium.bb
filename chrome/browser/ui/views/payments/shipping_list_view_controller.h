// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAYMENTS_SHIPPING_LIST_VIEW_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_PAYMENTS_SHIPPING_LIST_VIEW_CONTROLLER_H_

#include "base/macros.h"
#include "chrome/browser/ui/views/payments/payment_request_item_list.h"
#include "chrome/browser/ui/views/payments/payment_request_sheet_controller.h"

namespace payments {

class PaymentRequest;
class PaymentRequestDialogView;

// The PaymentRequestSheetController subtype for the Shipping address list
// screen of the Payment Request flow.
class ShippingListViewController : public PaymentRequestSheetController {
 public:
  // Does not take ownership of the arguments, which should outlive this object.
  ShippingListViewController(PaymentRequest* request,
                             PaymentRequestDialogView* dialog);
  ~ShippingListViewController() override;

  // PaymentRequestSheetController:
  std::unique_ptr<views::View> CreateView() override;

 private:
  PaymentRequestItemList list_;

  DISALLOW_COPY_AND_ASSIGN(ShippingListViewController);
};

}  // namespace payments

#endif  // CHROME_BROWSER_UI_VIEWS_PAYMENTS_SHIPPING_LIST_VIEW_CONTROLLER_H_
