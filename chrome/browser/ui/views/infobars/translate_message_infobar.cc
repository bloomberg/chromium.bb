// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/infobars/translate_message_infobar.h"

#include "chrome/browser/translate/translate_infobar_delegate.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/label.h"

TranslateMessageInfoBar::TranslateMessageInfoBar(
    scoped_ptr<TranslateInfoBarDelegate> delegate)
    : TranslateInfoBarBase(delegate.Pass()),
      label_(NULL),
      button_(NULL) {
}

TranslateMessageInfoBar::~TranslateMessageInfoBar() {
}

void TranslateMessageInfoBar::Layout() {
  TranslateInfoBarBase::Layout();

  int x = StartX();
  const int width =
      std::min(label_->width(), std::max(0, EndX() - x - NonLabelWidth()));
  label_->SetBounds(x, OffsetY(label_), width, label_->height());
  if (!label_->text().empty())
    x = label_->bounds().right() + kEndOfLabelSpacing;

  if (button_)
    button_->SetPosition(gfx::Point(x, OffsetY(button_)));
}

void TranslateMessageInfoBar::ViewHierarchyChanged(
    const ViewHierarchyChangedDetails& details) {
  if (details.is_add && (details.child == this) && (label_ == NULL)) {
    TranslateInfoBarDelegate* delegate = GetDelegate();
    label_ = CreateLabel(delegate->GetMessageInfoBarText());
    AddChildView(label_);

    base::string16 button_text(delegate->GetMessageInfoBarButtonText());
    if (!button_text.empty()) {
      button_ = CreateLabelButton(this, button_text, false);
      AddChildView(button_);
    }
  }

  // This must happen after adding all other children so InfoBarView can ensure
  // the close button is the last child.
  TranslateInfoBarBase::ViewHierarchyChanged(details);
}

void TranslateMessageInfoBar::ButtonPressed(views::Button* sender,
                                            const ui::Event& event) {
  if (!owner())
    return;  // We're closing; don't call anything, it might access the owner.
  if (sender == button_)
    GetDelegate()->MessageInfoBarButtonPressed();
  else
    TranslateInfoBarBase::ButtonPressed(sender, event);
}

int TranslateMessageInfoBar::ContentMinimumWidth() {
  return label_->GetMinimumSize().width() + NonLabelWidth();
}

int TranslateMessageInfoBar::NonLabelWidth() const {
  if (!button_)
    return 0;
  return button_->width() + (label_->text().empty() ? 0 : kEndOfLabelSpacing);
}
