// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/autoclick/autoclick_drag_event_rewriter.h"

namespace ash {

void AutoclickDragEventRewriter::SetEnabled(bool enabled) {
  enabled_ = enabled;
}

bool AutoclickDragEventRewriter::IsEnabled() const {
  return enabled_;
}

ui::EventRewriteStatus AutoclickDragEventRewriter::RewriteEvent(
    const ui::Event& event,
    std::unique_ptr<ui::Event>* new_event) {
  // Only rewrite mouse moved events to drag events when enabled.
  if (!enabled_)
    return ui::EVENT_REWRITE_CONTINUE;

  // On touchpads, a SCROLL_FLING_CANCEL can also indicate the start of a drag.
  // If this rewriter is enabled, a SCROLL_FLING_CANCEL should simply be
  // ignored.
  if (event.type() == ui::ET_SCROLL_FLING_CANCEL)
    return ui::EVENT_REWRITE_DISCARD;

  // Only rewrite move events, but any other type should still go through.
  if (event.type() != ui::ET_MOUSE_MOVED)
    return ui::EVENT_REWRITE_CONTINUE;

  const ui::MouseEvent* mouse_event = event.AsMouseEvent();
  ui::MouseEvent* rewritten_event = new ui::MouseEvent(
      ui::ET_MOUSE_DRAGGED, mouse_event->location(),
      mouse_event->root_location(), mouse_event->time_stamp(),
      mouse_event->flags(), mouse_event->changed_button_flags(),
      mouse_event->pointer_details());
  new_event->reset(rewritten_event);
  return ui::EVENT_REWRITE_REWRITTEN;
}

ui::EventRewriteStatus AutoclickDragEventRewriter::NextDispatchEvent(
    const ui::Event& last_event,
    std::unique_ptr<ui::Event>* new_event) {
  // Unused.
  return ui::EVENT_REWRITE_CONTINUE;
}

}  // namespace ash
