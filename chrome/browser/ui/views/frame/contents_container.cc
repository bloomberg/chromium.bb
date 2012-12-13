// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/contents_container.h"

#include "ui/views/controls/webview/webview.h"

// static
const char ContentsContainer::kViewClassName[] =
    "browser/ui/views/frame/ContentsContainer";

ContentsContainer::ContentsContainer(views::WebView* active)
    : active_(active),
      preview_(NULL),
      preview_web_contents_(NULL),
      active_top_margin_(0),
      preview_height_(100),
      preview_height_units_(INSTANT_SIZE_PERCENT),
      extra_content_height_(0) {
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

void ContentsContainer::SetPreview(views::WebView* preview,
                                   content::WebContents* preview_web_contents,
                                   int height,
                                   InstantSizeUnits units) {
  if (preview_ == preview && preview_web_contents_ == preview_web_contents &&
      preview_height_ == height && preview_height_units_ == units)
    return;

  if (preview_ != preview) {
    if (preview_)
      RemoveChildView(preview_);
    preview_ = preview;
    if (preview_)
      AddChildView(preview_);
  }
  preview_web_contents_ = preview_web_contents;
  preview_height_ = height;
  preview_height_units_ = units;
  Layout();
}

void ContentsContainer::MaybeStackPreviewAtTop() {
  if (preview_) {
    RemoveChildView(preview_);
    AddChildView(preview_);
    Layout();
  }
}

void ContentsContainer::SetActiveTopMargin(int margin) {
  if (active_top_margin_ == margin)
    return;

  active_top_margin_ = margin;
  // Make sure we layout next time around. We need this in case our bounds
  // haven't changed.
  InvalidateLayout();
}

gfx::Rect ContentsContainer::GetPreviewBounds() const {
  gfx::Point screen_loc;
  ConvertPointToScreen(this, &screen_loc);
  return gfx::Rect(screen_loc, size());
}

void ContentsContainer::SetExtraContentHeight(int height) {
  if (height == extra_content_height_)
    return;
  extra_content_height_ = height;
}

void ContentsContainer::Layout() {
  int content_y = active_top_margin_;
  int content_height =
      std::max(0, height() - content_y + extra_content_height_);

  active_->SetBounds(0, content_y, width(), content_height);

  if (preview_)
    preview_->SetBounds(0, 0, width(), PreviewHeightInPixels());

  // Need to invoke views::View in case any views whose bounds didn't change
  // still need a layout.
  views::View::Layout();
}

std::string ContentsContainer::GetClassName() const {
  return kViewClassName;
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
