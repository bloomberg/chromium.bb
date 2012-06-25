// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/contents_container.h"

#include "base/logging.h"
#include "ui/views/layout/fill_layout.h"

using content::WebContents;

class ContentsContainer::HeaderView : public views::View {
 public:
  HeaderView();

  bool should_show() { return child_count() && child_at(0)->visible(); }

 protected:
  // views::View overrides:
  virtual void ChildPreferredSizeChanged(views::View* child) OVERRIDE;

 private:
  bool child_visibile_;

  DISALLOW_COPY_AND_ASSIGN(HeaderView);
};

ContentsContainer::HeaderView::HeaderView() : child_visibile_(false) {}

void ContentsContainer::HeaderView::ChildPreferredSizeChanged(
    views::View* child) {
  if (parent() && (child->visible() || child_visibile_))
    parent()->Layout();
  child_visibile_ = child->visible();
}

// static
const char ContentsContainer::kViewClassName[] =
    "browser/ui/views/frame/ContentsContainer";

ContentsContainer::ContentsContainer(views::View* active)
    : header_(new HeaderView()),
      active_(active),
      overlay_(NULL),
      preview_(NULL),
      preview_web_contents_(NULL),
      active_top_margin_(0) {
  AddChildView(active_);
  header_->SetLayoutManager(new views::FillLayout);
  AddChildView(header_);
}

ContentsContainer::~ContentsContainer() {
}

views::View* ContentsContainer::header() { return header_; }

void ContentsContainer::SetOverlay(views::View* overlay) {
  if (overlay_)
    RemoveChildView(overlay_);
  overlay_ = overlay;
  if (overlay_)
    AddChildView(overlay_);
  Layout();
}

void ContentsContainer::MakePreviewContentsActiveContents() {
  DCHECK(preview_);

  active_ = preview_;
  preview_ = NULL;
  preview_web_contents_ = NULL;
  Layout();
}

void ContentsContainer::SetPreview(views::View* preview,
                                   WebContents* preview_web_contents) {
  if (preview == preview_)
    return;

  if (preview_)
    RemoveChildView(preview_);
  preview_ = preview;
  preview_web_contents_ = preview_web_contents;
  if (preview_)
    AddChildView(preview_);

  Layout();
}

void ContentsContainer::SetActiveTopMargin(int margin) {
  if (active_top_margin_ == margin)
    return;

  active_top_margin_ = margin;
  // Make sure we layout next time around. We need this in case our bounds
  // haven't changed.
  InvalidateLayout();
}

gfx::Rect ContentsContainer::GetPreviewBounds() {
  gfx::Point screen_loc;
  ConvertPointToScreen(this, &screen_loc);
  return gfx::Rect(screen_loc, size());
}

void ContentsContainer::Layout() {
  int content_y = active_top_margin_;
  int content_height = std::max(0, height() - content_y);
  if (header_->should_show()) {
    gfx::Size header_pref = header_->GetPreferredSize();
    int header_height = std::min(content_height, header_pref.height());
    content_height -= header_height;
    header_->SetBounds(0, content_y, width(), header_height);
    content_y += header_height;
  } else {
    header_->SetBoundsRect(gfx::Rect());
  }
  active_->SetBounds(0, content_y, width(), content_height);

  if (overlay_)
    overlay_->SetBounds(0, 0, width(), height());

  if (preview_)
    preview_->SetBounds(0, 0, width(), height());

  // Need to invoke views::View in case any views whose bounds didn't change
  // still need a layout.
  views::View::Layout();
}

std::string ContentsContainer::GetClassName() const {
  return kViewClassName;
}
