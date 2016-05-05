// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/aura/wm_window_aura.h"

#include "ash/screen_util.h"
#include "ash/shelf/shelf_util.h"
#include "ash/wm/aura/aura_layout_manager_adapter.h"
#include "ash/wm/aura/wm_globals_aura.h"
#include "ash/wm/aura/wm_root_window_controller_aura.h"
#include "ash/wm/common/window_state.h"
#include "ash/wm/common/wm_layout_manager.h"
#include "ash/wm/common/wm_window_observer.h"
#include "ash/wm/common/wm_window_property.h"
#include "ash/wm/window_animations.h"
#include "ash/wm/window_properties.h"
#include "ash/wm/window_state_aura.h"
#include "ash/wm/window_util.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/window_tree_client.h"
#include "ui/aura/layout_manager.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"
#include "ui/aura/window_property.h"
#include "ui/base/hit_test.h"
#include "ui/compositor/layer_tree_owner.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/display/screen.h"
#include "ui/wm/core/coordinate_conversion.h"
#include "ui/wm/core/window_util.h"

DECLARE_WINDOW_PROPERTY_TYPE(ash::wm::WmWindowAura*);

namespace ash {
namespace wm {

DEFINE_OWNED_WINDOW_PROPERTY_KEY(ash::wm::WmWindowAura, kWmWindowKey, nullptr);

namespace {

// A tentative class to set the bounds on the window.
// TODO(oshima): Once all logic is cleaned up, move this to the real layout
// manager with proper friendship.
class BoundsSetter : public aura::LayoutManager {
 public:
  BoundsSetter() {}
  ~BoundsSetter() override {}

  // aura::LayoutManager overrides:
  void OnWindowResized() override {}
  void OnWindowAddedToLayout(aura::Window* child) override {}
  void OnWillRemoveWindowFromLayout(aura::Window* child) override {}
  void OnWindowRemovedFromLayout(aura::Window* child) override {}
  void OnChildWindowVisibilityChanged(aura::Window* child,
                                      bool visible) override {}
  void SetChildBounds(aura::Window* child,
                      const gfx::Rect& requested_bounds) override {}

