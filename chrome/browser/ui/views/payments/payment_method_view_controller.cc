// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/payments/payment_method_view_controller.h"

#include <memory>
#include <utility>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/ui/views/payments/payment_request_dialog_view.h"
#include "chrome/browser/ui/views/payments/payment_request_views_util.h"
#include "components/payments/payment_request.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace payments {

PaymentMethodViewController::PaymentMethodViewController(
    PaymentRequest* request,
    PaymentRequestDialogView* dialog)
    : PaymentRequestSheetController(request, dialog) {}

PaymentMethodViewController::~PaymentMethodViewController() {}

std::unique_ptr<views::View> PaymentMethodViewController::CreateView() {
  std::unique_ptr<views::View> content_view = base::MakeUnique<views::View>();

  // TODO(anthonyvd): populate this view.

  return CreatePaymentView(CreateSheetHeaderView(
          true,
          l10n_util::GetStringUTF16(
              IDS_PAYMENT_REQUEST_PAYMENT_METHOD_SECTION_NAME),
          this),
      std::move(content_view));
}

void PaymentMethodViewController::ButtonPressed(
    views::Button* sender, const ui::Event& event) {
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
