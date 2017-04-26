// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/payments/payment_request_sheet_controller.h"

#include "chrome/browser/ui/views/payments/payment_request_dialog_view.h"
#include "chrome/browser/ui/views/payments/payment_request_dialog_view_ids.h"
#include "chrome/browser/ui/views/payments/payment_request_views_util.h"
#include "components/payments/content/payment_request.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/grid_layout.h"

namespace payments {

PaymentRequestSheetController::PaymentRequestSheetController(
    PaymentRequestSpec* spec,
    PaymentRequestState* state,
    PaymentRequestDialogView* dialog)
    : spec_(spec), state_(state), dialog_(dialog) {}

PaymentRequestSheetController::~PaymentRequestSheetController() {}

std::unique_ptr<views::View> PaymentRequestSheetController::CreateView() {
  std::unique_ptr<views::View> view = CreatePaymentView();
  UpdateContentView();
  return view;
}

void PaymentRequestSheetController::UpdateContentView() {
  content_view_->RemoveAllChildViews(true);
  FillContentView(content_view_);
  content_view_->Layout();
  pane_->SizeToPreferredSize();
  // Now that the content and its surrounding pane are updated, force a Layout
  // on the ScrollView so that it updates its scroll bars now.
  scroll_->Layout();
}

std::unique_ptr<views::Button>
PaymentRequestSheetController::CreatePrimaryButton() {
  return nullptr;
}

base::string16 PaymentRequestSheetController::GetSecondaryButtonLabel() {
  return l10n_util::GetStringUTF16(IDS_CANCEL);
}

bool PaymentRequestSheetController::ShouldShowHeaderBackArrow() {
  return true;
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
      dialog()->Pay();
      break;
    case PaymentRequestCommonTags::PAYMENT_REQUEST_COMMON_TAG_MAX:
      NOTREACHED();
      break;
  }
}

std::unique_ptr<views::View>
PaymentRequestSheetController::CreatePaymentView() {
  std::unique_ptr<views::View> view = base::MakeUnique<views::View>();
  view->set_background(views::Background::CreateSolidBackground(SK_ColorWHITE));

  // Paint the sheets to layers, otherwise the MD buttons (which do paint to a
  // layer) won't do proper clipping.
  view->SetPaintToLayer();

  views::GridLayout* layout = new views::GridLayout(view.get());
  view->SetLayoutManager(layout);

  // Note: each view is responsible for its own padding (insets).
  views::ColumnSet* columns = layout->AddColumnSet(0);
  columns->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL, 1,
                     views::GridLayout::USE_PREF, 0, 0);

  layout->StartRow(0, 0);
  // |header_view| will be deleted when |view| is.
  layout->AddView(
      CreateSheetHeaderView(ShouldShowHeaderBackArrow(), GetSheetTitle(), this)
          .release());

  layout->StartRow(1, 0);
  // |content_view| will go into a views::ScrollView so it needs to be sized now
  // otherwise it'll be sized to the ScrollView's viewport height, preventing
  // the scroll bar from ever being shown.
  pane_ = new views::View;
  views::GridLayout* pane_layout = new views::GridLayout(pane_);
  views::ColumnSet* pane_columns = pane_layout->AddColumnSet(0);
  pane_columns->AddColumn(
      views::GridLayout::Alignment::FILL, views::GridLayout::Alignment::LEADING,
      0, views::GridLayout::SizeType::FIXED, kDialogWidth, kDialogWidth);
  pane_->SetLayoutManager(pane_layout);
  pane_layout->StartRow(0, 0);
  // This is owned by its parent. It's the container passed to FillContentView.
  content_view_ = new views::View;
  pane_layout->AddView(content_view_);
  pane_->SizeToPreferredSize();

  scroll_ = base::MakeUnique<views::ScrollView>();
  scroll_->set_owned_by_client();
  scroll_->EnableViewPortLayer();
  scroll_->set_hide_horizontal_scrollbar(true);
  scroll_->SetContents(pane_);
  layout->AddView(scroll_.get());

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
      views::BoxLayout::kHorizontal, 0, 0, kPaymentRequestButtonSpacing));

  std::unique_ptr<views::Button> primary_button = CreatePrimaryButton();
  if (primary_button)
    trailing_buttons_container->AddChildView(primary_button.release());

  views::LabelButton* button = views::MdTextButton::CreateSecondaryUiButton(
      this, GetSecondaryButtonLabel());
  button->set_tag(static_cast<int>(PaymentRequestCommonTags::CLOSE_BUTTON_TAG));
  button->set_id(static_cast<int>(DialogViewID::CANCEL_BUTTON));
  trailing_buttons_container->AddChildView(button);

  layout->AddView(trailing_buttons_container.release());

  return container;
}

}  // namespace payments
