// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/tray/tray_event_filter.h"

#include "ash/root_window_controller.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/system/tray/tray_background_view.h"
#include "ash/system/tray/tray_bubble_wrapper.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_event_filter.h"
#include "ui/aura/client/screen_position_client.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/views/widget/widget.h"

namespace ash {

TrayEventFilter::TrayEventFilter() {
}

TrayEventFilter::~TrayEventFilter() {
  DCHECK(wrappers_.empty());
}

void TrayEventFilter::AddWrapper(TrayBubbleWrapper* wrapper) {
  bool was_empty = wrappers_.empty();
  wrappers_.insert(wrapper);
  if (was_empty && !wrappers_.empty())
    Shell::GetInstance()->AddPointerWatcher(this);
}

void TrayEventFilter::RemoveWrapper(TrayBubbleWrapper* wrapper) {
  wrappers_.erase(wrapper);
  if (wrappers_.empty())
    Shell::GetInstance()->RemovePointerWatcher(this);
}

void TrayEventFilter::OnMousePressed(const ui::MouseEvent& event,
                                     const gfx::Point& location_in_screen) {
  ProcessLocatedEvent(event, location_in_screen);
}

void TrayEventFilter::OnTouchPressed(const ui::TouchEvent& event,
                                     const gfx::Point& location_in_screen) {
  ProcessLocatedEvent(event, location_in_screen);
}

void TrayEventFilter::ProcessLocatedEvent(
    const ui::LocatedEvent& event,
    const gfx::Point& location_in_screen) {
  // TODO(jamescook): Rewrite this to avoid using event.target() as that will
  // always be null on mus. http://crbug.com/608570
  if (event.target()) {
    aura::Window* target = static_cast<aura::Window*>(event.target());
    // Don't process events that occurred inside an embedded menu.
    RootWindowController* root_controller =
        GetRootWindowController(target->GetRootWindow());
    if (root_controller &&
        root_controller->GetContainer(kShellWindowId_MenuContainer)
            ->Contains(target)) {
      return;
    }
    // Don't process events that occurred inside the status area widget and
    // a popup notification from message center.
    if (root_controller &&
        root_controller->GetContainer(kShellWindowId_StatusContainer)
            ->Contains(target)) {
      return;
    }
  }

  // Check the boundary for all wrappers, and do not handle the event if it
  // happens inside of any of those wrappers.
  for (std::set<TrayBubbleWrapper*>::const_iterator iter = wrappers_.begin();
       iter != wrappers_.end(); ++iter) {
    const TrayBubbleWrapper* wrapper = *iter;
    const views::Widget* bubble_widget = wrapper->bubble_widget();
    if (!bubble_widget)
      continue;

    gfx::Rect bounds = bubble_widget->GetWindowBoundsInScreen();
    bounds.Inset(wrapper->bubble_view()->GetBorderInsets());
    if (bounds.Contains(location_in_screen))
      return;
    if (wrapper->tray()) {
      // If the user clicks on the parent tray, don't process the event here,
      // let the tray logic handle the event and determine show/hide behavior.
      bounds = wrapper->tray()->GetBoundsInScreen();
      if (bounds.Contains(location_in_screen))
        return;
    }
  }

  // Handle clicking outside the bubble and tray.
  // Cannot iterate |wrappers_| directly, because clicking outside will remove
  // the wrapper, which shrinks |wrappers_| unsafely.
  std::set<TrayBackgroundView*> trays;
  for (std::set<TrayBubbleWrapper*>::iterator iter = wrappers_.begin();
       iter != wrappers_.end(); ++iter) {
    trays.insert((*iter)->tray());
  }
  for (std::set<TrayBackgroundView*>::iterator iter = trays.begin();
       iter != trays.end(); ++iter) {
    (*iter)->ClickedOutsideBubble();
  }
}

}  // namespace ash
