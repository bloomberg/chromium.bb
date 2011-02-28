// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/infobars/link_infobar.h"

#include "chrome/browser/tab_contents/infobar_delegate.h"
#include "chrome/browser/ui/views/event_utils.h"

// LinkInfoBarDelegate --------------------------------------------------------

InfoBar* LinkInfoBarDelegate::CreateInfoBar() {
  return new LinkInfoBar(this);
}

// LinkInfoBar ----------------------------------------------------------------

LinkInfoBar::LinkInfoBar(LinkInfoBarDelegate* delegate)
    : InfoBarView(delegate) {
  size_t offset;
  string16 message_text = delegate->GetMessageTextWithOffset(&offset);
  DCHECK_NE(string16::npos, offset);
  label_1_ = CreateLabel(message_text.substr(0, offset));
  AddChildView(label_1_);

  link_ = CreateLink(delegate->GetLinkText(), this,
                     background()->get_color());
  AddChildView(link_);

  label_2_ = CreateLabel(message_text.substr(offset));
  AddChildView(label_2_);
}

LinkInfoBar::~LinkInfoBar() {
}

void LinkInfoBar::Layout() {
  InfoBarView::Layout();

  gfx::Size label_1_size = label_1_->GetPreferredSize();
  label_1_->SetBounds(StartX(), OffsetY(this, label_1_size),
                      label_1_size.width(), label_1_size.height());

  gfx::Size link_size = link_->GetPreferredSize();
  link_->SetBounds(label_1_->bounds().right(), OffsetY(this, link_size),
                   link_size.width(), link_size.height());

  gfx::Size label_2_size = label_2_->GetPreferredSize();
  label_2_->SetBounds(link_->bounds().right(), OffsetY(this, label_2_size),
                      label_2_size.width(), label_2_size.height());
}

void LinkInfoBar::LinkActivated(views::Link* source, int event_flags) {
  DCHECK_EQ(link_, source);
  if (GetDelegate()->LinkClicked(
      event_utils::DispositionFromEventFlags(event_flags)))
    RemoveInfoBar();
}

LinkInfoBarDelegate* LinkInfoBar::GetDelegate() {
  return delegate()->AsLinkInfoBarDelegate();
}
