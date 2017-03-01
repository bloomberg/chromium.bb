// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/payments/payment_request_sheet_controller.h"

#include "chrome/browser/ui/views/payments/payment_request_dialog_view.h"
#include "chrome/browser/ui/views/payments/payment_request_views_util.h"
#include "components/payments/content/payment_request.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/grid_layout.h"

namespace payments {

PaymentRequestSheetController::PaymentRequestSheetController(
    PaymentRequest* request, PaymentRequestDialogView* dialog)
  : request_(request), dialog_(dialog) {
}

std::unique_ptr<views::Button>
PaymentRequestSheetController::CreatePrimaryButton() {
  return nullptr;
}

std::unique_ptr<views::View>
PaymentRequestSheetController::CreateExtraFooterView() {
  return nullptr;
}

void PaymentRequestSheetController::ButtonPressed(
    views::Button* sender, const ui::Event& event) {
  switch (static_cast<PaymentRequestCommonTags>(sender->tag())) {
    case PaymentRequestCommonTags::CLOSE_BUTTON_TAG:
      dialog()->CloseDialog();
      break;
    case PaymentRequestCommonTags::BACK_BUTTON_TAG:
      dialog()->GoBack();
      break;
    case PaymentRequestCommonTags::PAY_BUTTON_TAG:
      request()->Pay();
      break;
    case PaymentRequestCommonTags::PAYMENT_REQUEST_COMMON_TAG_MAX:
      NOTREACHED();
      break;
  }
}

std::unique_ptr<views::View> PaymentRequestSheetController::CreatePaymentView(
    std::unique_ptr<views::View> header_view,
    std::unique_ptr<views::View> content_view) {
  std::unique_ptr<views::View> view = base::MakeUnique<views::View>();
  view->set_background(views::Background::CreateSolidBackground(SK_ColorWHITE));

  // Paint the sheets to layers, otherwise the MD buttons (which do paint to a
  // layer) won't do proper clipping.
  view->SetPaintToLayer();

  views::GridLayout* layout = new views::GridLayout(view.get());
  view->SetLayoutManager(layout);

  // Note: each view is responsible for its own padding (insets).
  views::ColumnSet* columns = layout->AddColumnSet(0);
  columns->AddColumn(views::GridLayout::FILL, views::GridLayout::CENTER,
                     1, views::GridLayout::USE_PREF, 0, 0);

  layout->StartRow(0, 0);
  // |header_view| will be deleted when |view| is.
  layout->AddView(header_view.release());

  layout->StartRow(0, 0);
  // |content_view| will be deleted when |view| is.
  layout->AddView(content_view.release());

  layout->AddPaddingRow(1, 0);
  layout->StartRow(0, 0);
  layout->AddView(CreateFooterView().release());

  return view;
}

std::unique_ptr<views::View> PaymentRequestSheetController::CreateFooterView() {
  std::unique_ptr<views::View> container = base::MakeUnique<views::View>();

  views::GridLayout* layout = new views::GridLayout(container.get());
  container->SetLayoutManager(layout);

  views::ColumnSet* columns = layout->AddColumnSet(0);
  columns->AddColumn(views::GridLayout::LEADING, views::GridLayout::CENTER,
                     0, views::GridLayout::USE_PREF, 0, 0);
  columns->AddPaddingColumn(1, 0);
  columns->AddColumn(views::GridLayout::TRAILING, views::GridLayout::CENTER,
                     0, views::GridLayout::USE_PREF, 0, 0);

  // The horizontal distance between the right/left edges of the dialog and the
  // elements.
  constexpr int kFooterHorizontalInset = 16;
  // The vertical distance between footer elements and the top/bottom border
  // (the bottom border is the edge of the dialog).
  constexpr int kFooterVerticalInset = 16;
  layout->SetInsets(kFooterVerticalInset, kFooterHorizontalInset,
                    kFooterVerticalInset, kFooterHorizontalInset);
  layout->StartRow(0, 0);
  std::unique_ptr<views::View> extra_view = CreateExtraFooterView();
  if (extra_view)
    layout->AddView(extra_view.release());
  else
    layout->SkipColumns(1);

  std::unique_ptr<views::View> trailing_buttons_container =
      base::MakeUnique<views::View>();

  trailing_buttons_container->SetLayoutManager(new views::BoxLayout(
      views::BoxLayout::kHorizontal, kPaymentRequestRowHorizontalInsets,
      kPaymentRequestRowVerticalInsets, kPaymentRequestButtonSpacing));

  std::unique_ptr<views::Button> primary_button = CreatePrimaryButton();
  if (primary_button)
    trailing_buttons_container->AddChildView(primary_button.release());

  views::LabelButton* button = views::MdTextButton::CreateSecondaryUiButton(
      this, l10n_util::GetStringUTF16(IDS_CANCEL));
  button->set_tag(static_cast<int>(PaymentRequestCommonTags::CLOSE_BUTTON_TAG));
  trailing_buttons_container->AddChildView(button);

  layout->AddView(trailing_buttons_container.release());

  return container;
}

}  // namespace payments
