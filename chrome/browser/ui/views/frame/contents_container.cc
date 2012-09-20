// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/contents_container.h"

#include "base/logging.h"
#include "ui/views/controls/webview/webview.h"

using content::WebContents;

// static
const char ContentsContainer::kViewClassName[] =
    "browser/ui/views/frame/ContentsContainer";

ContentsContainer::ContentsContainer(views::WebView* active)
    : active_(active),
      overlay_(NULL),
      preview_(NULL),
      preview_web_contents_(NULL),
      active_top_margin_(0),
      preview_height_(100),
      preview_height_units_(INSTANT_SIZE_PERCENT) {
  AddChildView(active_);
}

ContentsContainer::~ContentsContainer() {
}

void ContentsContainer::SetActive(views::WebView* active) {
  if (active_)
    RemoveChildView(active_);
  active_ = active;
  // Note the active view is always the first child.
  if (active_)
    AddChildViewAt(active_, 0);
  Layout();
}

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

void ContentsContainer::SetPreview(views::WebView* preview,
                                   WebContents* preview_web_contents,
                                   int height,
                                   InstantSizeUnits units) {
  const int old_height = PreviewHeightInPixels();
  preview_height_ = height;
  preview_height_units_ = units;
  if (preview == preview_ && preview_web_contents_ == preview_web_contents &&
      old_height == PreviewHeightInPixels())
    return;
  if (preview_ != preview) {
    if (preview_)
      RemoveChildView(preview_);
    preview_ = preview;
    if (preview_)
      AddChildView(preview_);
  }
  preview_web_contents_ = preview_web_contents;
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

  if (active_)
    active_->SetBounds(0, content_y, width(), content_height);

  if (overlay_)
    overlay_->SetBounds(0, 0, width(), height());

  if (preview_)
    preview_->SetBounds(0, 0, width(), PreviewHeightInPixels());

  // Need to invoke views::View in case any views whose bounds didn't change
  // still need a layout.
  views::View::Layout();
}

int ContentsContainer::PreviewHeightInPixels() const {
  switch (preview_height_units_) {
    case INSTANT_SIZE_PERCENT:
      return std::min(height(), (height() * preview_height_) / 100);

    case INSTANT_SIZE_PIXELS:
      return std::min(height(), preview_height_);
  }
  NOTREACHED() << "unknown units: " << preview_height_units_;
  return 0;
}

std::string ContentsContainer::GetClassName() const {
  return kViewClassName;
}
