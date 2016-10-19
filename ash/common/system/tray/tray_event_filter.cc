// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/system/tray/tray_event_filter.h"

#include "ash/common/system/tray/tray_background_view.h"
#include "ash/common/system/tray/tray_bubble_wrapper.h"
#include "ash/common/wm/container_finder.h"
#include "ash/common/wm_lookup.h"
#include "ash/common/wm_shell.h"
#include "ash/common/wm_window.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ui/views/widget/widget.h"

namespace ash {

TrayEventFilter::TrayEventFilter() {}

TrayEventFilter::~TrayEventFilter() {
  DCHECK(wrappers_.empty());
}

void TrayEventFilter::AddWrapper(TrayBubbleWrapper* wrapper) {
  bool was_empty = wrappers_.empty();
  wrappers_.insert(wrapper);
  if (was_empty && !wrappers_.empty()) {
    WmShell::Get()->AddPointerWatcher(this,
                                      views::PointerWatcherEventTypes::BASIC);
  }
}

void TrayEventFilter::RemoveWrapper(TrayBubbleWrapper* wrapper) {
  wrappers_.erase(wrapper);
  if (wrappers_.empty())
    WmShell::Get()->RemovePointerWatcher(this);
}

void TrayEventFilter::OnPointerEventObserved(
    const ui::PointerEvent& event,
    const gfx::Point& location_in_screen,
    views::Widget* target) {
  if (event.type() == ui::ET_POINTER_DOWN)
    ProcessPressedEvent(location_in_screen, target);
}

void TrayEventFilter::ProcessPressedEvent(const gfx::Point& location_in_screen,
                                          views::Widget* target) {
  if (target) {
    WmWindow* window = WmLookup::Get()->GetWindowForWidget(target);
    int container_id = wm::GetContainerForWindow(window)->GetShellWindowId();
    // Don't process events that occurred inside an embedded menu, for example
    // the right-click menu in a popup notification.
    if (container_id == kShellWindowId_MenuContainer)
      return;
    // Don't process events that occurred inside a popup notification
    // from message center.
    if (container_id == kShellWindowId_StatusContainer &&
        window->GetType() == ui::wm::WINDOW_TYPE_POPUP &&
        target->IsAlwaysOnTop()) {
      return;
    }
  }

  std::set<TrayBackgroundView*> trays;
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
      continue;
    if (wrapper->tray()) {
      // If the user clicks on the parent tray, don't process the event here,
      // let the tray logic handle the event and determine show/hide behavior.
      bounds = wrapper->tray()->GetBoundsInScreen();
      if (bounds.Contains(location_in_screen))
        continue;
    }
    trays.insert((*iter)->tray());
  }

  // Close all bubbles other than the one a user clicked on the tray
  // or its bubble.
  for (std::set<TrayBackgroundView*>::iterator iter = trays.begin();
       iter != trays.end(); ++iter) {
    (*iter)->ClickedOutsideBubble();
  }
}

}  // namespace ash
