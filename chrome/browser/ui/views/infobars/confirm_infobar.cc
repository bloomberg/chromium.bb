// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/infobars/confirm_infobar.h"

#include "chrome/browser/tab_contents/infobar_delegate.h"
#include "chrome/browser/ui/views/event_utils.h"
#include "views/controls/button/text_button.h"
#include "views/controls/image_view.h"
#include "views/controls/label.h"

// AlertInfoBar, public: -------------------------------------------------------

AlertInfoBar::AlertInfoBar(ConfirmInfoBarDelegate* delegate)
    : InfoBarView(delegate) {
  label_ = CreateLabel(delegate->GetMessageText());
  AddChildView(label_);

  icon_ = new views::ImageView;
  if (delegate->GetIcon())
    icon_->SetImage(delegate->GetIcon());
  AddChildView(icon_);
}

AlertInfoBar::~AlertInfoBar() {
}

// AlertInfoBar, views::View overrides: ----------------------------------------

void AlertInfoBar::Layout() {
  // Layout the close button.
  InfoBarView::Layout();

  // Layout the icon and text.
  gfx::Size icon_size = icon_->GetPreferredSize();
  icon_->SetBounds(kHorizontalPadding, OffsetY(this, icon_size),
                   icon_size.width(), icon_size.height());

  int available_width = std::max(0,
      GetAvailableWidth() - icon_->bounds().right() - kIconLabelSpacing);
  gfx::Size label_size = label_->GetPreferredSize();
  label_->SetBounds(icon_->bounds().right() + kIconLabelSpacing,
      OffsetY(this, label_size), std::min(label_size.width(), available_width),
      label_size.height());
}

// ConfirmInfoBarDelegate -----------------------------------------------------

InfoBar* ConfirmInfoBarDelegate::CreateInfoBar() {
  return new ConfirmInfoBar(this);
}

// ConfirmInfoBar -------------------------------------------------------------

ConfirmInfoBar::ConfirmInfoBar(ConfirmInfoBarDelegate* delegate)
    : AlertInfoBar(delegate),
      initialized_(false) {
  ok_button_ = CreateTextButton(this,
      (delegate->GetButtons() & ConfirmInfoBarDelegate::BUTTON_OK) ?
          delegate->GetButtonLabel(ConfirmInfoBarDelegate::BUTTON_OK) :
          string16(),
      delegate->NeedElevation(ConfirmInfoBarDelegate::BUTTON_OK));
  cancel_button_ = CreateTextButton(this,
      (delegate->GetButtons() & ConfirmInfoBarDelegate::BUTTON_CANCEL) ?
          delegate->GetButtonLabel(ConfirmInfoBarDelegate::BUTTON_CANCEL) :
          string16(),
      delegate->NeedElevation(ConfirmInfoBarDelegate::BUTTON_CANCEL));

  link_ = CreateLink(delegate->GetLinkText(), this, background()->get_color());
}

ConfirmInfoBar::~ConfirmInfoBar() {
  if (!initialized_) {
    delete ok_button_;
    delete cancel_button_;
    delete link_;
  }
}

void ConfirmInfoBar::Layout() {
  InfoBarView::Layout();

  int available_width = AlertInfoBar::GetAvailableWidth();
  int ok_button_width = 0;
  int cancel_button_width = 0;
  gfx::Size ok_size = ok_button_->GetPreferredSize();
  gfx::Size cancel_size = cancel_button_->GetPreferredSize();

  if (GetDelegate()->GetButtons() & ConfirmInfoBarDelegate::BUTTON_OK) {
    ok_button_width = ok_size.width();
  } else {
    ok_button_->SetVisible(false);
  }
  if (GetDelegate()->GetButtons() & ConfirmInfoBarDelegate::BUTTON_CANCEL) {
    cancel_button_width = cancel_size.width();
  } else {
    cancel_button_->SetVisible(false);
  }

  cancel_button_->SetBounds(available_width - cancel_button_width,
      OffsetY(this, cancel_size), cancel_size.width(), cancel_size.height());
  int spacing = cancel_button_width > 0 ? kButtonButtonSpacing : 0;
  ok_button_->SetBounds(cancel_button_->x() - spacing - ok_button_width,
      OffsetY(this, ok_size), ok_size.width(), ok_size.height());

  // Layout the icon and label.
  AlertInfoBar::Layout();

  // Now append the link to the label's right edge.
  link_->SetVisible(!link_->GetText().empty());
  gfx::Size link_size = link_->GetPreferredSize();
  int link_x = label()->bounds().right() + kEndOfLabelSpacing;
  int link_w = std::min(GetAvailableWidth() - link_x, link_size.width());
  link_->SetBounds(link_x, OffsetY(this, link_size), link_w,
                   link_size.height());
}

void ConfirmInfoBar::ViewHierarchyChanged(bool is_add,
                                          View* parent,
                                          View* child) {
  if (is_add && child == this && !initialized_) {
    AddChildView(ok_button_);
    AddChildView(cancel_button_);
    AddChildView(link_);
    initialized_ = true;
  }

  // This must happen after adding all other children so InfoBarView can ensure
  // the close button is the last child.
  InfoBarView::ViewHierarchyChanged(is_add, parent, child);
}

int ConfirmInfoBar::GetAvailableWidth() const {
  return ok_button_->x() - kEndOfLabelSpacing;
}

void ConfirmInfoBar::ButtonPressed(views::Button* sender,
                                   const views::Event& event) {
  ConfirmInfoBarDelegate* delegate = GetDelegate();
  if (sender == ok_button_) {
    if (delegate->Accept())
      RemoveInfoBar();
  } else if (sender == cancel_button_) {
    if (delegate->Cancel())
      RemoveInfoBar();
  } else {
    InfoBarView::ButtonPressed(sender, event);
  }
}

void ConfirmInfoBar::LinkActivated(views::Link* source, int event_flags) {
  DCHECK_EQ(link_, source);
  if (GetDelegate()->LinkClicked(
      event_utils::DispositionFromEventFlags(event_flags)))
    RemoveInfoBar();
}

ConfirmInfoBarDelegate* ConfirmInfoBar::GetDelegate() {
  return delegate()->AsConfirmInfoBarDelegate();
}
