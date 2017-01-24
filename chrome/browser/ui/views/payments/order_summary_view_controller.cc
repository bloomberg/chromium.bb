// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/payments/order_summary_view_controller.h"

#include <memory>
#include <utility>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/views/payments/payment_request_dialog_view.h"
#include "chrome/browser/ui/views/payments/payment_request_views_util.h"
#include "chrome/grit/generated_resources.h"
#include "components/payments/currency_formatter.h"
#include "components/payments/payment_request.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/font.h"
#include "ui/views/border.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/view.h"

namespace payments {

namespace {

// Creates a view for a line item to be displayed in the Order Summary Sheet.
// |label| is the text in the left-aligned label and |amount| is the text of the
// right-aliged label in the row. The |amount| text is bold if |bold_amount| is
// true, which is only the case for the last row containing the total of the
// order.
std::unique_ptr<views::View> CreateLineItemView(const base::string16& label,
                                                const base::string16& amount,
                                                bool bold_amount) {
  std::unique_ptr<views::View> row = base::MakeUnique<views::View>();

  row->SetBorder(views::CreateSolidSidedBorder(0, 0, 1, 0, SK_ColorLTGRAY));

  views::GridLayout* layout = new views::GridLayout(row.get());

  constexpr int kRowVerticalInset = 12;
  layout->SetInsets(kRowVerticalInset, 0, kRowVerticalInset, 0);

  row->SetLayoutManager(layout);
  views::ColumnSet* columns = layout->AddColumnSet(0);
  columns->AddColumn(views::GridLayout::LEADING, views::GridLayout::CENTER,
                     0, views::GridLayout::USE_PREF, 0, 0);
  columns->AddPaddingColumn(1, 0);
  columns->AddColumn(views::GridLayout::TRAILING, views::GridLayout::CENTER,
                     0, views::GridLayout::USE_PREF, 0, 0);

  layout->StartRow(0, 0);
  layout->AddView(new views::Label(label));
  views::StyledLabel::RangeStyleInfo style_info;
  if (bold_amount)
    style_info.weight = gfx::Font::Weight::BOLD;

  std::unique_ptr<views::StyledLabel> amount_label =
      base::MakeUnique<views::StyledLabel>(amount, nullptr);
  amount_label->SetDefaultStyle(style_info);
  amount_label->SizeToFit(0);
  layout->AddView(amount_label.release());

  return row;
}

}  // namespace

OrderSummaryViewController::OrderSummaryViewController(
    PaymentRequest* request,
    PaymentRequestDialogView* dialog)
    : PaymentRequestSheetController(request, dialog) {}

OrderSummaryViewController::~OrderSummaryViewController() {}

std::unique_ptr<views::View> OrderSummaryViewController::CreateView() {
  std::unique_ptr<views::View> content_view = base::MakeUnique<views::View>();

  views::BoxLayout* layout = new views::BoxLayout(
      views::BoxLayout::kVertical, 0, 0, 0);
  layout->set_main_axis_alignment(views::BoxLayout::MAIN_AXIS_ALIGNMENT_START);
  layout->set_cross_axis_alignment(
      views::BoxLayout::CROSS_AXIS_ALIGNMENT_STRETCH);
  content_view->SetLayoutManager(layout);

  CurrencyFormatter* formatter = request()->GetOrCreateCurrencyFormatter(
      request()->details()->total->amount->currency,
      request()->details()->total->amount->currency_system,
      g_browser_process->GetApplicationLocale());

  for (const auto& item: request()->details()->display_items) {
    content_view->AddChildView(
        CreateLineItemView(base::UTF8ToUTF16(item->label),
                           formatter->Format(item->amount->value),
                           false).release());
  }

  base::string16 total_label_value = l10n_util::GetStringFUTF16(
      IDS_PAYMENT_REQUEST_ORDER_SUMMARY_SHEET_TOTAL_FORMAT,
      base::UTF8ToUTF16(request()->details()->total->amount->currency),
      formatter->Format(request()->details()->total->amount->value));

  content_view->AddChildView(
      CreateLineItemView(base::UTF8ToUTF16(request()->details()->total->label),
                         total_label_value,
                         true).release());

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
