// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/cursor_manager.h"

#include "content/browser/renderer_host/render_widget_host_view_base.h"

#if defined(OS_WIN)
#include "base/command_line.h"
#include "content/public/common/content_switches.h"
#endif

namespace content {

CursorManager::CursorManager(RenderWidgetHostViewBase* root)
    : view_under_cursor_(root),
      root_view_(root),
#if defined(OS_WIN)
      enable_logging_for_test_(false),
#endif
      tooltip_observer_for_testing_(nullptr) {
#if defined(OS_WIN)
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kBrowserTest))
    enable_logging_for_test_ = true;
#endif
}

CursorManager::~CursorManager() {}

void CursorManager::UpdateCursor(RenderWidgetHostViewBase* view,
                                 const WebCursor& cursor) {
  cursor_map_[view] = cursor;
#if defined(OS_WIN)
  if (enable_logging_for_test_) {
    enable_logging_for_test_ = false;
    LOG(ERROR) << "Setting first cursor for view = " << view
               << ", this = " << this;
  }
#endif
  if (view == view_under_cursor_)
    root_view_->DisplayCursor(cursor);
}

void CursorManager::SetTooltipTextForView(const RenderWidgetHostViewBase* view,
                                          const base::string16& tooltip_text) {
  if (view == view_under_cursor_) {
    root_view_->DisplayTooltipText(tooltip_text);
    if (tooltip_observer_for_testing_ && view) {
      tooltip_observer_for_testing_->OnSetTooltipTextForView(view,
                                                             tooltip_text);
    }
  }
}

void CursorManager::UpdateViewUnderCursor(RenderWidgetHostViewBase* view) {
  if (view == view_under_cursor_)
    return;

  // Whenever we switch from one view to another, clear the tooltip: as the
  // mouse moves, the view now controlling the cursor will send a new tooltip,
  // though this is only guaranteed if the view's tooltip is non-empty, so
  // clearing here is important. Tooltips sent from the previous view will be
  // ignored.
  SetTooltipTextForView(view_under_cursor_, base::string16());
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

void CursorManager::SetTooltipObserverForTesting(TooltipObserver* observer) {
  tooltip_observer_for_testing_ = observer;
}

}  // namespace content
