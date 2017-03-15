// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/payments/shipping_option_view_controller.h"

#include "chrome/browser/ui/views/payments/payment_request_views_util.h"
#include "components/payments/content/payment_request.h"
#include "components/payments/content/payment_request_spec.h"

namespace payments {

namespace {

class ShippingOptionItem : public PaymentRequestItemList::Item {
 public:
  ShippingOptionItem(payments::mojom::PaymentShippingOption* shipping_option,
                     PaymentRequest* request,
                     PaymentRequestItemList* parent_list,
                     bool selected)
      : PaymentRequestItemList::Item(request, parent_list, selected),
        shipping_option_(shipping_option) {}
  ~ShippingOptionItem() override {}

 private:
  // payments::PaymentRequestItemList::Item:
  std::unique_ptr<views::View> CreateItemView() override {
    return CreateShippingOptionLabel(
        shipping_option_, request()->spec()->GetFormattedCurrencyAmount(
                              shipping_option_->amount->value));
  }

  void SelectedStateChanged() override {}

  mojom::PaymentShippingOption* shipping_option_;

  DISALLOW_COPY_AND_ASSIGN(ShippingOptionItem);
};

}  // namespace

ShippingOptionViewController::ShippingOptionViewController(
    PaymentRequest* request,
    PaymentRequestDialogView* dialog)
    : PaymentRequestSheetController(request, dialog) {
  for (const auto& option : request->spec()->details().shipping_options) {
    shipping_option_list_.AddItem(base::MakeUnique<ShippingOptionItem>(
        option.get(), request, &shipping_option_list_,
        option.get() == request->state()->selected_shipping_option()));
  }
}

ShippingOptionViewController::~ShippingOptionViewController() {}

std::unique_ptr<views::View> ShippingOptionViewController::CreateView() {
  std::unique_ptr<views::View> list_view =
      shipping_option_list_.CreateListView();
  return CreatePaymentView(
      CreateSheetHeaderView(true,
                            GetShippingOptionSectionString(
                                request()->spec()->options().shipping_type),
                            this),
      std::move(list_view));
}

std::unique_ptr<views::View>
ShippingOptionViewController::CreateExtraFooterView() {
  return nullptr;
}

}  // namespace payments
