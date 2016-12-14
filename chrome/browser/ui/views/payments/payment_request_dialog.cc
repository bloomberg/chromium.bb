// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/payments/payment_request_dialog.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/payments/payment_request_impl.h"
#include "chrome/grit/generated_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "content/public/browser/browser_thread.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/grid_layout.h"

namespace {

// The tag for the button that navigates back to the payment sheet.
constexpr int kBackButtonTag = 0;

// The tag for the button that navigates to the Order Summary sheet.
constexpr int kOrderSummaryTag = 1;

std::unique_ptr<views::View> CreateOrderSummaryView(
    views::ButtonListener* button_listener) {
  std::unique_ptr<views::View> view = base::MakeUnique<views::View>();

  views::GridLayout* layout = new views::GridLayout(view.get());
  view->SetLayoutManager(layout);
  views::ColumnSet* columns = layout->AddColumnSet(0);
  columns->AddColumn(views::GridLayout::TRAILING, views::GridLayout::CENTER,
                     0, views::GridLayout::USE_PREF, 0, 0);

  layout->StartRow(0, 0);
  layout->AddView(new views::Label(
      l10n_util::GetStringUTF16(IDS_PAYMENT_REQUEST_ORDER_SUMMARY_TITLE)));

  layout->StartRow(0, 0);
  views::LabelButton* back_button =
      views::MdTextButton::CreateSecondaryUiBlueButton(
          button_listener, base::ASCIIToUTF16("Back"));
  back_button->set_tag(kBackButtonTag);
  layout->AddView(back_button);

  return view;
}

std::unique_ptr<views::View> CreatePaymentSheetView(
    views::ButtonListener* button_listener) {
  std::unique_ptr<views::View> view = base::MakeUnique<views::View>();

  views::GridLayout* layout = new views::GridLayout(view.get());
  view->SetLayoutManager(layout);
  views::ColumnSet* columns = layout->AddColumnSet(0);
  columns->AddColumn(views::GridLayout::TRAILING, views::GridLayout::CENTER,
                     0, views::GridLayout::USE_PREF, 0, 0);

  layout->StartRow(0, 0);
  layout->AddView(new views::Label(
      l10n_util::GetStringUTF16(IDS_PAYMENT_REQUEST_PAYMENT_SHEET_TITLE)));

  layout->StartRow(0, 0);
  views::LabelButton* order_summary_button =
      views::MdTextButton::CreateSecondaryUiBlueButton(
          button_listener, base::ASCIIToUTF16("Order Summary"));
  order_summary_button->set_tag(kOrderSummaryTag);
  layout->AddView(order_summary_button);

  return view;
}

}  // namespace

namespace chrome {

void ShowPaymentRequestDialog(payments::PaymentRequestImpl* impl) {
  constrained_window::ShowWebModalDialogViews(
      new payments::PaymentRequestDialog(impl), impl->web_contents());
}

}  // namespace chrome

namespace payments {

PaymentRequestDialog::PaymentRequestDialog(PaymentRequestImpl* impl)
    : impl_(impl) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  SetLayoutManager(new views::FillLayout());

  view_stack_.set_owned_by_client();
  AddChildView(&view_stack_);

  ShowInitialPaymentSheet();
}

PaymentRequestDialog::~PaymentRequestDialog() {}

ui::ModalType PaymentRequestDialog::GetModalType() const {
  return ui::MODAL_TYPE_CHILD;
}

gfx::Size PaymentRequestDialog::GetPreferredSize() const {
  return gfx::Size(300, 300);
}

bool PaymentRequestDialog::Cancel() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  impl_->Cancel();
  return true;
}

void PaymentRequestDialog::ShowInitialPaymentSheet() {
  view_stack_.Push(CreatePaymentSheetView(this), false);
}

void PaymentRequestDialog::ShowOrderSummary() {
  view_stack_.Push(CreateOrderSummaryView(this), true);
}

void PaymentRequestDialog::GoBack() {
  view_stack_.Pop();
}

void PaymentRequestDialog::ButtonPressed(
    views::Button* sender, const ui::Event& event) {
  if (sender->tag() == kBackButtonTag) {
    GoBack();
  } else if (sender->tag() == kOrderSummaryTag) {
    ShowOrderSummary();
  }
}

}  // namespace payments
