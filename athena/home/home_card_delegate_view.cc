// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/home/home_card_delegate_view.h"

#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/widget/widget.h"

namespace athena {

HomeCardDelegateView::HomeCardDelegateView() {
  const SkColor kHomeTopColor = 0xDDFFFFFF;
  const SkColor kHomeBottomColor = 0xAAEEEEEE;
  const SkColor kHomeBorderColor = 0xDDFFFFFF;
  set_background(views::Background::CreateVerticalGradientBackground(
      kHomeTopColor, kHomeBottomColor));
  scoped_ptr<views::Border> border(
      views::Border::CreateSolidBorder(5, kHomeBorderColor));
  SetBorder(border.Pass());
}

HomeCardDelegateView::~HomeCardDelegateView() {
}

void HomeCardDelegateView::OnMouseEvent(ui::MouseEvent* event) {
  // Temp code to test animation.
  if (event->type() == ui::ET_MOUSE_PRESSED) {
    views::Widget* widget = GetWidget();
    if (widget->IsVisible())
      widget->Hide();
  }
}

void HomeCardDelegateView::DeleteDelegate() {
  delete this;
}

views::Widget* HomeCardDelegateView::GetWidget() {
  return views::View::GetWidget();
}

const views::Widget* HomeCardDelegateView::GetWidget() const {
  return views::View::GetWidget();
}

views::View* HomeCardDelegateView::GetContentsView() {
  return this;
}

}  // namespace athena
