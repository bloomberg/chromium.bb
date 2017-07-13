// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cursor_manager.h"

#include "content/browser/renderer_host/render_widget_host_view_base.h"

namespace content {

CursorManager::CursorManager(RenderWidgetHostViewBase* root)
    : view_under_cursor_(root), root_view_(root) {}

CursorManager::~CursorManager() {}

void CursorManager::UpdateCursor(RenderWidgetHostViewBase* view,
                                 const WebCursor& cursor) {
  cursor_map_[view] = cursor;
  if (view == view_under_cursor_)
    root_view_->DisplayCursor(cursor);
}

void CursorManager::UpdateViewUnderCursor(RenderWidgetHostViewBase* view) {
  view_under_cursor_ = view;
  WebCursor cursor;

  // If no UpdateCursor has been received for this view, use an empty cursor.
  auto it = cursor_map_.find(view);
  if (it != cursor_map_.end())
    cursor = it->second;

  root_view_->DisplayCursor(cursor);
}

void CursorManager::ViewBeingDestroyed(RenderWidgetHostViewBase* view) {
  cursor_map_.erase(view);

  // If the view right under the mouse is going away, use the root's cursor
  // until UpdateViewUnderCursor is called again.
  if (view == view_under_cursor_ && view != root_view_)
    UpdateViewUnderCursor(root_view_);
}

bool CursorManager::GetCursorForTesting(RenderWidgetHostViewBase* view,
                                        WebCursor& cursor) {
  if (cursor_map_.find(view) == cursor_map_.end())
    return false;

  cursor = cursor_map_[view];
  return true;
}

}  // namespace content