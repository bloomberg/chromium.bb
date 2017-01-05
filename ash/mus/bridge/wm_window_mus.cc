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

int WmWindowMus::GetIntProperty(WmWindowProperty key) {
  if (key == WmWindowProperty::SHELF_ITEM_TYPE) {
    if (aura_window()->GetProperty(kShelfItemTypeKey) != TYPE_UNDEFINED)
      return aura_window()->GetProperty(kShelfItemTypeKey);

    // Mash provides a default shelf item type for non-ignored windows.
    return GetWindowState()->ignored_by_shelf() ? TYPE_UNDEFINED : TYPE_APP;
  }

  return WmWindowAura::GetIntProperty(key);
}

// TODO(sky): investigate if needed.
bool WmWindowMus::MoveToEventRoot(const ui::Event& event) {
  views::View* target = static_cast<views::View*>(event.target());
  if (!target)
    return false;
  WmWindow* target_root =
      WmLookup::Get()->GetWindowForWidget(target->GetWidget())->GetRootWindow();
  if (!target_root || target_root == GetRootWindow())
    return false;
  WmWindow* window_container =
      target_root->GetChildByShellWindowId(GetParent()->GetShellWindowId());
  window_container->AddChild(this);
  return true;
}

// TODO(sky): investigate if needed.
void WmWindowMus::SetBoundsInScreen(const gfx::Rect& bounds_in_screen,
                                    const display::Display& dst_display) {
  DCHECK(GetParent());  // Aura code assumed a parent, so this does too.
  if (static_cast<const WmWindowMus*>(GetParent())
          ->child_bounds_in_screen_behavior_ ==
      BoundsInScreenBehavior::USE_LOCAL_COORDINATES) {
    SetBounds(bounds_in_screen);
    return;
  }
  wm::SetBoundsInScreen(this, bounds_in_screen, dst_display);
}

// TODO(sky): remove this override.
void WmWindowMus::SetPinned(bool trusted) {
  // http://crbug.com/622486.
  NOTIMPLEMENTED();
}

void WmWindowMus::CloseWidget() {
  views::Widget* widget = views::Widget::GetWidgetForNativeView(aura_window());
  DCHECK(widget);
  // Allow the client to service the close request for remote widgets.
  if (aura_window()->GetProperty(kWidgetCreationTypeKey) ==
      WidgetCreationType::FOR_CLIENT) {
    WmShellMus::Get()->window_manager()->window_manager_client()->RequestClose(
        aura_window());
  } else {
    widget->Close();
  }
}

// TODO(sky): investigate if needed.
bool WmWindowMus::CanActivate() const {
  // TODO(sky): this isn't quite right. Should key off CanFocus(), which is not
  // replicated.
  // TODO(sky): fix const cast (most likely remove this override entirely).
  return WmWindowAura::CanActivate() &&
         views::Widget::GetWidgetForNativeView(
             const_cast<aura::Window*>(aura_window())) != nullptr;
}

void WmWindowMus::ShowResizeShadow(int component) {
  // TODO: http://crbug.com/640773.
  NOTIMPLEMENTED();
}

void WmWindowMus::HideResizeShadow() {
  // TODO: http://crbug.com/640773.
  NOTIMPLEMENTED();
}

void WmWindowMus::InstallResizeHandleWindowTargeter(
    ImmersiveFullscreenController* immersive_fullscreen_controller) {
  // TODO(sky): I believe once ImmersiveFullscreenController is ported this
  // won't be necessary in mash, but I need to verify that:
  // http://crbug.com/548435.
}

// TODO: nuke this once SetBoundsInScreen() is updated.
void WmWindowMus::SetBoundsInScreenBehaviorForChildren(
    WmWindow::BoundsInScreenBehavior behavior) {
  child_bounds_in_screen_behavior_ = behavior;
}

void WmWindowMus::AddLimitedPreTargetHandler(ui::EventHandler* handler) {
  DCHECK(WmShellMus::Get()->window_tree_client()->WasCreatedByThisClient(
      aura::WindowMus::Get(aura_window())));
  WmWindowAura::AddLimitedPreTargetHandler(handler);
}

}  // namespace mus
}  // namespace ash
