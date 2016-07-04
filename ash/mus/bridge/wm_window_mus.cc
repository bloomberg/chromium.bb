// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/mus/bridge/wm_window_mus.h"

#include "ash/common/wm/container_finder.h"
#include "ash/common/wm/window_state.h"
#include "ash/common/wm_layout_manager.h"
#include "ash/common/wm_window_observer.h"
#include "ash/common/wm_window_property.h"
#include "ash/mus/bridge/mus_layout_manager_adapter.h"
#include "ash/mus/bridge/wm_root_window_controller_mus.h"
#include "ash/mus/bridge/wm_shell_mus.h"
#include "ash/mus/property_util.h"
#include "services/ui/public/cpp/property_type_converters.h"
#include "services/ui/public/cpp/window.h"
#include "services/ui/public/cpp/window_property.h"
#include "services/ui/public/cpp/window_tree_client.h"
#include "services/ui/public/interfaces/window_manager.mojom.h"
#include "ui/aura/mus/mus_util.h"
#include "ui/base/hit_test.h"
#include "ui/display/display.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

MUS_DECLARE_WINDOW_PROPERTY_TYPE(ash::mus::WmWindowMus*);

// TODO(sky): fully implement this. Making DVLOG as too spammy to be useful.
#undef NOTIMPLEMENTED
#define NOTIMPLEMENTED() DVLOG(1) << "notimplemented"

namespace {

MUS_DEFINE_OWNED_WINDOW_PROPERTY_KEY(ash::mus::WmWindowMus,
                                     kWmWindowKey,
                                     nullptr);

}  // namespace

