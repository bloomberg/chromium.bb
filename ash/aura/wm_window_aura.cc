// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/aura/wm_window_aura.h"

#include "ash/aura/aura_layout_manager_adapter.h"
#include "ash/aura/wm_root_window_controller_aura.h"
#include "ash/aura/wm_shell_aura.h"
#include "ash/common/ash_constants.h"
#include "ash/common/shelf/shelf_item_types.h"
#include "ash/common/wm/window_state.h"
#include "ash/common/wm_layout_manager.h"
#include "ash/common/wm_transient_window_observer.h"
#include "ash/common/wm_window_observer.h"
#include "ash/common/wm_window_property.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/screen_util.h"
#include "ash/shell.h"
#include "ash/wm/resize_handle_window_targeter.h"
#include "ash/wm/resize_shadow_controller.h"
#include "ash/wm/window_animations.h"
#include "ash/wm/window_mirror_view.h"
#include "ash/wm/window_properties.h"
#include "ash/wm/window_state_aura.h"
#include "ash/wm/window_util.h"
#include "base/memory/ptr_util.h"
#include "services/ui/public/interfaces/window_manager_constants.mojom.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/focus_client.h"
#include "ui/aura/client/window_parenting_client.h"
#include "ui/aura/layout_manager.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"
#include "ui/aura/window_property.h"
#include "ui/base/hit_test.h"
#include "ui/compositor/layer_tree_owner.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/wm/core/coordinate_conversion.h"
#include "ui/wm/core/easy_resize_window_targeter.h"
#include "ui/wm/core/transient_window_manager.h"
#include "ui/wm/core/visibility_controller.h"
#include "ui/wm/core/window_util.h"

DECLARE_WINDOW_PROPERTY_TYPE(ash::WmWindowAura*);

