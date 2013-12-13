// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/contents_layout_manager.h"

#include "ui/views/view.h"

ContentsLayoutManager::ContentsLayoutManager(
    views::View* devtools_view,
    views::View* contents_view)
    : devtools_view_(devtools_view),
      contents_view_(contents_view),
      host_(NULL),
      active_top_margin_(0) {
}

ContentsLayoutManager::~ContentsLayoutManager() {
}

void ContentsLayoutManager::SetContentsViewInsets(const gfx::Insets& insets) {
  if (insets_ == insets)
    return;

  insets_ = insets;
  if (host_)
    host_->InvalidateLayout();
}

void ContentsLayoutManager::SetActiveTopMargin(int margin) {
  if (active_top_margin_ == margin)
    return;

  active_top_margin_ = margin;
  if (host_)
    host_->InvalidateLayout();
}

void ContentsLayoutManager::Layout(views::View* contents_container) {
  DCHECK(host_ == contents_container);

  int top = active_top_margin_;
  int height = std::max(0, contents_container->height() - top);
  int width = contents_container->width();
  devtools_view_->SetBounds(0, top, width, height);

  int contents_width = std::max(0, width - insets_.width());
  int contents_height = std::max(0, height - insets_.height());
  contents_view_->SetBounds(
      std::min(insets_.left(), width),
      top + std::min(insets_.top(), height),
      contents_width,
      contents_height);
}

gfx::Size ContentsLayoutManager::GetPreferredSize(views::View* host) {
  return gfx::Size();
}

void ContentsLayoutManager::Installed(views::View* host) {
  DCHECK(!host_);
  host_ = host;
}

void ContentsLayoutManager::Uninstalled(views::View* host) {
  DCHECK(host_ == host);
  host_ = NULL;
}