  void SetBounds(aura::Window* window, const gfx::Rect& bounds) {
    SetChildBoundsDirect(window, bounds);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(BoundsSetter);
};

}  // namespace

WmWindowAura::WmWindowAura(aura::Window* window)
    : window_(window),
      // Mirrors that of aura::Window.
      observers_(base::ObserverList<WmWindowObserver>::NOTIFY_EXISTING_ONLY) {
  window_->AddObserver(this);
  window_->SetProperty(kWmWindowKey, this);
}

// static
WmWindow* WmWindowAura::Get(aura::Window* window) {
  if (!window)
    return nullptr;

  WmWindow* wm_window = window->GetProperty(kWmWindowKey);
  if (wm_window)
    return wm_window;
  // WmWindowAura is owned by the aura::Window.
  return new WmWindowAura(window);
}

// static
std::vector<WmWindow*> WmWindowAura::FromAuraWindows(
    const std::vector<aura::Window*>& aura_windows) {
  std::vector<WmWindow*> result(aura_windows.size());
  for (size_t i = 0; i < aura_windows.size(); ++i)
    result[i] = Get(aura_windows[i]);
  return result;
}

// static
const aura::Window* WmWindowAura::GetAuraWindow(const WmWindow* wm_window) {
  return wm_window ? static_cast<const WmWindowAura*>(wm_window)->aura_window()
                   : nullptr;
}

const WmWindow* WmWindowAura::GetRootWindow() const {
  return Get(window_->GetRootWindow());
}

WmRootWindowController* WmWindowAura::GetRootWindowController() {
  aura::Window* root = window_->GetRootWindow();
  return root ? WmRootWindowControllerAura::Get(root) : nullptr;
}

WmGlobals* WmWindowAura::GetGlobals() const {
  return WmGlobals::Get();
}

void WmWindowAura::SetShellWindowId(int id) {
  window_->set_id(id);
}

int WmWindowAura::GetShellWindowId() const {
  return window_->id();
}

ui::wm::WindowType WmWindowAura::GetType() const {
  return window_->type();
}

ui::Layer* WmWindowAura::GetLayer() {
  return window_->layer();
}

display::Display WmWindowAura::GetDisplayNearestWindow() {
  return display::Screen::GetScreen()->GetDisplayNearestWindow(window_);
}

bool WmWindowAura::HasNonClientArea() {
  return window_->delegate() ? true : false;
}

int WmWindowAura::GetNonClientComponent(const gfx::Point& location) {
  return window_->delegate()
             ? window_->delegate()->GetNonClientComponent(location)
             : HTNOWHERE;
}

gfx::Point WmWindowAura::ConvertPointToTarget(const WmWindow* target,
                                              const gfx::Point& point) const {
  gfx::Point result(point);
  aura::Window::ConvertPointToTarget(window_, GetAuraWindow(target), &result);
  return result;
}

gfx::Point WmWindowAura::ConvertPointToScreen(const gfx::Point& point) const {
  gfx::Point result(point);
  ::wm::ConvertPointToScreen(window_, &result);
  return result;
}

gfx::Point WmWindowAura::ConvertPointFromScreen(const gfx::Point& point) const {
  gfx::Point result(point);
  ::wm::ConvertPointFromScreen(window_, &result);
  return result;
}

gfx::Rect WmWindowAura::ConvertRectToScreen(const gfx::Rect& rect) const {
  return ScreenUtil::ConvertRectToScreen(window_, rect);
}

gfx::Rect WmWindowAura::ConvertRectFromScreen(const gfx::Rect& rect) const {
  return ScreenUtil::ConvertRectFromScreen(window_, rect);
}

gfx::Size WmWindowAura::GetMinimumSize() const {
  return window_->delegate() ? window_->delegate()->GetMinimumSize()
                             : gfx::Size();
}

gfx::Size WmWindowAura::GetMaximumSize() const {
  return window_->delegate() ? window_->delegate()->GetMaximumSize()
                             : gfx::Size();
}

bool WmWindowAura::GetTargetVisibility() const {
  return window_->TargetVisibility();
}

bool WmWindowAura::IsVisible() const {
  return window_->IsVisible();
}

bool WmWindowAura::GetBoolProperty(WmWindowProperty key) {
  switch (key) {
    case WmWindowProperty::SNAP_CHILDREN_TO_PIXEL_BOUDARY:
      return window_->GetProperty(kSnapChildrenToPixelBoundary);

    case WmWindowProperty::ALWAYS_ON_TOP:
      return window_->GetProperty(aura::client::kAlwaysOnTopKey);

    default:
      NOTREACHED();
      break;
  }

  return false;
}

int WmWindowAura::GetIntProperty(WmWindowProperty key) {
  if (key == WmWindowProperty::SHELF_ID)
    return GetShelfIDForWindow(window_);

  NOTREACHED();
  return 0;
}

const WindowState* WmWindowAura::GetWindowState() const {
  return ash::wm::GetWindowState(window_);
}

WmWindow* WmWindowAura::GetToplevelWindow() {
  return Get(window_->GetToplevelWindow());
}

void WmWindowAura::SetParentUsingContext(WmWindow* context,
                                         const gfx::Rect& screen_bounds) {
  aura::client::ParentWindowWithContext(window_, GetAuraWindow(context),
                                        screen_bounds);
}

void WmWindowAura::AddChild(WmWindow* window) {
  window_->AddChild(GetAuraWindow(window));
}

WmWindow* WmWindowAura::GetParent() {
  return Get(window_->parent());
}

const WmWindow* WmWindowAura::GetTransientParent() const {
  return Get(::wm::GetTransientParent(window_));
}

std::vector<WmWindow*> WmWindowAura::GetTransientChildren() {
  return FromAuraWindows(::wm::GetTransientChildren(window_));
}

void WmWindowAura::SetLayoutManager(
    std::unique_ptr<WmLayoutManager> layout_manager) {
  // |window_| takes ownership of AuraLayoutManagerAdapter.
  window_->SetLayoutManager(
      new AuraLayoutManagerAdapter(std::move(layout_manager)));
}

WmLayoutManager* WmWindowAura::GetLayoutManager() {
  return static_cast<AuraLayoutManagerAdapter*>(window_->layout_manager())
      ->wm_layout_manager();
}

void WmWindowAura::SetVisibilityAnimationType(int type) {
  ::wm::SetWindowVisibilityAnimationType(window_, type);
}

void WmWindowAura::SetVisibilityAnimationDuration(base::TimeDelta delta) {
  ::wm::SetWindowVisibilityAnimationDuration(window_, delta);
}

void WmWindowAura::Animate(::wm::WindowAnimationType type) {
  ::wm::AnimateWindow(window_, type);
}

void WmWindowAura::SetBounds(const gfx::Rect& bounds) {
  window_->SetBounds(bounds);
}

void WmWindowAura::SetBoundsWithTransitionDelay(const gfx::Rect& bounds,
                                                base::TimeDelta delta) {
  if (::wm::WindowAnimationsDisabled(window_)) {
    window_->SetBounds(bounds);
    return;
  }

  ui::ScopedLayerAnimationSettings settings(window_->layer()->GetAnimator());
  settings.SetTransitionDuration(delta);
  window_->SetBounds(bounds);
}

void WmWindowAura::SetBoundsDirect(const gfx::Rect& bounds) {
  BoundsSetter().SetBounds(window_, bounds);
  SnapWindowToPixelBoundary(window_);
}

void WmWindowAura::SetBoundsDirectAnimated(const gfx::Rect& bounds) {
  const int kBoundsChangeSlideDurationMs = 120;

  ui::Layer* layer = window_->layer();
  ui::ScopedLayerAnimationSettings slide_settings(layer->GetAnimator());
  slide_settings.SetPreemptionStrategy(
      ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
  slide_settings.SetTransitionDuration(
      base::TimeDelta::FromMilliseconds(kBoundsChangeSlideDurationMs));
  SetBoundsDirect(bounds);
}

void WmWindowAura::SetBoundsDirectCrossFade(const gfx::Rect& bounds) {
  const gfx::Rect old_bounds = window_->bounds();

  // Create fresh layers for the window and all its children to paint into.
  // Takes ownership of the old layer and all its children, which will be
  // cleaned up after the animation completes.
  // Specify |set_bounds| to true here to keep the old bounds in the child
  // windows of |window|.
  std::unique_ptr<ui::LayerTreeOwner> old_layer_owner =
      ::wm::RecreateLayers(window_);
  ui::Layer* old_layer = old_layer_owner->root();
  DCHECK(old_layer);
  ui::Layer* new_layer = window_->layer();

  // Resize the window to the new size, which will force a layout and paint.
  SetBoundsDirect(bounds);

  // Ensure the higher-resolution layer is on top.
  bool old_on_top = (old_bounds.width() > bounds.width());
  if (old_on_top)
    old_layer->parent()->StackBelow(new_layer, old_layer);
  else
    old_layer->parent()->StackAbove(new_layer, old_layer);

  CrossFadeAnimation(window_, std::move(old_layer_owner), gfx::Tween::EASE_OUT);
}

void WmWindowAura::SetBoundsInScreen(const gfx::Rect& bounds_in_screen,
                                     const display::Display& dst_display) {
  window_->SetBoundsInScreen(bounds_in_screen, dst_display);
}

gfx::Rect WmWindowAura::GetBoundsInScreen() const {
  return window_->GetBoundsInScreen();
}

const gfx::Rect& WmWindowAura::GetBounds() const {
  return window_->bounds();
}

gfx::Rect WmWindowAura::GetTargetBounds() {
  return window_->GetTargetBounds();
}

void WmWindowAura::ClearRestoreBounds() {
  window_->ClearProperty(aura::client::kRestoreBoundsKey);
}

void WmWindowAura::SetRestoreBoundsInScreen(const gfx::Rect& bounds) {
  window_->SetProperty(aura::client::kRestoreBoundsKey, new gfx::Rect(bounds));
}

gfx::Rect WmWindowAura::GetRestoreBoundsInScreen() const {
  return *window_->GetProperty(aura::client::kRestoreBoundsKey);
}

bool WmWindowAura::Contains(const WmWindow* other) const {
  return other
             ? window_->Contains(
                   static_cast<const WmWindowAura*>(other)->window_)
             : false;
}

void WmWindowAura::SetShowState(ui::WindowShowState show_state) {
  window_->SetProperty(aura::client::kShowStateKey, show_state);
}

ui::WindowShowState WmWindowAura::GetShowState() const {
  return window_->GetProperty(aura::client::kShowStateKey);
}

void WmWindowAura::SetRestoreShowState(ui::WindowShowState show_state) {
  window_->SetProperty(aura::client::kRestoreShowStateKey, show_state);
}

void WmWindowAura::SetLockedToRoot(bool value) {
  window_->SetProperty(kStayInSameRootWindowKey, value);
}

void WmWindowAura::SetCapture() {
  window_->SetCapture();
}

bool WmWindowAura::HasCapture() {
  return window_->HasCapture();
}

void WmWindowAura::ReleaseCapture() {
  window_->ReleaseCapture();
}

bool WmWindowAura::HasRestoreBounds() const {
  return window_->GetProperty(aura::client::kRestoreBoundsKey) != nullptr;
}

bool WmWindowAura::CanMaximize() const {
  return window_->GetProperty(aura::client::kCanMaximizeKey);
}

bool WmWindowAura::CanMinimize() const {
  return window_->GetProperty(aura::client::kCanMinimizeKey);
}

bool WmWindowAura::CanResize() const {
  return window_->GetProperty(aura::client::kCanResizeKey);
}

bool WmWindowAura::CanActivate() const {
  return ::wm::CanActivateWindow(window_);
}

void WmWindowAura::StackChildAtTop(WmWindow* child) {
  window_->StackChildAtTop(GetAuraWindow(child));
}

void WmWindowAura::StackChildAtBottom(WmWindow* child) {
  window_->StackChildAtBottom(GetAuraWindow(child));
}

void WmWindowAura::StackChildAbove(WmWindow* child, WmWindow* target) {
  window_->StackChildAbove(GetAuraWindow(child), GetAuraWindow(target));
}

void WmWindowAura::StackChildBelow(WmWindow* child, WmWindow* target) {
  window_->StackChildBelow(GetAuraWindow(child), GetAuraWindow(target));
}

void WmWindowAura::SetAlwaysOnTop(bool value) {
  window_->SetProperty(aura::client::kAlwaysOnTopKey, value);
}

bool WmWindowAura::IsAlwaysOnTop() const {
  return window_->GetProperty(aura::client::kAlwaysOnTopKey);
}

void WmWindowAura::Hide() {
  window_->Hide();
}

void WmWindowAura::Show() {
  window_->Show();
}

bool WmWindowAura::IsFocused() const {
  return window_->HasFocus();
}

bool WmWindowAura::IsActive() const {
  return IsActiveWindow(window_);
}

void WmWindowAura::Activate() {
  ActivateWindow(window_);
}

void WmWindowAura::Deactivate() {
  DeactivateWindow(window_);
}

void WmWindowAura::SetFullscreen() {
  window_->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_FULLSCREEN);
}

void WmWindowAura::Maximize() {
  return window_->SetProperty(aura::client::kShowStateKey,
                              ui::SHOW_STATE_MAXIMIZED);
}

void WmWindowAura::Minimize() {
  window_->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_MINIMIZED);
}

void WmWindowAura::Unminimize() {
  window_->SetProperty(
      aura::client::kShowStateKey,
      window_->GetProperty(aura::client::kRestoreShowStateKey));
  window_->ClearProperty(aura::client::kRestoreShowStateKey);
}

std::vector<WmWindow*> WmWindowAura::GetChildren() {
  return FromAuraWindows(window_->children());
}

WmWindow* WmWindowAura::GetChildByShellWindowId(int id) {
  return Get(window_->GetChildById(id));
}

void WmWindowAura::SnapToPixelBoundaryIfNecessary() {
  SnapWindowToPixelBoundary(window_);
}

void WmWindowAura::AddObserver(WmWindowObserver* observer) {
  observers_.AddObserver(observer);
}

void WmWindowAura::RemoveObserver(WmWindowObserver* observer) {
  observers_.RemoveObserver(observer);
}

WmWindowAura::~WmWindowAura() {
  window_->RemoveObserver(this);
}

void WmWindowAura::OnWindowHierarchyChanged(
    const HierarchyChangeParams& params) {
  WmWindowObserver::TreeChangeParams wm_params;
  wm_params.target = Get(params.target);
  wm_params.new_parent = Get(params.new_parent);
  wm_params.old_parent = Get(params.old_parent);
  FOR_EACH_OBSERVER(WmWindowObserver, observers_,
                    OnWindowTreeChanged(this, wm_params));
}

void WmWindowAura::OnWindowStackingChanged(aura::Window* window) {
  FOR_EACH_OBSERVER(WmWindowObserver, observers_,
                    OnWindowStackingChanged(this));
}

void WmWindowAura::OnWindowPropertyChanged(aura::Window* window,
                                           const void* key,
                                           intptr_t old) {
  if (key == aura::client::kShowStateKey) {
    ash::wm::GetWindowState(window_)->OnWindowShowStateChanged();
    return;
  }
  WmWindowProperty wm_property;
  if (key == kSnapChildrenToPixelBoundary) {
    wm_property = WmWindowProperty::SNAP_CHILDREN_TO_PIXEL_BOUDARY;
  } else if (key == aura::client::kAlwaysOnTopKey) {
    wm_property = WmWindowProperty::ALWAYS_ON_TOP;
  } else if (key == kShelfID) {
    wm_property = WmWindowProperty::SHELF_ID;
  } else {
    return;
  }
  FOR_EACH_OBSERVER(WmWindowObserver, observers_,
                    OnWindowPropertyChanged(this, wm_property, old));
}

void WmWindowAura::OnWindowBoundsChanged(aura::Window* window,
                                         const gfx::Rect& old_bounds,
                                         const gfx::Rect& new_bounds) {
  FOR_EACH_OBSERVER(WmWindowObserver, observers_,
                    OnWindowBoundsChanged(this, old_bounds, new_bounds));
}

void WmWindowAura::OnWindowDestroying(aura::Window* window) {
  FOR_EACH_OBSERVER(WmWindowObserver, observers_, OnWindowDestroying(this));
}

void WmWindowAura::OnWindowVisibilityChanging(aura::Window* window,
                                              bool visible) {
  FOR_EACH_OBSERVER(WmWindowObserver, observers_,
                    OnWindowVisibilityChanging(this, visible));
}

}  // namespace wm
}  // namespace ash
