// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/payments/payment_sheet_view_controller.h"

#include <memory>
#include <utility>

#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/views/payments/payment_request_dialog.h"
#include "chrome/browser/ui/views/payments/payment_request_views_util.h"
#include "chrome/grit/generated_resources.h"
#include "components/payments/currency_formatter.h"
#include "components/payments/payment_request.h"
#include "components/strings/grit/components_strings.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/font.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/range/range.h"
#include "ui/gfx/vector_icons_public.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/custom_button.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/view.h"

namespace payments {
namespace {

enum class PaymentSheetViewControllerTags {
  // The tag for the button that navigates to the Order Summary sheet.
  SHOW_ORDER_SUMMARY_BUTTON = static_cast<int>(
      payments::PaymentRequestCommonTags::PAYMENT_REQUEST_COMMON_TAG_MAX),
};

// Creates a clickable row to be displayed in the Payment Sheet. It contains
// a section name and some content, followed by a chevron as a clickability
// affordance. Both, either, or none of |content_view| and |extra_content_view|
// may be present, the difference between the two being that content is pinned
// to the left and extra_content is pinned to the right.
// The row also displays a light gray horizontal ruler on its lower boundary.
// +----------------------------+
// | Name | Content | Extra | > |
// +~~~~~~~~~~~~~~~~~~~~~~~~~~~~+ <-- ruler
class PaymentSheetRow : public views::CustomButton {
 public:
  PaymentSheetRow(views::ButtonListener* listener,
                  const base::string16& section_name,
                  std::unique_ptr<views::View> content_view,
                  std::unique_ptr<views::View> extra_content_view)
    : views::CustomButton(listener) {
    SetBorder(views::CreateSolidSidedBorder(0, 0, 1, 0, SK_ColorLTGRAY));
    views::GridLayout* layout = new views::GridLayout(this);

    constexpr int kRowVerticalInset = 18;
    // The rows have extra inset compared to the header so that their right edge
    // lines up with the close button's X rather than its invisible right edge.
    constexpr int kRowExtraRightInset = 8;
    layout->SetInsets(
        kRowVerticalInset, 0, kRowVerticalInset, kRowExtraRightInset);
    SetLayoutManager(layout);

    views::ColumnSet* columns = layout->AddColumnSet(0);
    // A column for the section name.
    columns->AddColumn(views::GridLayout::LEADING, views::GridLayout::CENTER,
                       0, views::GridLayout::USE_PREF, 0, 0);
    // A column for the content.
    columns->AddColumn(views::GridLayout::FILL, views::GridLayout::CENTER,
                       1, views::GridLayout::USE_PREF, 0, 0);
    // A column for the extra content.
    columns->AddColumn(views::GridLayout::TRAILING, views::GridLayout::CENTER,
                       0, views::GridLayout::USE_PREF, 0, 0);

    constexpr int kPaddingColumnsWidth = 25;
    columns->AddPaddingColumn(0, kPaddingColumnsWidth);
    // A column for the chevron.
    columns->AddColumn(views::GridLayout::TRAILING, views::GridLayout::CENTER,
                       0, views::GridLayout::USE_PREF, 0, 0);

    layout->StartRow(0, 0);
    views::Label* name_label = new views::Label(section_name);
    layout->AddView(name_label);

    if (content_view) {
      layout->AddView(content_view.release());
    } else {
      layout->SkipColumns(1);
    }

    if (extra_content_view) {
      layout->AddView(extra_content_view.release());
    } else {
      layout->SkipColumns(1);
    }

    views::ImageView* chevron = new views::ImageView();
    chevron->SetImage(gfx::CreateVectorIcon(
        gfx::VectorIconId::SUBMENU_ARROW,
        color_utils::DeriveDefaultIconColor(name_label->enabled_color())));
    layout->AddView(chevron);
  }

  DISALLOW_COPY_AND_ASSIGN(PaymentSheetRow);
};

}  // namespace

PaymentSheetViewController::PaymentSheetViewController(
    PaymentRequest* request,
    PaymentRequestDialog* dialog)
    : PaymentRequestSheetController(request, dialog) {}

PaymentSheetViewController::~PaymentSheetViewController() {}

std::unique_ptr<views::View> PaymentSheetViewController::CreateView() {
  std::unique_ptr<views::View> content_view = base::MakeUnique<views::View>();

  views::GridLayout* layout = new views::GridLayout(content_view.get());
  content_view->SetLayoutManager(layout);
  views::ColumnSet* columns = layout->AddColumnSet(0);
  columns->AddColumn(views::GridLayout::FILL, views::GridLayout::CENTER,
                     1, views::GridLayout::USE_PREF, 0, 0);

  layout->StartRow(0, 0);
  layout->AddView(CreatePaymentSheetSummaryRow().release());

  return CreatePaymentView(
      CreateSheetHeaderView(
          false,
          l10n_util::GetStringUTF16(IDS_PAYMENT_REQUEST_PAYMENT_SHEET_TITLE),
          this),
      std::move(content_view));
}

void PaymentSheetViewController::ButtonPressed(
    views::Button* sender, const ui::Event& event) {
  switch (sender->tag()) {
    case static_cast<int>(PaymentRequestCommonTags::CLOSE_BUTTON_TAG):
      dialog()->CloseDialog();
      break;
    case static_cast<int>(
        PaymentSheetViewControllerTags::SHOW_ORDER_SUMMARY_BUTTON):
      dialog()->ShowOrderSummary();
      break;
    default:
      NOTREACHED();
  }
}

std::unique_ptr<views::View>
PaymentSheetViewController::CreateOrderSummarySectionContent() {
  CurrencyFormatter* formatter = request()->GetOrCreateCurrencyFormatter(
      request()->details()->total->amount->currency,
      request()->details()->total->amount->currencySystem,
      g_browser_process->GetApplicationLocale());
  base::string16 label_value = l10n_util::GetStringFUTF16(
      IDS_PAYMENT_REQUEST_ORDER_SUMMARY_SECTION_TOTAL_FORMAT,
      base::UTF8ToUTF16(request()->details()->total->label),
      base::UTF8ToUTF16(request()->details()->total->amount->currency),
      formatter->Format(request()->details()->total->amount->value));

  return base::MakeUnique<views::Label>(label_value);
}

std::unique_ptr<views::Button>
PaymentSheetViewController::CreatePaymentSheetSummaryRow() {
  std::unique_ptr<views::Button> section = base::MakeUnique<PaymentSheetRow>(
      this,
      l10n_util::GetStringUTF16(IDS_PAYMENT_REQUEST_ORDER_SUMMARY_SECTION_NAME),
      std::unique_ptr<views::View>(nullptr),
      CreateOrderSummarySectionContent());
  section->set_tag(static_cast<int>(
      PaymentSheetViewControllerTags::SHOW_ORDER_SUMMARY_BUTTON));
  return section;
}

}  // namespace payments
