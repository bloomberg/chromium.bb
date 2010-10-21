// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/username_view.h"

#include "chrome/browser/chromeos/login/helper.h"
#include "views/background.h"
#include "views/painter.h"
#include "views/controls/label.h"

namespace {
// Username label background color.
const SkColor kLabelBackgoundColor = 0x55000000;
}  // namespace

namespace chromeos {

UsernameView::UsernameView(const std::wstring& username) {
  label_ = new views::Label(username);
  label_->SetColor(login::kTextColor);
  label_->set_background(
      views::Background::CreateSolidBackground(kLabelBackgoundColor));
  label_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  AddChildView(label_);

  gradient_ = new views::View;
  gradient_->SetVisible(false);
  views::Background* gradient_background =
      views::Background::CreateBackgroundPainter(true,
          views::Painter::CreateHorizontalGradient(
              kLabelBackgoundColor, 0x00000000));
  gradient_->set_background(gradient_background);
  AddChildView(gradient_);
}

void UsernameView::SetFont(const gfx::Font& font) {
  label_->SetFont(font);
}

// views::View
void UsernameView::Layout() {
  int text_width = std::min(label_->GetPreferredSize().width(), width());
  label_->SetBounds(0, 0, text_width, height());
  int gradient_width = std::min(width() - text_width, height());
  if (gradient_width > 0)  {
    gradient_->SetVisible(true);
    gradient_->SetBounds(0 + text_width, 0, gradient_width, height());
  } else {
    // No place for the gradient part.
    gradient_->SetVisible(false);
  }
}

gfx::Size UsernameView::GetPreferredSize() {
  return label_->GetPreferredSize();
}

}  // namespace chromeos
