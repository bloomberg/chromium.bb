// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/mus/bridge/wm_window_mus.h"

#include "ash/common/shelf/shelf_item_types.h"
#include "ash/common/wm/container_finder.h"
#include "ash/common/wm/window_positioning_utils.h"
#include "ash/common/wm/window_state.h"
#include "ash/common/wm_layout_manager.h"
#include "ash/common/wm_lookup.h"
#include "ash/common/wm_transient_window_observer.h"
#include "ash/common/wm_window_observer.h"
#include "ash/common/wm_window_property.h"
#include "ash/mus/bridge/wm_root_window_controller_mus.h"
#include "ash/mus/bridge/wm_shell_mus.h"
#include "ash/mus/property_util.h"
#include "ash/mus/window_manager.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/wm/window_properties.h"
#include "services/ui/public/interfaces/window_manager.mojom.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/mus/window_manager_delegate.h"
#include "ui/aura/mus/window_mus.h"
#include "ui/aura/mus/window_tree_client.h"
#include "ui/aura/window.h"
#include "ui/base/hit_test.h"
#include "ui/display/display.h"
#include "ui/resources/grit/ui_resources.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace ash {
namespace mus {

WmWindowMus::WmWindowMus(aura::Window* window) : WmWindowAura(window) {}

WmWindowMus::~WmWindowMus() {}

// static
const WmWindowMus* WmWindowMus::Get(const aura::Window* window) {
  if (!window)
    return nullptr;

  if (HasInstance(window))
    return static_cast<const WmWindowMus*>(WmWindowAura::Get(window));

  // WmWindowMus is owned by the aura::Window.
  // Unfortunately there isn't a good way to avoid the cast here.
  return new WmWindowMus(const_cast<aura::Window*>(window));
}

// static
WmWindowMus* WmWindowMus::Get(views::Widget* widget) {
  return WmWindowMus::Get(widget->GetNativeView());
}

const WmRootWindowControllerMus* WmWindowMus::GetRootWindowControllerMus()
    const {
  return WmRootWindowControllerMus::Get(aura_window()->GetRootWindow());
}

bool WmWindowMus::IsContainer() const {
  return GetShellWindowId() != kShellWindowId_Invalid;
}

WmRootWindowController* WmWindowMus::GetRootWindowController() {
  return GetRootWindowControllerMus();
}

WmShell* WmWindowMus::GetShell() const {
  return WmShellMus::Get();
}

void WmWindowMus::CloseWidget() {
  // NOTE: in the FOR_CLIENT case there is not necessarily a widget associated
  // with the window. Mash only creates widgets for top level windows if mash
  // renders the non-client frame.
  if (aura_window()->GetProperty(kWidgetCreationTypeKey) ==
      WidgetCreationType::FOR_CLIENT) {
    WmShellMus::Get()->window_manager()->window_manager_client()->RequestClose(
        aura_window());
  } else {
    WmWindowAura::CloseWidget();
  }
}

void WmWindowMus::AddLimitedPreTargetHandler(ui::EventHandler* handler) {
  DCHECK(WmShellMus::Get()->window_tree_client()->WasCreatedByThisClient(
      aura::WindowMus::Get(aura_window())));
  WmWindowAura::AddLimitedPreTargetHandler(handler);
}

}  // namespace mus
}  // namespace ash
