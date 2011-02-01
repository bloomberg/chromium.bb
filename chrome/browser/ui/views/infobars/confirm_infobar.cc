// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/infobars/confirm_infobar.h"

#include "base/utf_string_conversions.h"
#include "chrome/browser/tab_contents/infobar_delegate.h"
#include "chrome/browser/ui/views/event_utils.h"
#include "chrome/browser/ui/views/infobars/infobar_text_button.h"
#include "ui/base/resource/resource_bundle.h"
#include "views/controls/image_view.h"
#include "views/controls/label.h"

// AlertInfoBar, public: -------------------------------------------------------

AlertInfoBar::AlertInfoBar(ConfirmInfoBarDelegate* delegate)
    : InfoBarView(delegate) {
  label_ = new views::Label(
      UTF16ToWideHack(delegate->GetMessageText()),
      ResourceBundle::GetSharedInstance().GetFont(ResourceBundle::MediumFont));
  label_->SetColor(SK_ColorBLACK);
  label_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
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
  gfx::Size icon_ps = icon_->GetPreferredSize();
  icon_->SetBounds(kHorizontalPadding, OffsetY(this, icon_ps), icon_ps.width(),
                   icon_ps.height());

  gfx::Size text_ps = label_->GetPreferredSize();
  int text_width = std::min(
      text_ps.width(),
      GetAvailableWidth() - icon_->bounds().right() - kIconLabelSpacing);
  label_->SetBounds(icon_->bounds().right() + kIconLabelSpacing,
                    OffsetY(this, text_ps), text_width, text_ps.height());
}

// ConfirmInfoBarDelegate, InfoBarDelegate overrides: --------------------------

InfoBar* ConfirmInfoBarDelegate::CreateInfoBar() {
  return new ConfirmInfoBar(this);
}

// ConfirmInfoBar, public: -----------------------------------------------------

ConfirmInfoBar::ConfirmInfoBar(ConfirmInfoBarDelegate* delegate)
    : AlertInfoBar(delegate),
      ok_button_(NULL),
      cancel_button_(NULL),
      link_(NULL),
      initialized_(false) {
  ok_button_ = InfoBarTextButton::Create(this,
      (delegate->GetButtons() & ConfirmInfoBarDelegate::BUTTON_OK) ?
          delegate->GetButtonLabel(ConfirmInfoBarDelegate::BUTTON_OK) :
          string16());
  ok_button_->SetAccessibleName(WideToUTF16Hack(ok_button_->text()));
  cancel_button_ = InfoBarTextButton::Create(this,
      (delegate->GetButtons() & ConfirmInfoBarDelegate::BUTTON_CANCEL) ?
          delegate->GetButtonLabel(ConfirmInfoBarDelegate::BUTTON_CANCEL) :
          string16());
  cancel_button_->SetAccessibleName(WideToUTF16Hack(cancel_button_->text()));

  // Set up the link.
  link_ = new views::Link;
  link_->SetText(UTF16ToWideHack(delegate->GetLinkText()));
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  link_->SetFont(rb.GetFont(ResourceBundle::MediumFont));
  link_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  link_->SetController(this);
  link_->MakeReadableOverBackgroundColor(background()->get_color());
}

ConfirmInfoBar::~ConfirmInfoBar() {
  if (!initialized_) {
    delete ok_button_;
    delete cancel_button_;
    delete link_;
  }
}

// ConfirmInfoBar, views::LinkController implementation: -----------------------

void ConfirmInfoBar::LinkActivated(views::Link* source, int event_flags) {
  DCHECK(source == link_);
  DCHECK(link_->IsVisible());
  DCHECK(!link_->GetText().empty());
  if (GetDelegate()->LinkClicked(
          event_utils::DispositionFromEventFlags(event_flags))) {
    RemoveInfoBar();
  }
}

// ConfirmInfoBar, views::View overrides: --------------------------------------

void ConfirmInfoBar::Layout() {
  // First layout right aligned items (from right to left) in order to determine
  // the space avalable, then layout the left aligned items.

  // Layout the close button.
  InfoBarView::Layout();

  // Layout the cancel and OK buttons.
  int available_width = AlertInfoBar::GetAvailableWidth();
  int ok_button_width = 0;
  int cancel_button_width = 0;
  gfx::Size ok_ps = ok_button_->GetPreferredSize();
  gfx::Size cancel_ps = cancel_button_->GetPreferredSize();

  if (GetDelegate()->GetButtons() & ConfirmInfoBarDelegate::BUTTON_OK) {
    ok_button_width = ok_ps.width();
  } else {
    ok_button_->SetVisible(false);
  }
  if (GetDelegate()->GetButtons() & ConfirmInfoBarDelegate::BUTTON_CANCEL) {
    cancel_button_width = cancel_ps.width();
  } else {
    cancel_button_->SetVisible(false);
  }

  cancel_button_->SetBounds(available_width - cancel_button_width,
                            OffsetY(this, cancel_ps), cancel_ps.width(),
                            cancel_ps.height());
  int spacing = cancel_button_width > 0 ? kButtonButtonSpacing : 0;
  ok_button_->SetBounds(cancel_button_->x() - spacing - ok_button_width,
                        OffsetY(this, ok_ps), ok_ps.width(), ok_ps.height());

  // Layout the icon and label.
  AlertInfoBar::Layout();

  // Now append the link to the label's right edge.
  link_->SetVisible(!link_->GetText().empty());
  gfx::Size link_ps = link_->GetPreferredSize();
  int link_x = label()->bounds().right() + kEndOfLabelSpacing;
  int link_w = std::min(GetAvailableWidth() - link_x, link_ps.width());
  link_->SetBounds(link_x, OffsetY(this, link_ps), link_w, link_ps.height());
}

void ConfirmInfoBar::ViewHierarchyChanged(bool is_add,
                                          views::View* parent,
                                          views::View* child) {
  if (is_add && child == this && !initialized_) {
    Init();
    initialized_ = true;
  }
  InfoBarView::ViewHierarchyChanged(is_add, parent, child);
}

// ConfirmInfoBar, views::ButtonListener implementation: ---------------

void ConfirmInfoBar::ButtonPressed(
    views::Button* sender, const views::Event& event) {
  InfoBarView::ButtonPressed(sender, event);
  if (sender == ok_button_) {
    if (GetDelegate()->Accept())
      RemoveInfoBar();
  } else if (sender == cancel_button_) {
    if (GetDelegate()->Cancel())
      RemoveInfoBar();
  }
}

// ConfirmInfoBar, InfoBar overrides: ------------------------------------------

int ConfirmInfoBar::GetAvailableWidth() const {
  return ok_button_->x() - kEndOfLabelSpacing;
}

// ConfirmInfoBar, private: ----------------------------------------------------

ConfirmInfoBarDelegate* ConfirmInfoBar::GetDelegate() {
  return delegate()->AsConfirmInfoBarDelegate();
}

void ConfirmInfoBar::Init() {
  AddChildView(ok_button_);
  AddChildView(cancel_button_);
  AddChildView(link_);
}
