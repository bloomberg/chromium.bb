// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/tray/tray_event_filter.h"

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shell.h"
#include "ash/shell_port.h"
#include "ash/system/tray/tray_background_view.h"
#include "ash/system/tray/tray_bubble_wrapper.h"
#include "ash/wm/container_finder.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ui/aura/window.h"
#include "ui/gfx/native_widget_types.h"
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
    ShellPort::Get()->AddPointerWatcher(this,
                                        views::PointerWatcherEventTypes::BASIC);
  }
}

void TrayEventFilter::RemoveWrapper(TrayBubbleWrapper* wrapper) {
  wrappers_.erase(wrapper);
  if (wrappers_.empty())
    ShellPort::Get()->RemovePointerWatcher(this);
}

void TrayEventFilter::OnPointerEventObserved(
    const ui::PointerEvent& event,
    const gfx::Point& location_in_screen,
    gfx::NativeView target) {
  if (event.type() == ui::ET_POINTER_DOWN)
    ProcessPressedEvent(location_in_screen, target);
}

void TrayEventFilter::ProcessPressedEvent(const gfx::Point& location_in_screen,
                                          gfx::NativeView target) {
  // The hit target window for the virtual keyboard isn't the same as its
  // views::Widget.
  const views::Widget* target_widget =
      views::Widget::GetTopLevelWidgetForNativeView(target);
  const aura::Window* container =
      target ? wm::GetContainerForWindow(target) : nullptr;
  if (target && container) {
    const int container_id = container->id();
    // Don't process events that occurred inside an embedded menu, for example
    // the right-click menu in a popup notification.
    if (container_id == kShellWindowId_MenuContainer)
      return;
    // Don't process events that occurred inside a popup notification
    // from message center.
    if (container_id == kShellWindowId_StatusContainer &&
        target->type() == aura::client::WINDOW_TYPE_POPUP && target_widget &&
        target_widget->IsAlwaysOnTop()) {
      return;
    }
    // Don't process events that occurred inside a virtual keyboard.
    if (container_id == kShellWindowId_VirtualKeyboardContainer)
      return;
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
    // System tray can be dragged to show the bubble if it is in tablet mode.
    // During the drag, the bubble's logical bounds can extend outside of the
    // work area, but its visual bounds are only within the work area. Restrict
    // |bounds| so that events located outside the bubble's visual bounds are
    // treated as outside of the bubble.
    int bubble_container_id =
        wm::GetContainerForWindow(bubble_widget->GetNativeWindow())->id();
    if (Shell::Get()
            ->tablet_mode_controller()
            ->IsTabletModeWindowManagerEnabled() &&
        bubble_container_id == kShellWindowId_SettingBubbleContainer) {
      bounds.Intersect(bubble_widget->GetWorkAreaBoundsInScreen());
    }
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
