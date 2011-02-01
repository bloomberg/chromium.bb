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
    : TranslateInfoBarBase(delegate) {
  label_ = CreateLabel(delegate->GetMessageInfoBarText());
  AddChildView(label_);

  string16 button_text = delegate->GetMessageInfoBarButtonText();
  if (button_text.empty()) {
    button_ = NULL;
  } else {
    button_ = InfoBarTextButton::Create(this, button_text);
    AddChildView(button_);
  }
}

void TranslateMessageInfoBar::Layout() {
  TranslateInfoBarBase::Layout();

  int x = icon_->bounds().right() + InfoBarView::kIconLabelSpacing;
  gfx::Size label_pref_size = label_->GetPreferredSize();
  int available_width = GetAvailableWidth() - x;
  gfx::Size button_pref_size;
  if (button_) {
    button_pref_size = button_->GetPreferredSize();
    available_width -=
        (button_pref_size.width() + InfoBarView::kButtonInLabelSpacing);
  }
  label_->SetBounds(x, InfoBarView::OffsetY(this, label_pref_size),
                    std::min(label_pref_size.width(), available_width),
                    label_pref_size.height());

  if (button_) {
    button_->SetBounds(label_->bounds().right() +
                          InfoBarView::kButtonInLabelSpacing,
                       InfoBarView::OffsetY(this, button_pref_size),
                       button_pref_size.width(), button_pref_size.height());
  }
}

void TranslateMessageInfoBar::ButtonPressed(views::Button* sender,
                                            const views::Event& event) {
  if (sender == button_) {
    GetDelegate()->MessageInfoBarButtonPressed();
    return;
  }
  TranslateInfoBarBase::ButtonPressed(sender, event);
}