namespace ash {
namespace mus {

namespace {

// This classes is used so that the WindowState constructor can be made
// protected. GetWindowState() is the only place that should be creating
// WindowState.
class WindowStateMus : public wm::WindowState {
 public:
  explicit WindowStateMus(WmWindow* window) : wm::WindowState(window) {}
  ~WindowStateMus() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(WindowStateMus);
};

ui::WindowShowState UIWindowShowStateFromMojom(::ui::mojom::ShowState state) {
  switch (state) {
    case ::ui::mojom::ShowState::DEFAULT:
      return ui::SHOW_STATE_DEFAULT;
    case ::ui::mojom::ShowState::NORMAL:
      return ui::SHOW_STATE_NORMAL;
    case ::ui::mojom::ShowState::MINIMIZED:
      return ui::SHOW_STATE_MINIMIZED;
    case ::ui::mojom::ShowState::MAXIMIZED:
      return ui::SHOW_STATE_MAXIMIZED;
    case ::ui::mojom::ShowState::INACTIVE:
      return ui::SHOW_STATE_INACTIVE;
    case ::ui::mojom::ShowState::FULLSCREEN:
      return ui::SHOW_STATE_FULLSCREEN;
    case ::ui::mojom::ShowState::DOCKED:
      return ui::SHOW_STATE_DOCKED;
    default:
      break;
  }
  return ui::SHOW_STATE_DEFAULT;
}

::ui::mojom::ShowState MojomWindowShowStateFromUI(ui::WindowShowState state) {
  switch (state) {
    case ui::SHOW_STATE_DEFAULT:
      return ::ui::mojom::ShowState::DEFAULT;
    case ui::SHOW_STATE_NORMAL:
      return ::ui::mojom::ShowState::NORMAL;
    case ui::SHOW_STATE_MINIMIZED:
      return ::ui::mojom::ShowState::MINIMIZED;
    case ui::SHOW_STATE_MAXIMIZED:
      return ::ui::mojom::ShowState::MAXIMIZED;
    case ui::SHOW_STATE_INACTIVE:
      return ::ui::mojom::ShowState::INACTIVE;
    case ui::SHOW_STATE_FULLSCREEN:
      return ::ui::mojom::ShowState::FULLSCREEN;
    case ui::SHOW_STATE_DOCKED:
      return ::ui::mojom::ShowState::DOCKED;
    default:
      break;
  }
  return ::ui::mojom::ShowState::DEFAULT;
}

}  // namespace

WmWindowMus::WmWindowMus(::ui::Window* window)
    : window_(window),
      // Matches aura, see aura::Window for details.
      observers_(base::ObserverList<WmWindowObserver>::NOTIFY_EXISTING_ONLY) {
  window_->AddObserver(this);
  window_->SetLocalProperty(kWmWindowKey, this);
  window_state_.reset(new WindowStateMus(this));
}

WmWindowMus::~WmWindowMus() {
  window_->RemoveObserver(this);
}

// static
WmWindowMus* WmWindowMus::Get(::ui::Window* window) {
  if (!window)
    return nullptr;

  WmWindowMus* wm_window = window->GetLocalProperty(kWmWindowKey);
  if (wm_window)
    return wm_window;
  // WmWindowMus is owned by the ui::Window.
  return new WmWindowMus(window);
}

// static
WmWindowMus* WmWindowMus::Get(views::Widget* widget) {
  return WmWindowMus::Get(aura::GetMusWindow(widget->GetNativeView()));
}

// static
const ::ui::Window* WmWindowMus::GetMusWindow(const WmWindow* wm_window) {
  return static_cast<const WmWindowMus*>(wm_window)->mus_window();
}

// static
std::vector<WmWindow*> WmWindowMus::FromMusWindows(
    const std::vector<::ui::Window*>& mus_windows) {
  std::vector<WmWindow*> result(mus_windows.size());
  for (size_t i = 0; i < mus_windows.size(); ++i)
    result[i] = Get(mus_windows[i]);
  return result;
}

const WmRootWindowControllerMus* WmWindowMus::GetRootWindowControllerMus()
    const {
  return WmRootWindowControllerMus::Get(window_->GetRoot());
}

bool WmWindowMus::ShouldUseExtendedHitRegion() const {
  const WmWindowMus* parent = Get(window_->parent());
  return parent && parent->children_use_extended_hit_region_;
}

bool WmWindowMus::IsContainer() const {
  return GetShellWindowId() != kShellWindowId_Invalid;
}

const WmWindow* WmWindowMus::GetRootWindow() const {
  return Get(window_->GetRoot());
}

WmRootWindowController* WmWindowMus::GetRootWindowController() {
  return GetRootWindowControllerMus();
}

WmShell* WmWindowMus::GetShell() const {
  return WmShellMus::Get();
}

void WmWindowMus::SetName(const char* name) {
  if (name) {
    window_->SetSharedProperty<std::string>(
        ::ui::mojom::WindowManager::kName_Property, std::string(name));
  } else {
    window_->ClearSharedProperty(::ui::mojom::WindowManager::kName_Property);
  }
}

base::string16 WmWindowMus::GetTitle() const {
  return GetWindowTitle(window_);
}

void WmWindowMus::SetShellWindowId(int id) {
  shell_window_id_ = id;
}

int WmWindowMus::GetShellWindowId() const {
  return shell_window_id_;
}

ui::wm::WindowType WmWindowMus::GetType() const {
  return GetWmWindowType(window_);
}

ui::Layer* WmWindowMus::GetLayer() {
  // TODO(sky): this function should be nuked entirely.
  NOTIMPLEMENTED();
  return widget_ ? widget_->GetLayer() : nullptr;
}

display::Display WmWindowMus::GetDisplayNearestWindow() {
  // TODO(sky): deal with null rwc.
  return GetRootWindowControllerMus()->GetDisplay();
}

bool WmWindowMus::HasNonClientArea() {
  return widget_ ? true : false;
}

int WmWindowMus::GetNonClientComponent(const gfx::Point& location) {
  return widget_ ? widget_->GetNonClientComponent(location) : HTNOWHERE;
}

gfx::Point WmWindowMus::ConvertPointToTarget(const WmWindow* target,
                                             const gfx::Point& point) const {
  const ::ui::Window* target_window = GetMusWindow(target);
  if (target_window->Contains(window_)) {
    gfx::Point result(point);
    const ::ui::Window* window = window_;
    while (window != target_window) {
      result += window->bounds().origin().OffsetFromOrigin();
      window = window->parent();
    }
    return result;
  }
  if (window_->Contains(target_window)) {
    gfx::Point result(point);
    result -=
        target->ConvertPointToTarget(this, gfx::Point()).OffsetFromOrigin();
    return result;
  }
  // Different roots.
  gfx::Point point_in_screen =
      GetRootWindowControllerMus()->ConvertPointToScreen(this, point);
  return AsWmWindowMus(target)
      ->GetRootWindowControllerMus()
      ->ConvertPointFromScreen(AsWmWindowMus(target), point_in_screen);
}

gfx::Point WmWindowMus::ConvertPointToScreen(const gfx::Point& point) const {
  return GetRootWindowControllerMus()->ConvertPointToScreen(this, point);
}

gfx::Point WmWindowMus::ConvertPointFromScreen(const gfx::Point& point) const {
  return GetRootWindowControllerMus()->ConvertPointFromScreen(this, point);
}

gfx::Rect WmWindowMus::ConvertRectToScreen(const gfx::Rect& rect) const {
  return gfx::Rect(ConvertPointToScreen(rect.origin()), rect.size());
}

gfx::Rect WmWindowMus::ConvertRectFromScreen(const gfx::Rect& rect) const {
  return gfx::Rect(ConvertPointFromScreen(rect.origin()), rect.size());
}

gfx::Size WmWindowMus::GetMinimumSize() const {
  return widget_ ? widget_->GetMinimumSize() : gfx::Size();
}

gfx::Size WmWindowMus::GetMaximumSize() const {
  return widget_ ? widget_->GetMaximumSize() : gfx::Size();
}

bool WmWindowMus::GetTargetVisibility() const {
  // TODO: need animation support: http://crbug.com/615087.
  NOTIMPLEMENTED();
  return window_->visible();
}

bool WmWindowMus::IsVisible() const {
  return window_->visible();
}

void WmWindowMus::SetOpacity(float opacity) {
  window_->SetOpacity(opacity);
}

float WmWindowMus::GetTargetOpacity() const {
  // TODO: need animation support: http://crbug.com/615087.
  return window_->opacity();
}

void WmWindowMus::SetTransform(const gfx::Transform& transform) {
  // TODO: mus needs to support transforms: http://crbug.com/615089.
  NOTIMPLEMENTED();
}

gfx::Transform WmWindowMus::GetTargetTransform() const {
  // TODO: need animation support: http://crbug.com/615087.
  return gfx::Transform();
}

bool WmWindowMus::IsSystemModal() const {
  NOTIMPLEMENTED();
  return false;
}

bool WmWindowMus::GetBoolProperty(WmWindowProperty key) {
  switch (key) {
    case WmWindowProperty::SNAP_CHILDREN_TO_PIXEL_BOUNDARY:
      return snap_children_to_pixel_boundary_;

    case WmWindowProperty::ALWAYS_ON_TOP:
      return IsAlwaysOnTop();

    case WmWindowProperty::EXCLUDE_FROM_MRU:
      NOTIMPLEMENTED();
      return false;

    default:
      NOTREACHED();
      break;
  }

  NOTREACHED();
  return false;
}

int WmWindowMus::GetIntProperty(WmWindowProperty key) {
  if (key == WmWindowProperty::SHELF_ID) {
    NOTIMPLEMENTED();
    return 0;
  }

  if (key == WmWindowProperty::TOP_VIEW_INSET) {
    // TODO: need support for TOP_VIEW_INSET: http://crbug.com/615100.
    NOTIMPLEMENTED();
    return 0;
  }

  NOTREACHED();
  return 0;
}

const wm::WindowState* WmWindowMus::GetWindowState() const {
  return window_state_.get();
}

WmWindow* WmWindowMus::GetToplevelWindow() {
  return WmShellMus::GetToplevelAncestor(window_);
}

void WmWindowMus::SetParentUsingContext(WmWindow* context,
                                        const gfx::Rect& screen_bounds) {
  wm::GetDefaultParent(context, this, screen_bounds)->AddChild(this);
}

void WmWindowMus::AddChild(WmWindow* window) {
  window_->AddChild(GetMusWindow(window));
}

WmWindow* WmWindowMus::GetParent() {
  return Get(window_->parent());
}

const WmWindow* WmWindowMus::GetTransientParent() const {
  return Get(window_->transient_parent());
}

std::vector<WmWindow*> WmWindowMus::GetTransientChildren() {
  return FromMusWindows(window_->transient_children());
}

void WmWindowMus::SetLayoutManager(
    std::unique_ptr<WmLayoutManager> layout_manager) {
  if (layout_manager) {
    layout_manager_adapter_.reset(
        new MusLayoutManagerAdapter(window_, std::move(layout_manager)));
  } else {
    layout_manager_adapter_.reset();
  }
}

WmLayoutManager* WmWindowMus::GetLayoutManager() {
  return layout_manager_adapter_ ? layout_manager_adapter_->layout_manager()
                                 : nullptr;
}

void WmWindowMus::SetVisibilityAnimationType(int type) {
  // TODO: need animation support: http://crbug.com/615087.
  NOTIMPLEMENTED();
}

void WmWindowMus::SetVisibilityAnimationDuration(base::TimeDelta delta) {
  // TODO: need animation support: http://crbug.com/615087.
  NOTIMPLEMENTED();
}

void WmWindowMus::SetVisibilityAnimationTransition(
    ::wm::WindowVisibilityAnimationTransition transition) {
  // TODO: need animation support: http://crbug.com/615087.
  NOTIMPLEMENTED();
}

void WmWindowMus::Animate(::wm::WindowAnimationType type) {
  // TODO: need animation support: http://crbug.com/615087.
  NOTIMPLEMENTED();
}

void WmWindowMus::StopAnimatingProperty(
    ui::LayerAnimationElement::AnimatableProperty property) {
  // TODO: need animation support: http://crbug.com/615087.
  NOTIMPLEMENTED();
}

void WmWindowMus::SetChildWindowVisibilityChangesAnimated() {
  // TODO: need animation support: http://crbug.com/615087.
  NOTIMPLEMENTED();
}

void WmWindowMus::SetMasksToBounds(bool value) {
  // TODO: mus needs mask to bounds support: http://crbug.com/615550.
  NOTIMPLEMENTED();
}

void WmWindowMus::SetBounds(const gfx::Rect& bounds) {
  if (window_->parent()) {
    WmWindowMus* parent = WmWindowMus::Get(window_->parent());
    if (parent->layout_manager_adapter_) {
      parent->layout_manager_adapter_->layout_manager()->SetChildBounds(this,
                                                                        bounds);
      return;
    }
  }
  SetBoundsDirect(bounds);
}

void WmWindowMus::SetBoundsWithTransitionDelay(const gfx::Rect& bounds,
                                               base::TimeDelta delta) {
  // TODO: need animation support: http://crbug.com/615087.
  NOTIMPLEMENTED();
  SetBounds(bounds);
}

void WmWindowMus::SetBoundsDirect(const gfx::Rect& bounds) {
  window_->SetBounds(bounds);
  SnapToPixelBoundaryIfNecessary();
}

void WmWindowMus::SetBoundsDirectAnimated(const gfx::Rect& bounds) {
  // TODO: need animation support: http://crbug.com/615087.
  NOTIMPLEMENTED();
  SetBoundsDirect(bounds);
}

void WmWindowMus::SetBoundsDirectCrossFade(const gfx::Rect& bounds) {
  // TODO: need animation support: http://crbug.com/615087.
  NOTIMPLEMENTED();
  SetBoundsDirect(bounds);
}

void WmWindowMus::SetBoundsInScreen(const gfx::Rect& bounds_in_screen,
                                    const display::Display& dst_display) {
  // TODO: SetBoundsInScreen isn't fully implemented yet,
  // http://crbug.com/615552.
  NOTIMPLEMENTED();
  SetBounds(ConvertRectFromScreen(bounds_in_screen));
}

gfx::Rect WmWindowMus::GetBoundsInScreen() const {
  return ConvertRectToScreen(gfx::Rect(window_->bounds().size()));
}

const gfx::Rect& WmWindowMus::GetBounds() const {
  return window_->bounds();
}

gfx::Rect WmWindowMus::GetTargetBounds() {
  // TODO: need animation support: http://crbug.com/615087.
  NOTIMPLEMENTED();
  return window_->bounds();
}

void WmWindowMus::ClearRestoreBounds() {
  restore_bounds_in_screen_.reset();
}

void WmWindowMus::SetRestoreBoundsInScreen(const gfx::Rect& bounds) {
  restore_bounds_in_screen_.reset(new gfx::Rect(bounds));
}

gfx::Rect WmWindowMus::GetRestoreBoundsInScreen() const {
  return *restore_bounds_in_screen_;
}

bool WmWindowMus::Contains(const WmWindow* other) const {
  return other
             ? window_->Contains(
                   static_cast<const WmWindowMus*>(other)->window_)
             : false;
}

void WmWindowMus::SetShowState(ui::WindowShowState show_state) {
  SetWindowShowState(window_, MojomWindowShowStateFromUI(show_state));
}

ui::WindowShowState WmWindowMus::GetShowState() const {
  return UIWindowShowStateFromMojom(GetWindowShowState(window_));
}

void WmWindowMus::SetRestoreShowState(ui::WindowShowState show_state) {
  restore_show_state_ = show_state;
}

void WmWindowMus::SetRestoreOverrides(
    const gfx::Rect& bounds_override,
    ui::WindowShowState window_state_override) {
  // TODO(sky): see http://crbug.com/623314.
  NOTIMPLEMENTED();
}

void WmWindowMus::SetLockedToRoot(bool value) {
  // TODO(sky): there is no getter for this. Investigate where used.
  NOTIMPLEMENTED();
}

void WmWindowMus::SetCapture() {
  window_->SetCapture();
}

bool WmWindowMus::HasCapture() {
  return window_->HasCapture();
}

void WmWindowMus::ReleaseCapture() {
  window_->ReleaseCapture();
}

bool WmWindowMus::HasRestoreBounds() const {
  return restore_bounds_in_screen_.get() != nullptr;
}

bool WmWindowMus::CanMaximize() const {
  return widget_ ? widget_->widget_delegate()->CanMaximize() : false;
}

bool WmWindowMus::CanMinimize() const {
  return widget_ ? widget_->widget_delegate()->CanMinimize() : false;
}

bool WmWindowMus::CanResize() const {
  return widget_ ? widget_->widget_delegate()->CanResize() : false;
}

bool WmWindowMus::CanActivate() const {
  // TODO(sky): this isn't quite right. Should key off CanFocus(), which is not
  // replicated.
  return widget_ != nullptr;
}

void WmWindowMus::StackChildAtTop(WmWindow* child) {
  GetMusWindow(child)->MoveToFront();
}

void WmWindowMus::StackChildAtBottom(WmWindow* child) {
  GetMusWindow(child)->MoveToBack();
}

void WmWindowMus::StackChildAbove(WmWindow* child, WmWindow* target) {
  GetMusWindow(child)->Reorder(GetMusWindow(target),
                               ::ui::mojom::OrderDirection::ABOVE);
}

void WmWindowMus::StackChildBelow(WmWindow* child, WmWindow* target) {
  GetMusWindow(child)->Reorder(GetMusWindow(target),
                               ::ui::mojom::OrderDirection::BELOW);
}

void WmWindowMus::SetAlwaysOnTop(bool value) {
  mus::SetAlwaysOnTop(window_, value);
}

bool WmWindowMus::IsAlwaysOnTop() const {
  return mus::IsAlwaysOnTop(window_);
}

void WmWindowMus::Hide() {
  window_->SetVisible(false);
}

void WmWindowMus::Show() {
  window_->SetVisible(true);
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
  if (widget_creation_type_ == WidgetCreationType::FOR_CLIENT)
    window_->RequestClose();
  else
    widget_->Close();
}

bool WmWindowMus::IsFocused() const {
  return window_->HasFocus();
}

bool WmWindowMus::IsActive() const {
  ::ui::Window* focused = window_->window_tree()->GetFocusedWindow();
  return focused && window_->Contains(focused);
}

void WmWindowMus::Activate() {
  window_->SetFocus();
  WmWindow* top_level = GetToplevelWindow();
  if (!top_level)
    return;

  // TODO(sky): mus should do this too.
  GetMusWindow(top_level)->MoveToFront();
}

void WmWindowMus::Deactivate() {
  if (IsActive())
    window_->window_tree()->ClearFocus();
}

void WmWindowMus::SetFullscreen() {
  SetWindowShowState(window_, ::ui::mojom::ShowState::FULLSCREEN);
}

void WmWindowMus::Maximize() {
  SetWindowShowState(window_, ::ui::mojom::ShowState::MAXIMIZED);
}

void WmWindowMus::Minimize() {
  SetWindowShowState(window_, ::ui::mojom::ShowState::MINIMIZED);
}

void WmWindowMus::Unminimize() {
  SetWindowShowState(window_, MojomWindowShowStateFromUI(restore_show_state_));
  restore_show_state_ = ui::SHOW_STATE_DEFAULT;
}

void WmWindowMus::SetExcludedFromMru(bool excluded_from_mru) {
  NOTIMPLEMENTED();
}

std::vector<WmWindow*> WmWindowMus::GetChildren() {
  return FromMusWindows(window_->children());
}

WmWindow* WmWindowMus::GetChildByShellWindowId(int id) {
  if (id == shell_window_id_)
    return this;
  for (::ui::Window* child : window_->children()) {
    WmWindow* result = Get(child)->GetChildByShellWindowId(id);
    if (result)
      return result;
  }
  return nullptr;
}

void WmWindowMus::ShowResizeShadow(int component) {
  NOTIMPLEMENTED();
}

void WmWindowMus::HideResizeShadow() {
  NOTIMPLEMENTED();
}

void WmWindowMus::SetBoundsInScreenBehaviorForChildren(
    WmWindow::BoundsInScreenBehavior behavior) {
  // TODO: SetBoundsInScreen isn't fully implemented yet,
  // http://crbug.com/615552.
  NOTIMPLEMENTED();
}

void WmWindowMus::SetSnapsChildrenToPhysicalPixelBoundary() {
  if (snap_children_to_pixel_boundary_)
    return;

  snap_children_to_pixel_boundary_ = true;
  FOR_EACH_OBSERVER(
      WmWindowObserver, observers_,
      OnWindowPropertyChanged(
          this, WmWindowProperty::SNAP_CHILDREN_TO_PIXEL_BOUNDARY));
}

void WmWindowMus::SnapToPixelBoundaryIfNecessary() {
  WmWindowMus* parent = Get(window_->parent());
  if (parent && parent->snap_children_to_pixel_boundary_) {
    // TODO: implement snap to pixel: http://crbug.com/615554.
    NOTIMPLEMENTED();
  }
}

void WmWindowMus::SetChildrenUseExtendedHitRegion() {
  children_use_extended_hit_region_ = true;
}

void WmWindowMus::SetDescendantsStayInSameRootWindow(bool value) {
  // TODO: this logic feeds into SetBoundsInScreen(), which is not implemented:
  // http://crbug.com/615552.
  NOTIMPLEMENTED();
}

void WmWindowMus::AddObserver(WmWindowObserver* observer) {
  observers_.AddObserver(observer);
}

void WmWindowMus::RemoveObserver(WmWindowObserver* observer) {
  observers_.RemoveObserver(observer);
}

bool WmWindowMus::HasObserver(const WmWindowObserver* observer) const {
  return observers_.HasObserver(observer);
}

void WmWindowMus::OnTreeChanging(const TreeChangeParams& params) {
  WmWindowObserver::TreeChangeParams wm_params;
  wm_params.target = Get(params.target);
  wm_params.new_parent = Get(params.new_parent);
  wm_params.old_parent = Get(params.old_parent);
  FOR_EACH_OBSERVER(WmWindowObserver, observers_,
                    OnWindowTreeChanging(this, wm_params));
}

void WmWindowMus::OnTreeChanged(const TreeChangeParams& params) {
  WmWindowObserver::TreeChangeParams wm_params;
  wm_params.target = Get(params.target);
  wm_params.new_parent = Get(params.new_parent);
  wm_params.old_parent = Get(params.old_parent);
  FOR_EACH_OBSERVER(WmWindowObserver, observers_,
                    OnWindowTreeChanged(this, wm_params));
}

void WmWindowMus::OnWindowReordered(::ui::Window* window,
                                    ::ui::Window* relative_window,
                                    ::ui::mojom::OrderDirection direction) {
  FOR_EACH_OBSERVER(WmWindowObserver, observers_,
                    OnWindowStackingChanged(this));
}

void WmWindowMus::OnWindowSharedPropertyChanged(
    ::ui::Window* window,
    const std::string& name,
    const std::vector<uint8_t>* old_data,
    const std::vector<uint8_t>* new_data) {
  if (name == ::ui::mojom::WindowManager::kShowState_Property) {
    GetWindowState()->OnWindowShowStateChanged();
    return;
  }
  if (name == ::ui::mojom::WindowManager::kAlwaysOnTop_Property) {
    FOR_EACH_OBSERVER(
        WmWindowObserver, observers_,
        OnWindowPropertyChanged(this, WmWindowProperty::ALWAYS_ON_TOP));
    return;
  }

  // Deal with snap to pixel.
  NOTIMPLEMENTED();
}

void WmWindowMus::OnWindowBoundsChanged(::ui::Window* window,
                                        const gfx::Rect& old_bounds,
                                        const gfx::Rect& new_bounds) {
  FOR_EACH_OBSERVER(WmWindowObserver, observers_,
                    OnWindowBoundsChanged(this, old_bounds, new_bounds));
}

void WmWindowMus::OnWindowDestroying(::ui::Window* window) {
  FOR_EACH_OBSERVER(WmWindowObserver, observers_, OnWindowDestroying(this));
}

void WmWindowMus::OnWindowDestroyed(::ui::Window* window) {
  FOR_EACH_OBSERVER(WmWindowObserver, observers_, OnWindowDestroyed(this));
}

}  // namespace mus
}  // namespace ash
