// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/payments/order_summary_view_controller.h"

#include <memory>
#include <utility>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/views/payments/payment_request_dialog.h"
#include "chrome/browser/ui/views/payments/payment_request_views_util.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/view.h"

namespace {

// The tag for the button that navigates back to the payment sheet.
constexpr int kBackButtonTag = 0;

}  // namespace

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
  columns->AddColumn(views::GridLayout::TRAILING, views::GridLayout::CENTER,
                     0, views::GridLayout::USE_PREF, 0, 0);

  layout->StartRow(0, 0);
  views::LabelButton* back_button =
      views::MdTextButton::CreateSecondaryUiBlueButton(
          this, base::ASCIIToUTF16("Back"));
  back_button->set_tag(kBackButtonTag);
  layout->AddView(back_button);

  return payments::CreatePaymentView(
      l10n_util::GetStringUTF16(IDS_PAYMENT_REQUEST_ORDER_SUMMARY_TITLE),
      std::move(content_view));
}

void OrderSummaryViewController::ButtonPressed(
    views::Button* sender, const ui::Event& event) {
  DCHECK_EQ(kBackButtonTag, sender->tag());

  dialog()->GoBack();
}

}  // namespace payments
