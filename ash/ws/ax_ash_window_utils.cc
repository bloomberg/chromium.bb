// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ws/ax_ash_window_utils.h"

#include <vector>

#include "ash/shell.h"
#include "ash/ws/window_lookup.h"
#include "base/stl_util.h"
#include "ui/views/widget/widget.h"

namespace ash {

AXAshWindowUtils::AXAshWindowUtils() = default;

AXAshWindowUtils::~AXAshWindowUtils() = default;

aura::Window* AXAshWindowUtils::GetParent(aura::Window* window) {
  // Use --log-level=1 --vmodule=*ax_ash_window_utils*=1 to debug.
  DVLOG(1) << "GetParent for " << window->GetName();
  aura::Window* parent = window->parent();
  if (parent)
    return parent;

  // If we don't have a parent, this might be the DesktopWindowTreeHostMus
  // root window inside a client Widget. Check for a proxy window in ash.
  aura::Window* proxy = window_lookup::GetProxyWindowForClientWindow(window);
  if (!proxy)
    return nullptr;

  // "Jump the fence" to the parent of ash's proxy window. This will usually
  // be a container window, like DefaultContainer.
  return proxy->parent();
}

aura::Window::Windows AXAshWindowUtils::GetChildren(aura::Window* window) {
  DVLOG(1) << "GetChildren for " << window->GetName();
  std::vector<aura::Window*> windows;
  windows.reserve(window->children().size());
  for (aura::Window* child : window->children()) {
    if (!window_lookup::IsProxyWindow(child)) {
      // Window is owned by ash, behave as usual.
      windows.push_back(child);
      continue;
    }
    if (window_lookup::IsProxyWindowForOutOfProcess(child)) {
      // Remote process clients like shortcut_viewer rely on the accessibility
      // system serializing the Widget and Windows in ash.
      windows.push_back(child);
      continue;
    }
    // "Jump the fence" from ash proxy window to client window.
    aura::Window* client = window_lookup::GetClientWindowForProxyWindow(child);
    // Client window may not exist during DesktopWindowTreeHostMus teardown.
    if (client)
      windows.push_back(client);
  }
  return windows;
}

bool AXAshWindowUtils::IsRootWindow(aura::Window* window) const {
  if (!window->IsRootWindow())
    return false;
  // SingleProcessMash behaves like classic ash. Only display roots are
  // considered root windows for accessibility, not top-level Widgets or embeds.
  std::vector<aura::Window*> roots = Shell::GetAllRootWindows();
  return base::ContainsValue(roots, window);
}

views::Widget* AXAshWindowUtils::GetWidgetForNativeView(aura::Window* window) {
  // If the window is owned by ash, behave as usual.
  if (!window_lookup::IsProxyWindow(window))
    return views::Widget::GetWidgetForNativeView(window);

  // Remote process clients like shortcut_viewer rely on the accessibility
  // system serializing the Widget and Windows in ash.
  if (window_lookup::IsProxyWindowForOutOfProcess(window))
    return views::Widget::GetWidgetForNativeView(window);

  // For other proxy windows, jump the fence into the client window.
  aura::Window* client = window_lookup::GetClientWindowForProxyWindow(window);
  // Client window may not exist during DesktopWindowTreeHostMus teardown.
  if (!client)
    return nullptr;
  return views::Widget::GetWidgetForNativeView(client);
}

}  // namespace ash
