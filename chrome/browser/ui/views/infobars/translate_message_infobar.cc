// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/infobars/translate_message_infobar.h"

#include "chrome/browser/translate/translate_infobar_delegate.h"
#include "ui/views/controls/button/text_button.h"
#include "ui/views/controls/label.h"

TranslateMessageInfoBar::TranslateMessageInfoBar(
    InfoBarTabHelper* owner,
    TranslateInfoBarDelegate* delegate)
    : TranslateInfoBarBase(owner, delegate),
      label_(NULL),
      button_(NULL) {
}

TranslateMessageInfoBar::~TranslateMessageInfoBar() {
}

void TranslateMessageInfoBar::Layout() {
  TranslateInfoBarBase::Layout();

  gfx::Size label_size = label_->GetPreferredSize();
  label_->SetBounds(StartX(), OffsetY(label_size),
      std::min(label_size.width(),
               std::max(0, EndX() - StartX() - ContentMinimumWidth())),
      label_size.height());

  if (button_) {
    gfx::Size button_size = button_->GetPreferredSize();
    button_->SetBounds(label_->bounds().right() + kButtonInLabelSpacing,
        OffsetY(button_size), button_size.width(), button_size.height());
  }
}

void TranslateMessageInfoBar::ViewHierarchyChanged(bool is_add,
                                                   View* parent,
                                                   View* child) {
  if (is_add && (child == this) && (label_ == NULL)) {
    TranslateInfoBarDelegate* delegate = GetDelegate();
    label_ = CreateLabel(delegate->GetMessageInfoBarText());
    AddChildView(label_);

    string16 button_text(delegate->GetMessageInfoBarButtonText());
    if (!button_text.empty()) {
      button_ = CreateTextButton(this, button_text, false);
      AddChildView(button_);
    }
  }

  // This must happen after adding all other children so InfoBarView can ensure
  // the close button is the last child.
  TranslateInfoBarBase::ViewHierarchyChanged(is_add, parent, child);
}

void TranslateMessageInfoBar::ButtonPressed(views::Button* sender,
                                            const views::Event& event) {
  if (!owned())
    return;  // We're closing; don't call anything, it might access the owner.
  if (sender == button_)
    GetDelegate()->MessageInfoBarButtonPressed();
  else
    TranslateInfoBarBase::ButtonPressed(sender, event);
}

int TranslateMessageInfoBar::ContentMinimumWidth() const {
  return (button_ != NULL) ?
      (button_->GetPreferredSize().width() + kButtonInLabelSpacing) : 0;
}
