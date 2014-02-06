// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/devtools_contents_resizing_strategy.h"

#include <algorithm>

DevToolsContentsResizingStrategy::DevToolsContentsResizingStrategy() {
}

DevToolsContentsResizingStrategy::DevToolsContentsResizingStrategy(
    const gfx::Insets& insets, const gfx::Size& min_size)
    : insets_(insets),
      min_size_(min_size) {
}

void DevToolsContentsResizingStrategy::CopyFrom(
    const DevToolsContentsResizingStrategy& strategy) {
  insets_ = strategy.insets();
  min_size_ = strategy.min_size();
}

bool DevToolsContentsResizingStrategy::Equals(
    const DevToolsContentsResizingStrategy& strategy) {
  return insets_ == strategy.insets() && min_size_ == strategy.min_size();
}

void ApplyDevToolsContentsResizingStrategy(
    const DevToolsContentsResizingStrategy& strategy,
    const gfx::Size& container_size,
    const gfx::Rect& old_devtools_bounds,
    const gfx::Rect& old_contents_bounds,
    gfx::Rect* new_devtools_bounds,
    gfx::Rect* new_contents_bounds) {
  new_devtools_bounds->SetRect(
      0, 0, container_size.width(), container_size.height());

  const gfx::Insets& insets = strategy.insets();
  const gfx::Size& min_size = strategy.min_size();

  int width = std::max(0, container_size.width() - insets.width());
  int left = insets.left();
  if (width < min_size.width() && insets.width() > 0) {
    int min_width = std::min(min_size.width(), container_size.width());
    int insets_width = container_size.width() - min_width;
    int insets_decrease = insets.width() - insets_width;
    // Decrease both left and right insets proportionally.
    left -= insets_decrease * insets.left() / insets.width();
    width = min_width;
  }
  left = std::max(0, std::min(container_size.width(), left));

  int height = std::max(0, container_size.height() - insets.height());
  int top = insets.top();
  if (height < min_size.height() && insets.height() > 0) {
    int min_height = std::min(min_size.height(), container_size.height());
    int insets_height = container_size.height() - min_height;
    int insets_decrease = insets.height() - insets_height;
    // Decrease both top and bottom insets proportionally.
    top -= insets_decrease * insets.top() / insets.height();
    height = min_height;
  }
  top = std::max(0, std::min(container_size.height(), top));

  new_contents_bounds->SetRect(left, top, width, height);
}
