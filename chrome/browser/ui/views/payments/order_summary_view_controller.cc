// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/payments/order_summary_view_controller.h"

#include <memory>
#include <utility>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/payments/payment_request_impl.h"
#include "chrome/browser/ui/views/payments/payment_request_dialog.h"
#include "chrome/browser/ui/views/payments/payment_request_views_util.h"
#include "chrome/grit/generated_resources.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/view.h"

namespace payments {

OrderSummaryViewController::OrderSummaryViewController(
    PaymentRequestImpl* impl, PaymentRequestDialog* dialog)
  : PaymentRequestSheetController(impl, dialog) {}

OrderSummaryViewController::~OrderSummaryViewController() {}

std::unique_ptr<views::View> OrderSummaryViewController::CreateView() {
  std::unique_ptr<views::View> content_view = base::MakeUnique<views::View>();

  views::GridLayout* layout = new views::GridLayout(content_view.get());
  content_view->SetLayoutManager(layout);
  views::ColumnSet* columns = layout->AddColumnSet(0);
  columns->AddColumn(views::GridLayout::FILL, views::GridLayout::CENTER,
                     0, views::GridLayout::USE_PREF, 0, 0);

  layout->StartRow(0, 0);
  layout->AddView(new views::Label(
    l10n_util::GetStringFUTF16(
        IDS_PAYMENT_REQUEST_ORDER_SUMMARY_SECTION_TOTAL_FORMAT,
        base::ASCIIToUTF16(impl()->details()->total->label),
        base::ASCIIToUTF16(impl()->details()->total->amount->value),
        base::ASCIIToUTF16(impl()->details()->total->amount->currency))));

  return payments::CreatePaymentView(
      CreateSheetHeaderView(
          true,
          l10n_util::GetStringUTF16(IDS_PAYMENT_REQUEST_ORDER_SUMMARY_TITLE),
          this),
      std::move(content_view));
}

void OrderSummaryViewController::ButtonPressed(
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
