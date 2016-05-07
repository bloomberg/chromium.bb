// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mash/wm/bridge/wm_window_mus.h"

#include "ash/wm/common/container_finder.h"
#include "ash/wm/common/window_state.h"
#include "ash/wm/common/wm_layout_manager.h"
#include "ash/wm/common/wm_window_observer.h"
#include "ash/wm/common/wm_window_property.h"
#include "components/mus/public/cpp/window.h"
#include "components/mus/public/cpp/window_property.h"
#include "components/mus/public/cpp/window_tree_connection.h"
#include "mash/wm/bridge/mus_layout_manager_adapter.h"
#include "mash/wm/bridge/wm_globals_mus.h"
#include "mash/wm/bridge/wm_root_window_controller_mus.h"
#include "mash/wm/property_util.h"
#include "ui/aura/mus/mus_util.h"
#include "ui/base/hit_test.h"
#include "ui/gfx/display.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

MUS_DECLARE_WINDOW_PROPERTY_TYPE(mash::wm::WmWindowMus*);

namespace mash {
namespace wm {

MUS_DEFINE_OWNED_WINDOW_PROPERTY_KEY(mash::wm::WmWindowMus,
                                     kWmWindowKey,
                                     nullptr);
MUS_DEFINE_LOCAL_WINDOW_PROPERTY_KEY(int, kShellWindowIdKey, -1);

namespace {

mus::Window* GetMusWindowByShellId(mus::Window* window, int id) {
  if (window->GetLocalProperty(kShellWindowIdKey) == id)
    return window;

  for (mus::Window* child : window->children()) {
    mus::Window* result = GetMusWindowByShellId(child, id);
    if (result)
      return result;
  }
  return nullptr;
}

// This classes is used so that the WindowState constructor can be made
// protected. GetWindowState() is the only place that should be creating
// WindowState.
class WindowStateMus : public ash::wm::WindowState {
 public:
  explicit WindowStateMus(ash::wm::WmWindow* window)
      : ash::wm::WindowState(window) {}
  ~WindowStateMus() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(WindowStateMus);
};

ui::WindowShowState UIWindowShowStateFromMojom(mus::mojom::ShowState state) {
  switch (state) {
    case mus::mojom::ShowState::DEFAULT:
      return ui::SHOW_STATE_DEFAULT;
    case mus::mojom::ShowState::NORMAL:
      return ui::SHOW_STATE_NORMAL;
    case mus::mojom::ShowState::MINIMIZED:
      return ui::SHOW_STATE_MINIMIZED;
    case mus::mojom::ShowState::MAXIMIZED:
      return ui::SHOW_STATE_MAXIMIZED;
    case mus::mojom::ShowState::INACTIVE:
      return ui::SHOW_STATE_INACTIVE;
    case mus::mojom::ShowState::FULLSCREEN:
      return ui::SHOW_STATE_FULLSCREEN;
    case mus::mojom::ShowState::DOCKED:
      return ui::SHOW_STATE_DOCKED;
    default:
      break;
  }
  return ui::SHOW_STATE_DEFAULT;
}

mus::mojom::ShowState MojomWindowShowStateFromUI(ui::WindowShowState state) {
  switch (state) {
    case ui::SHOW_STATE_DEFAULT:
      return mus::mojom::ShowState::DEFAULT;
    case ui::SHOW_STATE_NORMAL:
      return mus::mojom::ShowState::NORMAL;
    case ui::SHOW_STATE_MINIMIZED:
      return mus::mojom::ShowState::MINIMIZED;
    case ui::SHOW_STATE_MAXIMIZED:
      return mus::mojom::ShowState::MAXIMIZED;
    case ui::SHOW_STATE_INACTIVE:
      return mus::mojom::ShowState::INACTIVE;
    case ui::SHOW_STATE_FULLSCREEN:
      return mus::mojom::ShowState::FULLSCREEN;
    case ui::SHOW_STATE_DOCKED:
      return mus::mojom::ShowState::DOCKED;
    default:
      break;
  }
  return mus::mojom::ShowState::DEFAULT;
}

}  // namespace

WmWindowMus::WmWindowMus(mus::Window* window) : window_(window) {
  window_->AddObserver(this);
  window_->SetLocalProperty(kWmWindowKey, this);
  window_state_.reset(new WindowStateMus(this));
}

WmWindowMus::~WmWindowMus() {
  window_->RemoveObserver(this);
}

// static
WmWindowMus* WmWindowMus::Get(mus::Window* window) {
  if (!window)
    return nullptr;

  wm::WmWindowMus* wm_window = window->GetLocalProperty(kWmWindowKey);
  if (wm_window)
    return wm_window;
  // WmWindowMus is owned by the mus::Window.
  return new WmWindowMus(window);
}

// static
WmWindowMus* WmWindowMus::Get(views::Widget* widget) {
  return WmWindowMus::Get(aura::GetMusWindow(widget->GetNativeView()));
}

// static
const mus::Window* WmWindowMus::GetMusWindow(
    const ash::wm::WmWindow* wm_window) {
  return static_cast<const WmWindowMus*>(wm_window)->mus_window();
}

// static
std::vector<ash::wm::WmWindow*> WmWindowMus::FromMusWindows(
    const std::vector<mus::Window*>& mus_windows) {
  std::vector<ash::wm::WmWindow*> result(mus_windows.size());
  for (size_t i = 0; i < mus_windows.size(); ++i)
    result[i] = Get(mus_windows[i]);
  return result;
}

const WmRootWindowControllerMus* WmWindowMus::GetRootWindowControllerMus()
    const {
  return WmRootWindowControllerMus::Get(window_->GetRoot());
}

const ash::wm::WmWindow* WmWindowMus::GetRootWindow() const {
  return Get(window_->GetRoot());
}

ash::wm::WmRootWindowController* WmWindowMus::GetRootWindowController() {
  return GetRootWindowControllerMus();
}

ash::wm::WmGlobals* WmWindowMus::GetGlobals() const {
  return WmGlobalsMus::Get();
}

void WmWindowMus::SetShellWindowId(int id) {
  window_->set_local_id(id);
}

int WmWindowMus::GetShellWindowId() const {
  return window_->local_id();
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
  const mus::Window* target_window = GetMusWindow(target);
  if (target_window->Contains(window_)) {
    gfx::Point result(point);
    const mus::Window* window = window_;
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
  NOTIMPLEMENTED();
  return window_->visible();
}

bool WmWindowMus::IsVisible() const {
  return window_->visible();
}

bool WmWindowMus::IsSystemModal() const {
  NOTIMPLEMENTED();
  return false;
}

bool WmWindowMus::GetBoolProperty(ash::wm::WmWindowProperty key) {
  NOTIMPLEMENTED();
  switch (key) {
    case ash::wm::WmWindowProperty::SNAP_CHILDREN_TO_PIXEL_BOUDARY:
      return true;

    case ash::wm::WmWindowProperty::ALWAYS_ON_TOP:
      return false;

    default:
      NOTREACHED();
      break;
  }

  NOTREACHED();
  return false;
}

int WmWindowMus::GetIntProperty(ash::wm::WmWindowProperty key) {
  NOTIMPLEMENTED();
  if (key == ash::wm::WmWindowProperty::SHELF_ID)
    return 0;

  NOTREACHED();
  return 0;
}

const ash::wm::WindowState* WmWindowMus::GetWindowState() const {
  return window_state_.get();
}

ash::wm::WmWindow* WmWindowMus::GetToplevelWindow() {
  return WmGlobalsMus::GetToplevelAncestor(window_);
}

void WmWindowMus::SetParentUsingContext(WmWindow* context,
                                        const gfx::Rect& screen_bounds) {
  GetDefaultParent(context, this, screen_bounds)->AddChild(this);
}

void WmWindowMus::AddChild(WmWindow* window) {
  window_->AddChild(GetMusWindow(window));
}

ash::wm::WmWindow* WmWindowMus::GetParent() {
  return Get(window_->parent());
}

const ash::wm::WmWindow* WmWindowMus::GetTransientParent() const {
  return Get(window_->transient_parent());
}

std::vector<ash::wm::WmWindow*> WmWindowMus::GetTransientChildren() {
  return FromMusWindows(window_->transient_children());
}

void WmWindowMus::SetLayoutManager(
    std::unique_ptr<ash::wm::WmLayoutManager> layout_manager) {
  if (layout_manager) {
    layout_manager_adapter_.reset(
        new MusLayoutManagerAdapter(window_, std::move(layout_manager)));
  } else {
    layout_manager_adapter_.reset();
  }
}

ash::wm::WmLayoutManager* WmWindowMus::GetLayoutManager() {
  return layout_manager_adapter_ ? layout_manager_adapter_->layout_manager()
                                 : nullptr;
}

void WmWindowMus::SetVisibilityAnimationType(int type) {
  NOTIMPLEMENTED();
}

void WmWindowMus::SetVisibilityAnimationDuration(base::TimeDelta delta) {
  NOTIMPLEMENTED();
}

void WmWindowMus::Animate(::wm::WindowAnimationType type) {
  NOTIMPLEMENTED();
}

void WmWindowMus::SetBounds(const gfx::Rect& bounds) {
  if (layout_manager_adapter_)
    layout_manager_adapter_->layout_manager()->SetChildBounds(this, bounds);
  else
    window_->SetBounds(bounds);
}

void WmWindowMus::SetBoundsWithTransitionDelay(const gfx::Rect& bounds,
                                               base::TimeDelta delta) {
  NOTIMPLEMENTED();
  SetBounds(bounds);
}

void WmWindowMus::SetBoundsDirect(const gfx::Rect& bounds) {
  window_->SetBounds(bounds);
  SnapToPixelBoundaryIfNecessary();
}

void WmWindowMus::SetBoundsDirectAnimated(const gfx::Rect& bounds) {
  NOTIMPLEMENTED();
  SetBoundsDirect(bounds);
}

void WmWindowMus::SetBoundsDirectCrossFade(const gfx::Rect& bounds) {
  NOTIMPLEMENTED();
  SetBoundsDirect(bounds);
}

void WmWindowMus::SetBoundsInScreen(const gfx::Rect& bounds_in_screen,
                                    const display::Display& dst_display) {
  // TODO(sky): need to find WmRootWindowControllerMus for dst_display and
  // convert.
  NOTIMPLEMENTED();
  SetBounds(ConvertRectFromScreen(bounds_in_screen));
}

gfx::Rect WmWindowMus::GetBoundsInScreen() const {
  return ConvertRectToScreen(window_->bounds());
}

const gfx::Rect& WmWindowMus::GetBounds() const {
  return window_->bounds();
}

gfx::Rect WmWindowMus::GetTargetBounds() {
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

bool WmWindowMus::Contains(const ash::wm::WmWindow* other) const {
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

void WmWindowMus::StackChildAtTop(ash::wm::WmWindow* child) {
  GetMusWindow(child)->MoveToFront();
}

void WmWindowMus::StackChildAtBottom(ash::wm::WmWindow* child) {
  GetMusWindow(child)->MoveToBack();
}

void WmWindowMus::StackChildAbove(ash::wm::WmWindow* child,
                                  ash::wm::WmWindow* target) {
  GetMusWindow(child)->Reorder(GetMusWindow(target),
                               mus::mojom::OrderDirection::ABOVE);
}

void WmWindowMus::StackChildBelow(WmWindow* child, WmWindow* target) {
  GetMusWindow(child)->Reorder(GetMusWindow(target),
                               mus::mojom::OrderDirection::BELOW);
}

void WmWindowMus::SetAlwaysOnTop(bool value) {
  // TODO(sky): need to set property on window.
  NOTIMPLEMENTED();
}

bool WmWindowMus::IsAlwaysOnTop() const {
  // TODO(sky): need to set property on window.
  NOTIMPLEMENTED();
  return false;
}

void WmWindowMus::Hide() {
  window_->SetVisible(false);
}

void WmWindowMus::Show() {
  window_->SetVisible(true);
}

bool WmWindowMus::IsFocused() const {
  return window_->HasFocus();
}

bool WmWindowMus::IsActive() const {
  mus::Window* focused = window_->connection()->GetFocusedWindow();
  return focused && window_->Contains(focused);
}

void WmWindowMus::Activate() {
  window_->SetFocus();
}

void WmWindowMus::Deactivate() {
  if (IsActive())
    window_->connection()->ClearFocus();
}

void WmWindowMus::SetFullscreen() {
  SetWindowShowState(window_, mus::mojom::ShowState::FULLSCREEN);
}

void WmWindowMus::Maximize() {
  SetWindowShowState(window_, mus::mojom::ShowState::MAXIMIZED);
}

void WmWindowMus::Minimize() {
  SetWindowShowState(window_, mus::mojom::ShowState::MINIMIZED);
}

void WmWindowMus::Unminimize() {
  SetWindowShowState(window_, MojomWindowShowStateFromUI(restore_show_state_));
  restore_show_state_ = ui::SHOW_STATE_DEFAULT;
}

std::vector<ash::wm::WmWindow*> WmWindowMus::GetChildren() {
  return FromMusWindows(window_->children());
}

ash::wm::WmWindow* WmWindowMus::GetChildByShellWindowId(int id) {
  return Get(GetMusWindowByShellId(window_, id));
}

void WmWindowMus::SnapToPixelBoundaryIfNecessary() {
  NOTIMPLEMENTED();
}

void WmWindowMus::AddObserver(ash::wm::WmWindowObserver* observer) {
  observers_.AddObserver(observer);
}

void WmWindowMus::RemoveObserver(ash::wm::WmWindowObserver* observer) {
  observers_.RemoveObserver(observer);
}

void WmWindowMus::NotifyStackingChanged() {
  FOR_EACH_OBSERVER(ash::wm::WmWindowObserver, observers_,
                    OnWindowStackingChanged(this));
}

void WmWindowMus::OnTreeChanged(const TreeChangeParams& params) {
  ash::wm::WmWindowObserver::TreeChangeParams wm_params;
  wm_params.target = Get(params.target);
  wm_params.new_parent = Get(params.new_parent);
  wm_params.old_parent = Get(params.old_parent);
  FOR_EACH_OBSERVER(ash::wm::WmWindowObserver, observers_,
                    OnWindowTreeChanged(this, wm_params));
}

void WmWindowMus::OnWindowReordered(mus::Window* window,
                                    mus::Window* relative_window,
                                    mus::mojom::OrderDirection direction) {
  if (!window_->parent())
    return;

  static_cast<WmWindowMus*>(Get(window_->parent()))->NotifyStackingChanged();
}

void WmWindowMus::OnWindowSharedPropertyChanged(
    mus::Window* window,
    const std::string& name,
    const std::vector<uint8_t>* old_data,
    const std::vector<uint8_t>* new_data) {
  // Deal with always on top and snap.
  NOTIMPLEMENTED();
}

void WmWindowMus::OnWindowBoundsChanged(mus::Window* window,
                                        const gfx::Rect& old_bounds,
                                        const gfx::Rect& new_bounds) {
  FOR_EACH_OBSERVER(ash::wm::WmWindowObserver, observers_,
                    OnWindowBoundsChanged(this, old_bounds, new_bounds));
}

void WmWindowMus::OnWindowDestroying(mus::Window* window) {
  FOR_EACH_OBSERVER(ash::wm::WmWindowObserver, observers_,
                    OnWindowDestroying(this));
}

}  // namespace wm
}  // namespace mash
