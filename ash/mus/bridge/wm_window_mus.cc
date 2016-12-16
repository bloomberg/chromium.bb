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

// static
bool WmWindowMus::default_use_empty_minimum_size_for_testing_ = false;

WmWindowMus::WmWindowMus(aura::Window* window)
    : WmWindowAura(window),
      use_empty_minimum_size_for_testing_(
          default_use_empty_minimum_size_for_testing_) {}

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

bool WmWindowMus::ShouldUseExtendedHitRegion() const {
  const WmWindowMus* parent = Get(aura_window()->parent());
  return parent && parent->children_use_extended_hit_region_;
}

bool WmWindowMus::IsContainer() const {
  return GetShellWindowId() != kShellWindowId_Invalid;
}

const WmWindow* WmWindowMus::GetRootWindow() const {
  return Get(aura_window()->GetRootWindow());
}

WmRootWindowController* WmWindowMus::GetRootWindowController() {
  return GetRootWindowControllerMus();
}

WmShell* WmWindowMus::GetShell() const {
  return WmShellMus::Get();
}

bool WmWindowMus::IsBubble() {
  return aura_window()->GetProperty(aura::client::kWindowTypeKey) ==
         ui::mojom::WindowType::BUBBLE;
}

bool WmWindowMus::HasNonClientArea() {
  return widget_ ? true : false;
}

int WmWindowMus::GetNonClientComponent(const gfx::Point& location) {
  return widget_ ? widget_->GetNonClientComponent(location) : HTNOWHERE;
}

gfx::Size WmWindowMus::GetMinimumSize() const {
  return widget_ && !use_empty_minimum_size_for_testing_
             ? widget_->GetMinimumSize()
             : gfx::Size();
}

gfx::Size WmWindowMus::GetMaximumSize() const {
  return widget_ ? widget_->GetMaximumSize() : gfx::Size();
}

gfx::Rect WmWindowMus::GetMinimizeAnimationTargetBoundsInScreen() const {
  // TODO: need animation support: http://crbug.com/615087.
  NOTIMPLEMENTED();
  return GetBoundsInScreen();
}

bool WmWindowMus::IsSystemModal() const {
  NOTIMPLEMENTED();
  return false;
}

bool WmWindowMus::GetBoolProperty(WmWindowProperty key) {
  switch (key) {
    case WmWindowProperty::SNAP_CHILDREN_TO_PIXEL_BOUNDARY:
      return snap_children_to_pixel_boundary_;

    default:
      break;
  }

  return WmWindowAura::GetBoolProperty(key);
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

WmWindow* WmWindowMus::GetToplevelWindow() {
  return WmShellMus::GetToplevelAncestor(aura_window());
}

WmWindow* WmWindowMus::GetToplevelWindowForFocus() {
  // TODO(sky): resolve if we really need two notions of top-level. In the mus
  // world they are the same.
  return WmShellMus::GetToplevelAncestor(aura_window());
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

views::Widget* WmWindowMus::GetInternalWidget() {
  // Don't return the window frame widget for an embedded client window.
  if (widget_creation_type_ == WidgetCreationType::FOR_CLIENT)
    return nullptr;

  return widget_;
}

void WmWindowMus::CloseWidget() {
  DCHECK(widget_);
  // Allow the client to service the close request for remote widgets.
  if (widget_creation_type_ == WidgetCreationType::FOR_CLIENT) {
    WmShellMus::Get()->window_manager()->window_manager_client()->RequestClose(
        aura_window());
  } else {
    widget_->Close();
  }
}

// TODO(sky): investigate if needed.
bool WmWindowMus::CanActivate() const {
  // TODO(sky): this isn't quite right. Should key off CanFocus(), which is not
  // replicated.
  return WmWindowAura::CanActivate() && widget_ != nullptr;
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

void WmWindowMus::SetSnapsChildrenToPhysicalPixelBoundary() {
  if (snap_children_to_pixel_boundary_)
    return;

  snap_children_to_pixel_boundary_ = true;
  for (auto& observer : observers()) {
    observer.OnWindowPropertyChanged(
        this, WmWindowProperty::SNAP_CHILDREN_TO_PIXEL_BOUNDARY);
  }
}

void WmWindowMus::SnapToPixelBoundaryIfNecessary() {
  WmWindowMus* parent = Get(aura_window()->parent());
  if (parent && parent->snap_children_to_pixel_boundary_) {
    // TODO: implement snap to pixel: http://crbug.com/615554.
    NOTIMPLEMENTED();
  }
}

void WmWindowMus::SetChildrenUseExtendedHitRegion() {
  children_use_extended_hit_region_ = true;
}

void WmWindowMus::AddLimitedPreTargetHandler(ui::EventHandler* handler) {
  DCHECK(WmShellMus::Get()->window_tree_client()->WasCreatedByThisClient(
      aura::WindowMus::Get(aura_window())));
  WmWindowAura::AddLimitedPreTargetHandler(handler);
}

}  // namespace mus
}  // namespace ash
