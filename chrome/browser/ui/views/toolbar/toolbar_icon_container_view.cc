// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/toolbar_icon_container_view.h"

#include <memory>

#include "ui/views/layout/flex_layout.h"

ToolbarIconContainerView::ToolbarIconContainerView() {
  SetLayoutManager(std::make_unique<views::FlexLayout>());
}

ToolbarIconContainerView::~ToolbarIconContainerView() = default;

void ToolbarIconContainerView::UpdateAllIcons() {}

void ToolbarIconContainerView::ChildPreferredSizeChanged(views::View* child) {
  PreferredSizeChanged();
}

void ToolbarIconContainerView::ChildVisibilityChanged(views::View* child) {
  PreferredSizeChanged();
}
