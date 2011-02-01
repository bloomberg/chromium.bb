// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/infobars/link_infobar.h"

#include "base/utf_string_conversions.h"
#include "chrome/browser/tab_contents/infobar_delegate.h"
#include "chrome/browser/ui/views/event_utils.h"
#include "ui/base/resource/resource_bundle.h"
#include "views/controls/image_view.h"

// LinkInfoBarDelegate, InfoBarDelegate overrides: -----------------------------

InfoBar* LinkInfoBarDelegate::CreateInfoBar() {
  return new LinkInfoBar(this);
}

// LinkInfoBar, public: --------------------------------------------------------

LinkInfoBar::LinkInfoBar(LinkInfoBarDelegate* delegate)
    : InfoBarView(delegate),
      icon_(new views::ImageView),
      label_1_(new views::Label),
      label_2_(new views::Label),
      link_(new views::Link) {
  // Set up the icon.
  if (delegate->GetIcon())
    icon_->SetImage(delegate->GetIcon());
  AddChildView(icon_);

  // Set up the labels.
  size_t offset;
  string16 message_text = delegate->GetMessageTextWithOffset(&offset);
  if (offset != string16::npos) {
    label_1_->SetText(UTF16ToWideHack(message_text.substr(0, offset)));
    label_2_->SetText(UTF16ToWideHack(message_text.substr(offset)));
  } else {
    label_1_->SetText(UTF16ToWideHack(message_text));
  }
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  label_1_->SetFont(rb.GetFont(ResourceBundle::MediumFont));
  label_2_->SetFont(rb.GetFont(ResourceBundle::MediumFont));
  label_1_->SetColor(SK_ColorBLACK);
  label_2_->SetColor(SK_ColorBLACK);
  label_1_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  label_2_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  AddChildView(label_1_);
  AddChildView(label_2_);

  // Set up the link.
  link_->SetText(UTF16ToWideHack(delegate->GetLinkText()));
  link_->SetFont(rb.GetFont(ResourceBundle::MediumFont));
  link_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  link_->SetController(this);
  link_->MakeReadableOverBackgroundColor(background()->get_color());
  AddChildView(link_);
}

LinkInfoBar::~LinkInfoBar() {
}

// LinkInfoBar, views::LinkController implementation: --------------------------

void LinkInfoBar::LinkActivated(views::Link* source, int event_flags) {
  DCHECK(source == link_);
  if (GetDelegate()->LinkClicked(
          event_utils::DispositionFromEventFlags(event_flags))) {
    RemoveInfoBar();
  }
}

// LinkInfoBar, views::View overrides: -----------------------------------------

void LinkInfoBar::Layout() {
  // Layout the close button.
  InfoBarView::Layout();

  // Layout the icon.
  gfx::Size icon_ps = icon_->GetPreferredSize();
  icon_->SetBounds(kHorizontalPadding, OffsetY(this, icon_ps), icon_ps.width(),
                   icon_ps.height());

  int label_1_x = icon_->bounds().right() + kIconLabelSpacing;

  // Figure out the amount of space available to the rest of the content now
  // that the close button and the icon have been positioned.
  int available_width = GetAvailableWidth() - label_1_x;

  // Layout the left label.
  gfx::Size label_1_ps = label_1_->GetPreferredSize();
  label_1_->SetBounds(label_1_x, OffsetY(this, label_1_ps), label_1_ps.width(),
                      label_1_ps.height());

  // Layout the link.
  gfx::Size link_ps = link_->GetPreferredSize();
  bool has_second_label = !label_2_->GetText().empty();
  if (has_second_label) {
    // Embed the link in the text string between the two labels.
    link_->SetBounds(label_1_->bounds().right(),
                     OffsetY(this, link_ps), link_ps.width(), link_ps.height());
  } else {
    // Right-align the link toward the edge of the InfoBar.
    link_->SetBounds(label_1_x + available_width - link_ps.width(),
                     OffsetY(this, link_ps), link_ps.width(), link_ps.height());
  }

  // Layout the right label (we do this regardless of whether or not it has
  // text).
  gfx::Size label_2_ps = label_2_->GetPreferredSize();
  label_2_->SetBounds(link_->bounds().right(),
                      OffsetY(this, label_2_ps), label_2_ps.width(),
                      label_2_ps.height());
}

// LinkInfoBar, private: -------------------------------------------------------

LinkInfoBarDelegate* LinkInfoBar::GetDelegate() {
  return delegate()->AsLinkInfoBarDelegate();
}
