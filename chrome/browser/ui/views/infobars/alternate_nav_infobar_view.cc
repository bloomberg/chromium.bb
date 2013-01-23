// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/infobars/alternate_nav_infobar_view.h"

#include "base/logging.h"
#include "chrome/browser/ui/omnibox/alternate_nav_infobar_delegate.h"
#include "ui/base/window_open_disposition.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link.h"

// AlternateNavInfoBarDelegate -------------------------------------------------

InfoBar* AlternateNavInfoBarDelegate::CreateInfoBar(InfoBarService* owner) {
  return new AlternateNavInfoBarView(owner, this);
}

// AlternateNavInfoBarView -----------------------------------------------------

AlternateNavInfoBarView::AlternateNavInfoBarView(
    InfoBarService* owner,
    AlternateNavInfoBarDelegate* delegate)
    : InfoBarView(owner, delegate),
      label_1_(NULL),
      link_(NULL),
      label_2_(NULL) {
}

AlternateNavInfoBarView::~AlternateNavInfoBarView() {
}

void AlternateNavInfoBarView::Layout() {
  InfoBarView::Layout();

  // TODO(pkasting): This isn't perfect; there are points when we should elide a
  // view because its subsequent view will be too small to show an ellipsis.
  gfx::Size label_1_size = label_1_->GetPreferredSize();
  int available_width = EndX() - StartX();
  label_1_->SetBounds(StartX(), OffsetY(label_1_size),
      std::min(label_1_size.width(), available_width), label_1_size.height());
  available_width = std::max(0, available_width - label_1_size.width());

  gfx::Size link_size = link_->GetPreferredSize();
  link_->SetBounds(label_1_->bounds().right(), OffsetY(link_size),
      std::min(link_size.width(), available_width), link_size.height());
  available_width = std::max(0, available_width - link_size.width());

  gfx::Size label_2_size = label_2_->GetPreferredSize();
  label_2_->SetBounds(link_->bounds().right(), OffsetY(label_2_size),
      std::min(label_2_size.width(), available_width), label_2_size.height());
}

void AlternateNavInfoBarView::ViewHierarchyChanged(bool is_add,
                                                   View* parent,
                                                   View* child) {
  if (is_add && (child == this) && (label_1_ == NULL)) {
    AlternateNavInfoBarDelegate* delegate = GetDelegate();
    size_t offset;
    string16 message_text = delegate->GetMessageTextWithOffset(&offset);
    DCHECK_NE(string16::npos, offset);
    label_1_ = CreateLabel(message_text.substr(0, offset));
    AddChildView(label_1_);

    link_ = CreateLink(delegate->GetLinkText(), this);
    AddChildView(link_);

    label_2_ = CreateLabel(message_text.substr(offset));
    AddChildView(label_2_);
  }

  // This must happen after adding all other children so InfoBarView can ensure
  // the close button is the last child.
  InfoBarView::ViewHierarchyChanged(is_add, parent, child);
}

void AlternateNavInfoBarView::LinkClicked(views::Link* source,
                                          int event_flags) {
  if (!owned())
    return;  // We're closing; don't call anything, it might access the owner.
  DCHECK(link_ != NULL);
  DCHECK_EQ(link_, source);
  if (GetDelegate()->LinkClicked(ui::DispositionFromEventFlags(event_flags)))
    RemoveSelf();
}

AlternateNavInfoBarDelegate* AlternateNavInfoBarView::GetDelegate() {
  return static_cast<AlternateNavInfoBarDelegate*>(delegate());
}