namespace ash {

DEFINE_OWNED_WINDOW_PROPERTY_KEY(WmWindowAura, kWmWindowKey, nullptr);

static_assert(aura::Window::kInitialId == kShellWindowId_Invalid,
              "ids must match");

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

WmWindowAura::~WmWindowAura() {
  if (added_transient_observer_)
    ::wm::TransientWindowManager::Get(window_)->RemoveObserver(this);

  window_->RemoveObserver(this);
}

// static
const WmWindow* WmWindowAura::Get(const aura::Window* window) {
  if (!window)
    return nullptr;

  const WmWindow* wm_window = window->GetProperty(kWmWindowKey);
  if (wm_window)
    return wm_window;
  // WmWindowAura is owned by the aura::Window.
  // TODO(sky): fix constness.
  return new WmWindowAura(const_cast<aura::Window*>(window));
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
std::vector<aura::Window*> WmWindowAura::ToAuraWindows(
    const std::vector<WmWindow*>& windows) {
  std::vector<aura::Window*> result(windows.size());
  for (size_t i = 0; i < windows.size(); ++i)
    result[i] = WmWindowAura::GetAuraWindow(windows[i]);
  return result;
}

// static
const aura::Window* WmWindowAura::GetAuraWindow(const WmWindow* wm_window) {
  return wm_window ? static_cast<const WmWindowAura*>(wm_window)->aura_window()
                   : nullptr;
}

void WmWindowAura::Destroy() {
  delete window_;
  // WARNING: this has been deleted.
}

const WmWindow* WmWindowAura::GetRootWindow() const {
  return Get(window_->GetRootWindow());
}

WmRootWindowController* WmWindowAura::GetRootWindowController() {
  aura::Window* root = window_->GetRootWindow();
  return root ? WmRootWindowControllerAura::Get(root) : nullptr;
}

WmShell* WmWindowAura::GetShell() const {
  return WmShell::Get();
}

void WmWindowAura::SetName(const char* name) {
  window_->SetName(name);
}

std::string WmWindowAura::GetName() const {
  return window_->GetName();
}

void WmWindowAura::SetTitle(const base::string16& title) {
  window_->SetTitle(title);
}

base::string16 WmWindowAura::GetTitle() const {
  return window_->GetTitle();
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

int WmWindowAura::GetAppType() const {
  return window_->GetProperty(aura::client::kAppType);
}

void WmWindowAura::SetAppType(int app_type) const {
  window_->SetProperty(aura::client::kAppType, app_type);
}

bool WmWindowAura::IsBubble() {
  views::Widget* widget = views::Widget::GetWidgetForNativeView(window_);
  return widget->widget_delegate()->AsBubbleDialogDelegate() != nullptr;
}

ui::Layer* WmWindowAura::GetLayer() {
  return window_->layer();
}

bool WmWindowAura::GetLayerTargetVisibility() {
  return GetLayer()->GetTargetVisibility();
}

bool WmWindowAura::GetLayerVisible() {
  return GetLayer()->visible();
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

void WmWindowAura::SetOpacity(float opacity) {
  window_->layer()->SetOpacity(opacity);
}

float WmWindowAura::GetTargetOpacity() const {
  return window_->layer()->GetTargetOpacity();
}

gfx::Rect WmWindowAura::GetMinimizeAnimationTargetBoundsInScreen() const {
  return ash::GetMinimizeAnimationTargetBoundsInScreen(window_);
}

void WmWindowAura::SetTransform(const gfx::Transform& transform) {
  window_->SetTransform(transform);
}

gfx::Transform WmWindowAura::GetTargetTransform() const {
  return window_->layer()->GetTargetTransform();
}

bool WmWindowAura::IsSystemModal() const {
  return window_->GetProperty(aura::client::kModalKey) == ui::MODAL_TYPE_SYSTEM;
}

bool WmWindowAura::GetBoolProperty(WmWindowProperty key) {
  switch (key) {
    case WmWindowProperty::DRAW_ATTENTION:
      return window_->GetProperty(aura::client::kDrawAttentionKey);

    case WmWindowProperty::SNAP_CHILDREN_TO_PIXEL_BOUNDARY:
      return window_->GetProperty(kSnapChildrenToPixelBoundary);

    case WmWindowProperty::ALWAYS_ON_TOP:
      return window_->GetProperty(aura::client::kAlwaysOnTopKey);

    case WmWindowProperty::EXCLUDE_FROM_MRU:
      return window_->GetProperty(aura::client::kExcludeFromMruKey);

    default:
      NOTREACHED();
      break;
  }

  return false;
}

SkColor WmWindowAura::GetColorProperty(WmWindowProperty key) {
  if (key == WmWindowProperty::TOP_VIEW_COLOR)
    return window_->GetProperty(aura::client::kTopViewColor);

  NOTREACHED();
  return 0;
}

void WmWindowAura::SetColorProperty(WmWindowProperty key, SkColor value) {
  if (key == WmWindowProperty::TOP_VIEW_COLOR) {
    window_->SetProperty(aura::client::kTopViewColor, value);
    return;
  }

  NOTREACHED();
}

int WmWindowAura::GetIntProperty(WmWindowProperty key) {
  if (key == WmWindowProperty::MODAL_TYPE)
    return window_->GetProperty(aura::client::kModalKey);

  if (key == WmWindowProperty::SHELF_ID)
    return window_->GetProperty(kShelfIDKey);

  if (key == WmWindowProperty::SHELF_ITEM_TYPE)
    return window_->GetProperty(kShelfItemTypeKey);

  if (key == WmWindowProperty::TOP_VIEW_INSET)
    return window_->GetProperty(aura::client::kTopViewInset);

  NOTREACHED();
  return 0;
}

void WmWindowAura::SetIntProperty(WmWindowProperty key, int value) {
  if (key == WmWindowProperty::SHELF_ID) {
    window_->SetProperty(kShelfIDKey, value);
    return;
  }
  if (key == WmWindowProperty::SHELF_ITEM_TYPE) {
    window_->SetProperty(kShelfItemTypeKey, value);
    return;
  }
  if (key == WmWindowProperty::TOP_VIEW_INSET) {
    window_->SetProperty(aura::client::kTopViewInset, value);
    return;
  }

  NOTREACHED();
}

std::string WmWindowAura::GetStringProperty(WmWindowProperty key) {
  if (key == WmWindowProperty::APP_ID) {
    std::string* value = window_->GetProperty(aura::client::kAppIdKey);
    return value ? *value : std::string();
  }

  NOTREACHED();
  return std::string();
}

void WmWindowAura::SetStringProperty(WmWindowProperty key,
                                     const std::string& value) {
  if (key == WmWindowProperty::APP_ID) {
    window_->SetProperty(aura::client::kAppIdKey, new std::string(value));
    return;
  }

  NOTREACHED();
}

gfx::ImageSkia WmWindowAura::GetWindowIcon() {
  gfx::ImageSkia* image = window_->GetProperty(aura::client::kWindowIconKey);
  return image ? *image : gfx::ImageSkia();
}

gfx::ImageSkia WmWindowAura::GetAppIcon() {
  gfx::ImageSkia* image = window_->GetProperty(aura::client::kAppIconKey);
  return image ? *image : gfx::ImageSkia();
}

const wm::WindowState* WmWindowAura::GetWindowState() const {
  return ash::wm::GetWindowState(window_);
}

WmWindow* WmWindowAura::GetToplevelWindow() {
  return Get(window_->GetToplevelWindow());
}

WmWindow* WmWindowAura::GetToplevelWindowForFocus() {
  return Get(::wm::GetToplevelWindow(window_));
}

void WmWindowAura::SetParentUsingContext(WmWindow* context,
                                         const gfx::Rect& screen_bounds) {
  aura::client::ParentWindowWithContext(window_, GetAuraWindow(context),
                                        screen_bounds);
}

void WmWindowAura::AddChild(WmWindow* window) {
  window_->AddChild(GetAuraWindow(window));
}

void WmWindowAura::RemoveChild(WmWindow* child) {
  window_->RemoveChild(GetAuraWindow(child));
}

const WmWindow* WmWindowAura::GetParent() const {
  return Get(window_->parent());
}

const WmWindow* WmWindowAura::GetTransientParent() const {
  return Get(::wm::GetTransientParent(window_));
}

std::vector<WmWindow*> WmWindowAura::GetTransientChildren() {
  return FromAuraWindows(::wm::GetTransientChildren(window_));
}

bool WmWindowAura::MoveToEventRoot(const ui::Event& event) {
  return ash::wm::MoveWindowToEventRoot(window_, event);
}

void WmWindowAura::SetLayoutManager(
    std::unique_ptr<WmLayoutManager> layout_manager) {
  // See ~AuraLayoutManagerAdapter for why SetLayoutManager(nullptr) is called.
  window_->SetLayoutManager(nullptr);
  if (!layout_manager)
    return;

  // |window_| takes ownership of AuraLayoutManagerAdapter.
  window_->SetLayoutManager(
      new AuraLayoutManagerAdapter(window_, std::move(layout_manager)));
}

WmLayoutManager* WmWindowAura::GetLayoutManager() {
  AuraLayoutManagerAdapter* adapter = AuraLayoutManagerAdapter::Get(window_);
  return adapter ? adapter->wm_layout_manager() : nullptr;
}

void WmWindowAura::SetVisibilityChangesAnimated() {
  ::wm::SetWindowVisibilityChangesAnimated(window_);
}

void WmWindowAura::SetVisibilityAnimationType(int type) {
  ::wm::SetWindowVisibilityAnimationType(window_, type);
}

void WmWindowAura::SetVisibilityAnimationDuration(base::TimeDelta delta) {
  ::wm::SetWindowVisibilityAnimationDuration(window_, delta);
}

void WmWindowAura::SetVisibilityAnimationTransition(
    ::wm::WindowVisibilityAnimationTransition transition) {
  ::wm::SetWindowVisibilityAnimationTransition(window_, transition);
}

void WmWindowAura::Animate(::wm::WindowAnimationType type) {
  ::wm::AnimateWindow(window_, type);
}

void WmWindowAura::StopAnimatingProperty(
    ui::LayerAnimationElement::AnimatableProperty property) {
  window_->layer()->GetAnimator()->StopAnimatingProperty(property);
}

void WmWindowAura::SetChildWindowVisibilityChangesAnimated() {
  ::wm::SetChildWindowVisibilityChangesAnimated(window_);
}

void WmWindowAura::SetMasksToBounds(bool value) {
  window_->layer()->SetMasksToBounds(value);
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
  wm::SnapWindowToPixelBoundary(window_);
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
  gfx::Rect* bounds = window_->GetProperty(aura::client::kRestoreBoundsKey);
  return bounds ? *bounds : gfx::Rect();
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

void WmWindowAura::SetRestoreOverrides(
    const gfx::Rect& bounds_override,
    ui::WindowShowState window_state_override) {
  if (bounds_override.IsEmpty()) {
    window_->ClearProperty(kRestoreShowStateOverrideKey);
    window_->ClearProperty(kRestoreBoundsOverrideKey);
    return;
  }
  window_->SetProperty(kRestoreShowStateOverrideKey, window_state_override);
  window_->SetProperty(kRestoreBoundsOverrideKey,
                       new gfx::Rect(bounds_override));
}

void WmWindowAura::SetLockedToRoot(bool value) {
  window_->SetProperty(kLockedToRootKey, value);
}

bool WmWindowAura::IsLockedToRoot() const {
  return window_->GetProperty(kLockedToRootKey);
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
  return (window_->GetProperty(aura::client::kResizeBehaviorKey) &
          ui::mojom::kResizeBehaviorCanMaximize) != 0;
}

bool WmWindowAura::CanMinimize() const {
  return (window_->GetProperty(aura::client::kResizeBehaviorKey) &
          ui::mojom::kResizeBehaviorCanMinimize) != 0;
}

bool WmWindowAura::CanResize() const {
  return (window_->GetProperty(aura::client::kResizeBehaviorKey) &
          ui::mojom::kResizeBehaviorCanResize) != 0;
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

void WmWindowAura::SetPinned(bool trusted) {
  wm::PinWindow(window_, trusted);
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

views::Widget* WmWindowAura::GetInternalWidget() {
  return views::Widget::GetWidgetForNativeView(window_);
}

void WmWindowAura::CloseWidget() {
  DCHECK(GetInternalWidget());
  GetInternalWidget()->Close();
}

void WmWindowAura::SetFocused() {
  aura::client::GetFocusClient(window_)->FocusWindow(window_);
}

bool WmWindowAura::IsFocused() const {
  return window_->HasFocus();
}

bool WmWindowAura::IsActive() const {
  return wm::IsActiveWindow(window_);
}

void WmWindowAura::Activate() {
  wm::ActivateWindow(window_);
}

void WmWindowAura::Deactivate() {
  wm::DeactivateWindow(window_);
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

void WmWindowAura::SetExcludedFromMru(bool excluded_from_mru) {
  window_->SetProperty(aura::client::kExcludeFromMruKey, excluded_from_mru);
}

std::vector<WmWindow*> WmWindowAura::GetChildren() {
  return FromAuraWindows(window_->children());
}

WmWindow* WmWindowAura::GetChildByShellWindowId(int id) {
  return Get(window_->GetChildById(id));
}

void WmWindowAura::ShowResizeShadow(int component) {
  ResizeShadowController* resize_shadow_controller =
      Shell::GetInstance()->resize_shadow_controller();
  if (resize_shadow_controller)
    resize_shadow_controller->ShowShadow(window_, component);
}

void WmWindowAura::HideResizeShadow() {
  ResizeShadowController* resize_shadow_controller =
      Shell::GetInstance()->resize_shadow_controller();
  if (resize_shadow_controller)
    resize_shadow_controller->HideShadow(window_);
}

void WmWindowAura::InstallResizeHandleWindowTargeter(
    ImmersiveFullscreenController* immersive_fullscreen_controller) {
  window_->SetEventTargeter(base::MakeUnique<ResizeHandleWindowTargeter>(
      window_, immersive_fullscreen_controller));
}

void WmWindowAura::SetBoundsInScreenBehaviorForChildren(
    BoundsInScreenBehavior behavior) {
  window_->SetProperty(
      kUsesScreenCoordinatesKey,
      behavior == BoundsInScreenBehavior::USE_SCREEN_COORDINATES);
}

void WmWindowAura::SetSnapsChildrenToPhysicalPixelBoundary() {
  wm::SetSnapsChildrenToPhysicalPixelBoundary(window_);
}

void WmWindowAura::SnapToPixelBoundaryIfNecessary() {
  wm::SnapWindowToPixelBoundary(window_);
}

void WmWindowAura::SetChildrenUseExtendedHitRegion() {
  gfx::Insets mouse_extend(-kResizeOutsideBoundsSize, -kResizeOutsideBoundsSize,
                           -kResizeOutsideBoundsSize,
                           -kResizeOutsideBoundsSize);
  gfx::Insets touch_extend =
      mouse_extend.Scale(kResizeOutsideBoundsScaleForTouch);
  window_->SetEventTargeter(base::MakeUnique<::wm::EasyResizeWindowTargeter>(
      window_, mouse_extend, touch_extend));
}

std::unique_ptr<views::View> WmWindowAura::CreateViewWithRecreatedLayers() {
  return base::MakeUnique<wm::WindowMirrorView>(this);
}

void WmWindowAura::AddObserver(WmWindowObserver* observer) {
  observers_.AddObserver(observer);
}

void WmWindowAura::RemoveObserver(WmWindowObserver* observer) {
  observers_.RemoveObserver(observer);
}

bool WmWindowAura::HasObserver(const WmWindowObserver* observer) const {
  return observers_.HasObserver(observer);
}

void WmWindowAura::AddTransientWindowObserver(
    WmTransientWindowObserver* observer) {
  if (!added_transient_observer_) {
    added_transient_observer_ = true;
    ::wm::TransientWindowManager::Get(window_)->AddObserver(this);
  }
  transient_observers_.AddObserver(observer);
}

void WmWindowAura::RemoveTransientWindowObserver(
    WmTransientWindowObserver* observer) {
  transient_observers_.RemoveObserver(observer);
  if (added_transient_observer_ &&
      !transient_observers_.might_have_observers()) {
    added_transient_observer_ = false;
    ::wm::TransientWindowManager::Get(window_)->RemoveObserver(this);
  }
}

void WmWindowAura::AddLimitedPreTargetHandler(ui::EventHandler* handler) {
  // This behaves differently from WmWindowMus for child and embedded windows.
  window_->AddPreTargetHandler(handler);
}

void WmWindowAura::RemoveLimitedPreTargetHandler(ui::EventHandler* handler) {
  window_->RemovePreTargetHandler(handler);
}

void WmWindowAura::OnWindowHierarchyChanging(
    const HierarchyChangeParams& params) {
  WmWindowObserver::TreeChangeParams wm_params;
  wm_params.target = Get(params.target);
  wm_params.new_parent = Get(params.new_parent);
  wm_params.old_parent = Get(params.old_parent);
  for (auto& observer : observers_)
    observer.OnWindowTreeChanging(this, wm_params);
}

void WmWindowAura::OnWindowHierarchyChanged(
    const HierarchyChangeParams& params) {
  WmWindowObserver::TreeChangeParams wm_params;
  wm_params.target = Get(params.target);
  wm_params.new_parent = Get(params.new_parent);
  wm_params.old_parent = Get(params.old_parent);
  for (auto& observer : observers_)
    observer.OnWindowTreeChanged(this, wm_params);
}

void WmWindowAura::OnWindowStackingChanged(aura::Window* window) {
  for (auto& observer : observers_)
    observer.OnWindowStackingChanged(this);
}

void WmWindowAura::OnWindowPropertyChanged(aura::Window* window,
                                           const void* key,
                                           intptr_t old) {
  if (key == aura::client::kShowStateKey) {
    ash::wm::GetWindowState(window_)->OnWindowShowStateChanged();
    return;
  }
  WmWindowProperty wm_property;
  if (key == aura::client::kAlwaysOnTopKey) {
    wm_property = WmWindowProperty::ALWAYS_ON_TOP;
  } else if (key == aura::client::kAppIconKey) {
    wm_property = WmWindowProperty::APP_ICON;
  } else if (key == aura::client::kDrawAttentionKey) {
    wm_property = WmWindowProperty::DRAW_ATTENTION;
  } else if (key == aura::client::kExcludeFromMruKey) {
    wm_property = WmWindowProperty::EXCLUDE_FROM_MRU;
  } else if (key == aura::client::kModalKey) {
    wm_property = WmWindowProperty::MODAL_TYPE;
  } else if (key == kShelfIDKey) {
    wm_property = WmWindowProperty::SHELF_ID;
  } else if (key == kShelfItemTypeKey) {
    wm_property = WmWindowProperty::SHELF_ITEM_TYPE;
  } else if (key == kSnapChildrenToPixelBoundary) {
    wm_property = WmWindowProperty::SNAP_CHILDREN_TO_PIXEL_BOUNDARY;
  } else if (key == aura::client::kTopViewInset) {
    wm_property = WmWindowProperty::TOP_VIEW_INSET;
  } else if (key == aura::client::kWindowIconKey) {
    wm_property = WmWindowProperty::WINDOW_ICON;
  } else {
    return;
  }
  for (auto& observer : observers_)
    observer.OnWindowPropertyChanged(this, wm_property);
}

void WmWindowAura::OnWindowBoundsChanged(aura::Window* window,
                                         const gfx::Rect& old_bounds,
                                         const gfx::Rect& new_bounds) {
  for (auto& observer : observers_)
    observer.OnWindowBoundsChanged(this, old_bounds, new_bounds);
}

void WmWindowAura::OnWindowDestroying(aura::Window* window) {
  for (auto& observer : observers_)
    observer.OnWindowDestroying(this);
}

void WmWindowAura::OnWindowDestroyed(aura::Window* window) {
  for (auto& observer : observers_)
    observer.OnWindowDestroyed(this);
}

void WmWindowAura::OnWindowVisibilityChanging(aura::Window* window,
                                              bool visible) {
  DCHECK_EQ(window, window_);
  for (auto& observer : observers_)
    observer.OnWindowVisibilityChanging(this, visible);
}

void WmWindowAura::OnWindowVisibilityChanged(aura::Window* window,
                                             bool visible) {
  for (auto& observer : observers_)
    observer.OnWindowVisibilityChanged(Get(window), visible);
}

void WmWindowAura::OnWindowTitleChanged(aura::Window* window) {
  for (auto& observer : observers_)
    observer.OnWindowTitleChanged(this);
}

void WmWindowAura::OnTransientChildAdded(aura::Window* window,
                                         aura::Window* transient) {
  for (auto& observer : transient_observers_)
    observer.OnTransientChildAdded(this, Get(transient));
}

void WmWindowAura::OnTransientChildRemoved(aura::Window* window,
                                           aura::Window* transient) {
  for (auto& observer : transient_observers_)
    observer.OnTransientChildRemoved(this, Get(transient));
}

}  // namespace ash
