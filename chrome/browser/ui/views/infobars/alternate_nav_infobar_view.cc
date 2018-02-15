// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/infobars/alternate_nav_infobar_view.h"

#include <stddef.h>

#include <utility>

#include "base/logging.h"
#include "build/build_config.h"
#include "chrome/browser/ui/omnibox/alternate_nav_infobar_delegate.h"
#include "chrome/browser/ui/views_mode_controller.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/text_elider.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link.h"


// AlternateNavInfoBarDelegate -------------------------------------------------

// static
std::unique_ptr<infobars::InfoBar> AlternateNavInfoBarDelegate::CreateInfoBar(
    std::unique_ptr<AlternateNavInfoBarDelegate> delegate) {
#if defined(OS_MACOSX)
  if (views_mode_controller::IsViewsBrowserCocoa()) {
    return CreateInfoBarCocoa(std::move(delegate));
  }
#endif
  return std::make_unique<AlternateNavInfoBarView>(std::move(delegate));
}


// AlternateNavInfoBarView -----------------------------------------------------

AlternateNavInfoBarView::AlternateNavInfoBarView(
    std::unique_ptr<AlternateNavInfoBarDelegate> delegate)
    : InfoBarView(std::move(delegate)),
      label_1_(NULL),
      link_(NULL),
      label_2_(NULL) {}

AlternateNavInfoBarView::~AlternateNavInfoBarView() {
}

// static
void AlternateNavInfoBarView::ElideLabels(Labels* labels, int available_width) {
  views::Label* last_label = labels->back();
  labels->pop_back();
  int used_width = 0;
  for (Labels::iterator i(labels->begin()); i != labels->end(); ++i)
    used_width += (*i)->GetPreferredSize().width();
  int last_label_width = std::min(last_label->GetPreferredSize().width(),
                                  available_width - used_width);
  if (last_label_width < last_label->GetMinimumSize().width()) {
    last_label_width = 0;
    if (!labels->empty())
      labels->back()->SetText(labels->back()->text() + gfx::kEllipsisUTF16);
  }
  last_label->SetSize(gfx::Size(last_label_width, last_label->height()));
  if (!labels->empty())
    ElideLabels(labels, available_width - last_label_width);
}

void AlternateNavInfoBarView::Layout() {
  InfoBarView::Layout();

  label_1_->SetText(label_1_text_);
  link_->SetText(link_text_);
  label_2_->SetText(label_2_text_);
  Labels labels;
  labels.push_back(label_1_);
  labels.push_back(link_);
  labels.push_back(label_2_);
  ElideLabels(&labels, EndX() - StartX());

  label_1_->SetPosition(gfx::Point(StartX(), OffsetY(label_1_)));
  link_->SetPosition(gfx::Point(label_1_->bounds().right(), OffsetY(link_)));
  label_2_->SetPosition(gfx::Point(link_->bounds().right(), OffsetY(label_2_)));
}

void AlternateNavInfoBarView::ViewHierarchyChanged(
    const ViewHierarchyChangedDetails& details) {
  if (details.is_add && (details.child == this) && (label_1_ == NULL)) {
    AlternateNavInfoBarDelegate* delegate = GetDelegate();
    size_t offset;
    base::string16 message_text = delegate->GetMessageTextWithOffset(&offset);
    DCHECK_NE(base::string16::npos, offset);
    label_1_text_ = message_text.substr(0, offset);
    label_1_ = CreateLabel(label_1_text_);
    AddViewToContentArea(label_1_);

    link_text_ = delegate->GetLinkText();
    link_ = CreateLink(link_text_, this);
    AddViewToContentArea(link_);

    label_2_text_ = message_text.substr(offset);
    label_2_ = CreateLabel(label_2_text_);
    AddViewToContentArea(label_2_);
  }

  // This must happen after adding all other children so InfoBarView can ensure
  // the close button is the last child.
  InfoBarView::ViewHierarchyChanged(details);
}

int AlternateNavInfoBarView::ContentMinimumWidth() const {
  int label_1_width = label_1_->GetMinimumSize().width();
  return label_1_width ? label_1_width : link_->GetMinimumSize().width();
}

void AlternateNavInfoBarView::LinkClicked(views::Link* source,
                                          int event_flags) {
  if (!owner())
    return;  // We're closing; don't call anything, it might access the owner.
  DCHECK(link_ != NULL);
  DCHECK_EQ(link_, source);
  if (GetDelegate()->LinkClicked(ui::DispositionFromEventFlags(event_flags)))
    RemoveSelf();
}

AlternateNavInfoBarDelegate* AlternateNavInfoBarView::GetDelegate() {
  return static_cast<AlternateNavInfoBarDelegate*>(delegate());
}
