// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/contents_container.h"

#include "base/logging.h"

using content::WebContents;

// static
const char ContentsContainer::kViewClassName[] =
    "browser/ui/views/frame/ContentsContainer";

ContentsContainer::ContentsContainer(views::View* active)
    : active_(active),
      preview_(NULL),
      preview_web_contents_(NULL),
      active_top_margin_(0) {
  AddChildView(active_);
}

ContentsContainer::~ContentsContainer() {
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
  // The active view always gets the full bounds.
  active_->SetBounds(0, active_top_margin_, width(),
                     std::max(0, height() - active_top_margin_));

  if (preview_)
    preview_->SetBounds(0, 0, width(), height());

  // Need to invoke views::View in case any views whose bounds didn't change
  // still need a layout.
  views::View::Layout();
}

std::string ContentsContainer::GetClassName() const {
  return kViewClassName;
}
