// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/toolbar_icon_container_view.h"

#include <memory>

#include "chrome/browser/ui/layout_constants.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/view_class_properties.h"

ToolbarIconContainerView::ToolbarIconContainerView() {
  auto layout_manager = std::make_unique<views::FlexLayout>();
  layout_manager->SetCollapseMargins(true).SetDefaultChildMargins(
      gfx::Insets(0, 0, 0, GetLayoutConstant(TOOLBAR_ELEMENT_PADDING)));
  SetLayoutManager(std::move(layout_manager));
}

ToolbarIconContainerView::~ToolbarIconContainerView() = default;

void ToolbarIconContainerView::UpdateAllIcons() {}

void ToolbarIconContainerView::AddMainView(views::View* main_view) {
  DCHECK(!main_view_);
  // Set empty margins from this view to negate the default ones set in the
  // constructor.
  main_view->SetProperty(views::kMarginsKey, new gfx::Insets());
  main_view_ = main_view;
  AddChildView(main_view_);
}

void ToolbarIconContainerView::ChildPreferredSizeChanged(views::View* child) {
  PreferredSizeChanged();
}

void ToolbarIconContainerView::ChildVisibilityChanged(views::View* child) {
  PreferredSizeChanged();
}
