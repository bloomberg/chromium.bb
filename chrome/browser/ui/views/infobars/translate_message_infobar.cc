// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/infobars/translate_message_infobar.h"

#include "chrome/browser/translate/translate_infobar_delegate.h"
#include "chrome/browser/ui/views/infobars/infobar_text_button.h"
#include "views/controls/image_view.h"
#include "views/controls/label.h"

TranslateMessageInfoBar::TranslateMessageInfoBar(
    TranslateInfoBarDelegate* delegate)
    : TranslateInfoBarBase(delegate),
      button_(NULL) {
  label_ = CreateLabel(delegate->GetMessageInfoBarText());
  AddChildView(label_);

  string16 button_text = delegate->GetMessageInfoBarButtonText();
  if (!button_text.empty()) {
    button_ = InfoBarTextButton::Create(this, button_text);
    AddChildView(button_);
  }
}

TranslateMessageInfoBar::~TranslateMessageInfoBar() {
}

void TranslateMessageInfoBar::Layout() {
  TranslateInfoBarBase::Layout();

  int x = icon_->bounds().right() + kIconLabelSpacing;
  gfx::Size label_size = label_->GetPreferredSize();
  int available_width = GetAvailableWidth() - x;
  gfx::Size button_size;
  if (button_) {
    button_size = button_->GetPreferredSize();
    available_width -= (button_size.width() + kButtonInLabelSpacing);
  }
  label_->SetBounds(x, OffsetY(this, label_size),
      std::min(label_size.width(), available_width), label_size.height());

  if (button_) {
    button_->SetBounds(label_->bounds().right() + kButtonInLabelSpacing,
        OffsetY(this, button_size), button_size.width(), button_size.height());
  }
}

void TranslateMessageInfoBar::ButtonPressed(views::Button* sender,
                                            const views::Event& event) {
  if (sender == button_)
    GetDelegate()->MessageInfoBarButtonPressed();
  else
    TranslateInfoBarBase::ButtonPressed(sender, event);
}
