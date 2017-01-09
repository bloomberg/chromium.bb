// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/payments/payment_request_views_util.h"

#include "base/memory/ptr_util.h"
#include "chrome/browser/ui/views/payments/payment_request_sheet_controller.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/vector_icons_public.h"
#include "ui/views/background.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/vector_icon_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/view.h"

namespace payments {

std::unique_ptr<views::View> CreateSheetHeaderView(
    bool show_back_arrow,
    const base::string16& title,
    views::VectorIconButtonDelegate* delegate) {
  std::unique_ptr<views::View> container = base::MakeUnique<views::View>();
  views::GridLayout* layout = new views::GridLayout(container.get());
  container->SetLayoutManager(layout);

  views::ColumnSet* columns = layout->AddColumnSet(0);
  // A column for the optional back arrow.
  columns->AddColumn(views::GridLayout::LEADING, views::GridLayout::CENTER,
                     0, views::GridLayout::USE_PREF, 0, 0);
  // A column for the title.
  columns->AddColumn(views::GridLayout::FILL, views::GridLayout::CENTER,
                     1, views::GridLayout::USE_PREF, 0, 0);
  // A column for the close button.
  columns->AddColumn(views::GridLayout::TRAILING, views::GridLayout::CENTER,
                     0, views::GridLayout::USE_PREF, 0, 0);

  layout->StartRow(0, 0);
  if (!show_back_arrow) {
    layout->SkipColumns(1);
  } else {
    views::VectorIconButton* back_arrow = new views::VectorIconButton(delegate);
    back_arrow->SetIcon(gfx::VectorIconId::NAVIGATE_BACK);
    back_arrow->SetSize(back_arrow->GetPreferredSize());
    back_arrow->set_tag(static_cast<int>(
        PaymentRequestCommonTags::BACK_BUTTON_TAG));
    layout->AddView(back_arrow);
  }

  views::Label* title_label = new views::Label(title);
  title_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  layout->AddView(title_label);
  views::Button* close_button =
      views::BubbleFrameView::CreateCloseButton(delegate);
  close_button->set_tag(static_cast<int>(
      PaymentRequestCommonTags::CLOSE_BUTTON_TAG));
  layout->AddView(close_button);

  return container;
}


std::unique_ptr<views::View> CreatePaymentView(
    std::unique_ptr<views::View> header_view,
    std::unique_ptr<views::View> content_view) {
  std::unique_ptr<views::View> view = base::MakeUnique<views::View>();
  view->set_background(views::Background::CreateSolidBackground(SK_ColorWHITE));

  // Paint the sheets to layers, otherwise the MD buttons (which do paint to a
  // layer) won't do proper clipping.
  view->SetPaintToLayer(true);

  views::GridLayout* layout = new views::GridLayout(view.get());
  view->SetLayoutManager(layout);

  constexpr int kTopInsetSize = 9;
  constexpr int kBottomInsetSize = 18;
  constexpr int kSideInsetSize = 14;
  layout->SetInsets(
      kTopInsetSize, kSideInsetSize, kBottomInsetSize, kSideInsetSize);
  views::ColumnSet* columns = layout->AddColumnSet(0);
  columns->AddColumn(views::GridLayout::FILL, views::GridLayout::CENTER,
                     1, views::GridLayout::USE_PREF, 0, 0);

  layout->StartRow(0, 0);
  // |header_view| will be deleted when |view| is.
  layout->AddView(header_view.release());

  layout->StartRow(0, 0);
  // |content_view| will be deleted when |view| is.
  layout->AddView(content_view.release());

  return view;
}

}  // namespace payments
