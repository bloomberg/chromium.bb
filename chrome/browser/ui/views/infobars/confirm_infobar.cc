// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/infobars/confirm_infobar.h"

#include "base/logging.h"
#include "chrome/browser/tab_contents/confirm_infobar_delegate.h"
#include "chrome/browser/ui/views/event_utils.h"
#include "ui/views/controls/button/text_button.h"
#include "views/controls/label.h"
#include "views/controls/link.h"

// ConfirmInfoBarDelegate -----------------------------------------------------

InfoBar* ConfirmInfoBarDelegate::CreateInfoBar(InfoBarTabHelper* owner) {
  return new ConfirmInfoBar(owner, this);
}

// ConfirmInfoBar -------------------------------------------------------------

ConfirmInfoBar::ConfirmInfoBar(InfoBarTabHelper* owner,
                               ConfirmInfoBarDelegate* delegate)
    : InfoBarView(owner, delegate),
      label_(NULL),
      ok_button_(NULL),
      cancel_button_(NULL),
      link_(NULL) {
}

ConfirmInfoBar::~ConfirmInfoBar() {
}

void ConfirmInfoBar::Layout() {
  InfoBarView::Layout();

  int available_width = std::max(0, EndX() - StartX() - ContentMinimumWidth());
  gfx::Size label_size = label_->GetPreferredSize();
  label_->SetBounds(StartX(), OffsetY(label_size),
      std::min(label_size.width(), available_width), label_size.height());
  available_width = std::max(0, available_width - label_size.width());

  int button_x = label_->bounds().right() + kEndOfLabelSpacing;
  if (ok_button_ != NULL) {
    gfx::Size ok_size = ok_button_->GetPreferredSize();
    ok_button_->SetBounds(button_x, OffsetY(ok_size), ok_size.width(),
                          ok_size.height());
    button_x += ok_size.width() + kButtonButtonSpacing;
  }

  if (cancel_button_ != NULL) {
    gfx::Size cancel_size = cancel_button_->GetPreferredSize();
    cancel_button_->SetBounds(button_x, OffsetY(cancel_size),
                              cancel_size.width(), cancel_size.height());
  }

  if (link_ != NULL) {
    gfx::Size link_size = link_->GetPreferredSize();
    int link_width = std::min(link_size.width(), available_width);
    link_->SetBounds(EndX() - link_width, OffsetY(link_size), link_width,
                     link_size.height());
  }
}

void ConfirmInfoBar::ViewHierarchyChanged(bool is_add,
                                          View* parent,
                                          View* child) {
  if (is_add && child == this && (label_ == NULL)) {
    ConfirmInfoBarDelegate* delegate = GetDelegate();
    label_ = CreateLabel(delegate->GetMessageText());
    AddChildView(label_);

    if (delegate->GetButtons() & ConfirmInfoBarDelegate::BUTTON_OK) {
      ok_button_ = CreateTextButton(this,
          delegate->GetButtonLabel(ConfirmInfoBarDelegate::BUTTON_OK),
          delegate->NeedElevation(ConfirmInfoBarDelegate::BUTTON_OK));
      AddChildView(ok_button_);
    }

    if (delegate->GetButtons() & ConfirmInfoBarDelegate::BUTTON_CANCEL) {
      cancel_button_ = CreateTextButton(this,
          delegate->GetButtonLabel(ConfirmInfoBarDelegate::BUTTON_CANCEL),
          delegate->NeedElevation(ConfirmInfoBarDelegate::BUTTON_CANCEL));
      AddChildView(cancel_button_);
    }

    string16 link_text(delegate->GetLinkText());
    if (!link_text.empty()) {
      link_ = CreateLink(link_text, this);
      AddChildView(link_);
    }
  }

  // This must happen after adding all other children so InfoBarView can ensure
  // the close button is the last child.
  InfoBarView::ViewHierarchyChanged(is_add, parent, child);
}

void ConfirmInfoBar::ButtonPressed(views::Button* sender,
                                   const views::Event& event) {
  if (!owned())
    return;  // We're closing; don't call anything, it might access the owner.
  ConfirmInfoBarDelegate* delegate = GetDelegate();
  if ((ok_button_ != NULL) && sender == ok_button_) {
    if (delegate->Accept())
      RemoveSelf();
  } else if ((cancel_button_ != NULL) && (sender == cancel_button_)) {
    if (delegate->Cancel())
      RemoveSelf();
  } else {
    InfoBarView::ButtonPressed(sender, event);
  }
}

int ConfirmInfoBar::ContentMinimumWidth() const {
  int width = (link_ == NULL) ? 0 : kEndOfLabelSpacing;  // Space before link
  int before_cancel_spacing = kEndOfLabelSpacing;
  if (ok_button_ != NULL) {
    width += kEndOfLabelSpacing + ok_button_->GetPreferredSize().width();
    before_cancel_spacing = kButtonButtonSpacing;
  }
  return width + ((cancel_button_ == NULL) ? 0 :
      (before_cancel_spacing + cancel_button_->GetPreferredSize().width()));
}

void ConfirmInfoBar::LinkClicked(views::Link* source, int event_flags) {
  if (!owned())
    return;  // We're closing; don't call anything, it might access the owner.
  DCHECK(link_ != NULL);
  DCHECK_EQ(link_, source);
  if (GetDelegate()->LinkClicked(
      event_utils::DispositionFromEventFlags(event_flags)))
    RemoveSelf();
}

ConfirmInfoBarDelegate* ConfirmInfoBar::GetDelegate() {
  return delegate()->AsConfirmInfoBarDelegate();
}
