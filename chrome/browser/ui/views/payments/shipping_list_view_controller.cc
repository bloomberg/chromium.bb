// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/payments/shipping_list_view_controller.h"

#include <memory>
#include <utility>

#include "chrome/browser/ui/views/payments/payment_request_dialog_view.h"
#include "chrome/browser/ui/views/payments/payment_request_views_util.h"
#include "components/payments/payment_request.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/layout/box_layout.h"

namespace payments {

ShippingListViewController::ShippingListViewController(
    PaymentRequest* request,
    PaymentRequestDialogView* dialog)
    : PaymentRequestSheetController(request, dialog) {}

ShippingListViewController::~ShippingListViewController() {}

std::unique_ptr<views::View> ShippingListViewController::CreateView() {
  std::unique_ptr<views::View> content_view = base::MakeUnique<views::View>();

  views::BoxLayout* layout =
      new views::BoxLayout(views::BoxLayout::kVertical, 0, 0, 0);
  layout->set_main_axis_alignment(views::BoxLayout::MAIN_AXIS_ALIGNMENT_START);
  layout->set_cross_axis_alignment(
      views::BoxLayout::CROSS_AXIS_ALIGNMENT_STRETCH);
  content_view->SetLayoutManager(layout);

  for (auto& profile : request()->shipping_profiles()) {
    // TODO(tmartino): Pass an actual locale in place of empty string.
    content_view->AddChildView(
        GetShippingAddressLabel(AddressStyleType::DETAILED, std::string(),
                                *profile)
            .release());
  }

  return CreatePaymentView(
      CreateSheetHeaderView(
          /* show_back_arrow = */ true,
          l10n_util::GetStringUTF16(IDS_PAYMENT_REQUEST_SHIPPING_SECTION_NAME),
          this),
      std::move(content_view));
}

void ShippingListViewController::ButtonPressed(views::Button* sender,
                                               const ui::Event& event) {
  switch (sender->tag()) {
    case static_cast<int>(PaymentRequestCommonTags::CLOSE_BUTTON_TAG):
      dialog()->CloseDialog();
      break;
    case static_cast<int>(PaymentRequestCommonTags::BACK_BUTTON_TAG):
      dialog()->GoBack();
      break;
    default:
      NOTREACHED();
  }
}

}  // namespace payments
