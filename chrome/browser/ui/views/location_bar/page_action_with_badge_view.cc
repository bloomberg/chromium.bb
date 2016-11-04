// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/page_action_with_badge_view.h"

#include "chrome/browser/extensions/extension_action.h"
#include "chrome/browser/ui/views/location_bar/page_action_image_view.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/views/layout/fill_layout.h"

PageActionWithBadgeView::PageActionWithBadgeView(
    PageActionImageView* image_view) : image_view_(image_view) {
  AddChildView(image_view_);
  SetLayoutManager(new views::FillLayout());
}

void PageActionWithBadgeView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  node_data->role = ui::AX_ROLE_GROUP;
}

gfx::Size PageActionWithBadgeView::GetPreferredSize() const {
  return gfx::Size(ExtensionAction::ActionIconSize(),
                   ExtensionAction::ActionIconSize());
}

void PageActionWithBadgeView::UpdateVisibility(content::WebContents* contents) {
  image_view_->UpdateVisibility(contents);
  SetVisible(image_view_->visible());
}

const char* PageActionWithBadgeView::GetClassName() const {
  return "PageActionWithBadgeView";
}
