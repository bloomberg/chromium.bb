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
#include "ui/aura/window.h"
#include "ui/base/hit_test.h"
#include "ui/display/display.h"
#include "ui/resources/grit/ui_resources.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

MUS_DECLARE_WINDOW_PROPERTY_TYPE(ash::mus::WmWindowMus*);

namespace {

MUS_DEFINE_OWNED_WINDOW_PROPERTY_KEY(ash::mus::WmWindowMus,
                                     kWmWindowKey,
                                     nullptr);

}  // namespace

namespace ash {
namespace mus {

namespace {

// This class is used so that the WindowState constructor can be made protected.
// GetWindowState() is the only place that should be creating WindowState.
class WindowStateMus : public wm::WindowState {
 public:
  explicit WindowStateMus(WmWindow* window) : wm::WindowState(window) {}
  ~WindowStateMus() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(WindowStateMus);
};

ui::WindowShowState UIWindowShowStateFromMojom(ui::mojom::ShowState state) {
  switch (state) {
    case ui::mojom::ShowState::DEFAULT:
      return ui::SHOW_STATE_DEFAULT;
    case ui::mojom::ShowState::NORMAL:
      return ui::SHOW_STATE_NORMAL;
    case ui::mojom::ShowState::MINIMIZED:
      return ui::SHOW_STATE_MINIMIZED;
    case ui::mojom::ShowState::MAXIMIZED:
      return ui::SHOW_STATE_MAXIMIZED;
    case ui::mojom::ShowState::INACTIVE:
      return ui::SHOW_STATE_INACTIVE;
    case ui::mojom::ShowState::FULLSCREEN:
      return ui::SHOW_STATE_FULLSCREEN;
    case ui::mojom::ShowState::DOCKED:
      return ui::SHOW_STATE_DOCKED;
    default:
      break;
  }
  return ui::SHOW_STATE_DEFAULT;
}

ui::mojom::ShowState MojomWindowShowStateFromUI(ui::WindowShowState state) {
  switch (state) {
    case ui::SHOW_STATE_DEFAULT:
      return ui::mojom::ShowState::DEFAULT;
    case ui::SHOW_STATE_NORMAL:
      return ui::mojom::ShowState::NORMAL;
    case ui::SHOW_STATE_MINIMIZED:
      return ui::mojom::ShowState::MINIMIZED;
    case ui::SHOW_STATE_MAXIMIZED:
      return ui::mojom::ShowState::MAXIMIZED;
    case ui::SHOW_STATE_INACTIVE:
      return ui::mojom::ShowState::INACTIVE;
    case ui::SHOW_STATE_FULLSCREEN:
      return ui::mojom::ShowState::FULLSCREEN;
    case ui::SHOW_STATE_DOCKED:
      return ui::mojom::ShowState::DOCKED;
    default:
      break;
  }
  return ui::mojom::ShowState::DEFAULT;
}

// Returns the WmWindowProperty enum value for the given ui::Window key name.
WmWindowProperty WmWindowPropertyFromUI(const std::string& ui_window_key) {
  if (ui_window_key == ui::mojom::WindowManager::kAlwaysOnTop_Property)
    return WmWindowProperty::ALWAYS_ON_TOP;
  if (ui_window_key == ui::mojom::WindowManager::kExcludeFromMru_Property)
    return WmWindowProperty::EXCLUDE_FROM_MRU;
  if (ui_window_key == ui::mojom::WindowManager::kShelfItemType_Property)
    return WmWindowProperty::SHELF_ITEM_TYPE;
  return WmWindowProperty::INVALID_PROPERTY;
}

}  // namespace

WmWindowMus::WmWindowMus(ui::Window* window)
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
const WmWindowMus* WmWindowMus::Get(const ui::Window* window) {
  if (!window)
    return nullptr;

  const WmWindowMus* wm_window = window->GetLocalProperty(kWmWindowKey);
  if (wm_window)
    return wm_window;
  // WmWindowMus is owned by the ui::Window.
  // Unfortunately there isn't a good way to avoid the cast here.
  return new WmWindowMus(const_cast<ui::Window*>(window));
}

// static
WmWindowMus* WmWindowMus::Get(views::Widget* widget) {
  return WmWindowMus::Get(aura::GetMusWindow(widget->GetNativeView()));
}

// static
const ui::Window* WmWindowMus::GetMusWindow(const WmWindow* wm_window) {
  return static_cast<const WmWindowMus*>(wm_window)->mus_window();
}

// static
std::vector<WmWindow*> WmWindowMus::FromMusWindows(
    const std::vector<ui::Window*>& mus_windows) {
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

void WmWindowMus::Destroy() {
  // TODO(sky): to match aura behavior this should delete children.
  // http://crbug.com/647513.
  window_->Destroy();
  // WARNING: this has been deleted.
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
        ui::mojom::WindowManager::kName_Property, std::string(name));
  } else {
    window_->ClearSharedProperty(ui::mojom::WindowManager::kName_Property);
  }
}

std::string WmWindowMus::GetName() const {
  return window_->HasSharedProperty(ui::mojom::WindowManager::kName_Property)
             ? window_->GetSharedProperty<std::string>(
                   ui::mojom::WindowManager::kName_Property)
             : std::string();
}

void WmWindowMus::SetTitle(const base::string16& title) {
  SetWindowTitle(window_, title);
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
  // If the WindowType was expicitly set, then it means |window_| was created
  // by way of WmShellMus::NewWindow() and the type is locally defined. For
  // windows created in other ways, say from the client, then we need to get
  // the type from |window_| directly.
  return is_wm_window_type_set_ ? wm_window_type_ : GetWmWindowType(window_);
}

int WmWindowMus::GetAppType() const {
  // TODO: Need support for window property kAppType: http://crbug.com/651206.
  NOTIMPLEMENTED();
  return 0;
}

void WmWindowMus::SetAppType(int app_type) const {
  // TODO: Need support for window property kAppType: http://crbug.com/651206.
  NOTIMPLEMENTED();
}

bool WmWindowMus::IsBubble() {
  return GetWindowType(window_) == ui::mojom::WindowType::BUBBLE;
}

ui::Layer* WmWindowMus::GetLayer() {
  // TODO: http://crbug.com/652877.
  NOTIMPLEMENTED();
  return widget_ ? widget_->GetLayer() : nullptr;
}

bool WmWindowMus::GetLayerTargetVisibility() {
  // TODO: http://crbug.com/652877.
  NOTIMPLEMENTED();
  return GetTargetVisibility();
}

bool WmWindowMus::GetLayerVisible() {
  // TODO: http://crbug.com/652877.
  NOTIMPLEMENTED();
  return IsVisible();
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
  const ui::Window* target_window = GetMusWindow(target);
  if (target_window->Contains(window_)) {
    gfx::Point result(point);
    const ui::Window* window = window_;
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
  return widget_ && !use_empty_minimum_size_for_testing_
             ? widget_->GetMinimumSize()
             : gfx::Size();
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

gfx::Rect WmWindowMus::GetMinimizeAnimationTargetBoundsInScreen() const {
  // TODO: need animation support: http://crbug.com/615087.
  NOTIMPLEMENTED();
  return GetBoundsInScreen();
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
    case WmWindowProperty::ALWAYS_ON_TOP:
      return IsAlwaysOnTop();

    case WmWindowProperty::DRAW_ATTENTION:
      NOTIMPLEMENTED();
      return false;

    case WmWindowProperty::EXCLUDE_FROM_MRU:
      return GetExcludeFromMru(window_);

    case WmWindowProperty::SNAP_CHILDREN_TO_PIXEL_BOUNDARY:
      return snap_children_to_pixel_boundary_;

    default:
      NOTREACHED();
      break;
  }

  NOTREACHED();
  return false;
}

SkColor WmWindowMus::GetColorProperty(WmWindowProperty key) {
  if (key == WmWindowProperty::TOP_VIEW_COLOR) {
    // TODO: need support for TOP_VIEW_COLOR: http://crbug.com/615100.
    NOTIMPLEMENTED();
    return 0;
  }

  NOTREACHED();
  return 0;
}

void WmWindowMus::SetColorProperty(WmWindowProperty key, SkColor value) {
  if (key == WmWindowProperty::TOP_VIEW_COLOR) {
    // TODO: need support for TOP_VIEW_COLOR: http://crbug.com/615100.
    NOTIMPLEMENTED();
    return;
  }

  NOTREACHED();
}

int WmWindowMus::GetIntProperty(WmWindowProperty key) {
  if (key == WmWindowProperty::MODAL_TYPE) {
    // TODO: WindowTree::SetModalWindow() needs to route through WindowManager
    // so wm can position. http://crbug.com/645996.
    NOTIMPLEMENTED();
    return static_cast<int>(ui::MODAL_TYPE_NONE);
  }

  if (key == WmWindowProperty::SHELF_ID) {
    if (window_->HasSharedProperty(
            ui::mojom::WindowManager::kShelfId_Property)) {
      return window_->GetSharedProperty<int>(
          ui::mojom::WindowManager::kShelfId_Property);
    }

    return kInvalidShelfID;
  }

  if (key == WmWindowProperty::SHELF_ITEM_TYPE) {
    if (window_->HasSharedProperty(
            ui::mojom::WindowManager::kShelfItemType_Property)) {
      return window_->GetSharedProperty<int>(
          ui::mojom::WindowManager::kShelfItemType_Property);
    }
    // Mash provides a default shelf item type for non-ignored windows.
    return GetWindowIgnoredByShelf(window_) ? TYPE_UNDEFINED : TYPE_APP;
  }

  if (key == WmWindowProperty::TOP_VIEW_INSET) {
    // TODO: need support for TOP_VIEW_INSET: http://crbug.com/615100.
    NOTIMPLEMENTED();
    return 0;
  }

  NOTREACHED();
  return 0;
}

void WmWindowMus::SetIntProperty(WmWindowProperty key, int value) {
  if (key == WmWindowProperty::SHELF_ID) {
    window_->SetSharedProperty<int>(ui::mojom::WindowManager::kShelfId_Property,
                                    value);
    return;
  }

  if (key == WmWindowProperty::SHELF_ITEM_TYPE) {
    window_->SetSharedProperty<int>(
        ui::mojom::WindowManager::kShelfItemType_Property, value);
    return;
  }

  if (key == WmWindowProperty::TOP_VIEW_INSET) {
    // TODO: need support for TOP_VIEW_INSET: http://crbug.com/615100.
    NOTIMPLEMENTED();
    return;
  }

  NOTREACHED();
}

std::string WmWindowMus::GetStringProperty(WmWindowProperty key) {
  NOTIMPLEMENTED();
  return std::string();
}

void WmWindowMus::SetStringProperty(WmWindowProperty key,
                                    const std::string& value) {
  NOTIMPLEMENTED();
}

gfx::ImageSkia WmWindowMus::GetWindowIcon() {
  NOTIMPLEMENTED();
  return gfx::ImageSkia();
}

gfx::ImageSkia WmWindowMus::GetAppIcon() {
  NOTIMPLEMENTED();
  return gfx::ImageSkia();
}

const wm::WindowState* WmWindowMus::GetWindowState() const {
  return window_state_.get();
}

WmWindow* WmWindowMus::GetToplevelWindow() {
  return WmShellMus::GetToplevelAncestor(window_);
}

WmWindow* WmWindowMus::GetToplevelWindowForFocus() {
  // TODO(sky): resolve if we really need two notions of top-level. In the mus
  // world they are the same.
  return WmShellMus::GetToplevelAncestor(window_);
}

void WmWindowMus::SetParentUsingContext(WmWindow* context,
                                        const gfx::Rect& screen_bounds) {
  wm::GetDefaultParent(context, this, screen_bounds)->AddChild(this);
}

void WmWindowMus::AddChild(WmWindow* window) {
  window_->AddChild(GetMusWindow(window));
}

void WmWindowMus::RemoveChild(WmWindow* child) {
  window_->RemoveChild(GetMusWindow(child));
}

const WmWindow* WmWindowMus::GetParent() const {
  return Get(window_->parent());
}

const WmWindow* WmWindowMus::GetTransientParent() const {
  return Get(window_->transient_parent());
}

std::vector<WmWindow*> WmWindowMus::GetTransientChildren() {
  return FromMusWindows(window_->transient_children());
}

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

void WmWindowMus::SetVisibilityChangesAnimated() {
  // TODO: need animation support: http://crbug.com/615087.
  NOTIMPLEMENTED();
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
  DCHECK(GetParent());  // Aura code assumed a parent, so this does too.
  if (static_cast<const WmWindowMus*>(GetParent())
          ->child_bounds_in_screen_behavior_ ==
      BoundsInScreenBehavior::USE_LOCAL_COORDINATES) {
    SetBounds(bounds_in_screen);
    return;
  }
  wm::SetBoundsInScreen(this, bounds_in_screen, dst_display);
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
  window_->ClearSharedProperty(
      ui::mojom::WindowManager::kRestoreBounds_Property);
}

void WmWindowMus::SetRestoreBoundsInScreen(const gfx::Rect& bounds) {
  SetRestoreBounds(window_, bounds);
}

gfx::Rect WmWindowMus::GetRestoreBoundsInScreen() const {
  return GetRestoreBounds(window_);
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
  locked_to_root_ = value;
}

bool WmWindowMus::IsLockedToRoot() const {
  return locked_to_root_;
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
  return window_->HasSharedProperty(
      ui::mojom::WindowManager::kRestoreBounds_Property);
}

bool WmWindowMus::CanMaximize() const {
  return (GetResizeBehavior(window_) &
          ::ui::mojom::kResizeBehaviorCanMaximize) != 0;
}

bool WmWindowMus::CanMinimize() const {
  return (GetResizeBehavior(window_) &
          ::ui::mojom::kResizeBehaviorCanMinimize) != 0;
}

bool WmWindowMus::CanResize() const {
  return window_ &&
         (GetResizeBehavior(window_) & ::ui::mojom::kResizeBehaviorCanResize) !=
             0;
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
                               ui::mojom::OrderDirection::ABOVE);
}

void WmWindowMus::StackChildBelow(WmWindow* child, WmWindow* target) {
  GetMusWindow(child)->Reorder(GetMusWindow(target),
                               ui::mojom::OrderDirection::BELOW);
}

void WmWindowMus::SetPinned(bool trusted) {
  // http://crbug.com/622486.
  NOTIMPLEMENTED();
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

void WmWindowMus::SetFocused() {
  window_->SetFocus();
}

bool WmWindowMus::IsFocused() const {
  return window_->HasFocus();
}

bool WmWindowMus::IsActive() const {
  ui::Window* focused = window_->window_tree()->GetFocusedWindow();
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
  SetWindowShowState(window_, ui::mojom::ShowState::FULLSCREEN);
}

void WmWindowMus::Maximize() {
  SetWindowShowState(window_, ui::mojom::ShowState::MAXIMIZED);
}

void WmWindowMus::Minimize() {
  SetWindowShowState(window_, ui::mojom::ShowState::MINIMIZED);
}

void WmWindowMus::Unminimize() {
  SetWindowShowState(window_, MojomWindowShowStateFromUI(restore_show_state_));
  restore_show_state_ = ui::SHOW_STATE_DEFAULT;
}

void WmWindowMus::SetExcludedFromMru(bool excluded_from_mru) {
  SetExcludeFromMru(window_, excluded_from_mru);
}

std::vector<WmWindow*> WmWindowMus::GetChildren() {
  return FromMusWindows(window_->children());
}

WmWindow* WmWindowMus::GetChildByShellWindowId(int id) {
  if (id == shell_window_id_)
    return this;
  for (ui::Window* child : window_->children()) {
    WmWindow* result = Get(child)->GetChildByShellWindowId(id);
    if (result)
      return result;
  }
  return nullptr;
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

void WmWindowMus::SetBoundsInScreenBehaviorForChildren(
    WmWindow::BoundsInScreenBehavior behavior) {
  child_bounds_in_screen_behavior_ = behavior;
}

void WmWindowMus::SetSnapsChildrenToPhysicalPixelBoundary() {
  if (snap_children_to_pixel_boundary_)
    return;

  snap_children_to_pixel_boundary_ = true;
  for (auto& observer : observers_) {
    observer.OnWindowPropertyChanged(
        this, WmWindowProperty::SNAP_CHILDREN_TO_PIXEL_BOUNDARY);
  }
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

std::unique_ptr<views::View> WmWindowMus::CreateViewWithRecreatedLayers() {
  // TODO: need real implementation, http://crbug.com/629497.
  std::unique_ptr<views::View> view(new views::View);
  return view;
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

void WmWindowMus::AddTransientWindowObserver(
    WmTransientWindowObserver* observer) {
  transient_observers_.AddObserver(observer);
}

void WmWindowMus::RemoveTransientWindowObserver(
    WmTransientWindowObserver* observer) {
  transient_observers_.RemoveObserver(observer);
}

void WmWindowMus::AddLimitedPreTargetHandler(ui::EventHandler* handler) {
  DCHECK(GetInternalWidget());
  widget_->GetNativeWindow()->AddPreTargetHandler(handler);
}

void WmWindowMus::RemoveLimitedPreTargetHandler(ui::EventHandler* handler) {
  widget_->GetNativeWindow()->RemovePreTargetHandler(handler);
}

void WmWindowMus::OnTreeChanging(const TreeChangeParams& params) {
  WmWindowObserver::TreeChangeParams wm_params;
  wm_params.target = Get(params.target);
  wm_params.new_parent = Get(params.new_parent);
  wm_params.old_parent = Get(params.old_parent);
  for (auto& observer : observers_)
    observer.OnWindowTreeChanging(this, wm_params);
}

void WmWindowMus::OnTreeChanged(const TreeChangeParams& params) {
  WmWindowObserver::TreeChangeParams wm_params;
  wm_params.target = Get(params.target);
  wm_params.new_parent = Get(params.new_parent);
  wm_params.old_parent = Get(params.old_parent);
  for (auto& observer : observers_)
    observer.OnWindowTreeChanged(this, wm_params);
}

void WmWindowMus::OnWindowReordered(ui::Window* window,
                                    ui::Window* relative_window,
                                    ui::mojom::OrderDirection direction) {
  for (auto& observer : observers_)
    observer.OnWindowStackingChanged(this);
}

void WmWindowMus::OnWindowSharedPropertyChanged(
    ui::Window* window,
    const std::string& name,
    const std::vector<uint8_t>* old_data,
    const std::vector<uint8_t>* new_data) {
  if (name == ui::mojom::WindowManager::kShowState_Property) {
    GetWindowState()->OnWindowShowStateChanged();
    return;
  }
  if (name == ui::mojom::WindowManager::kWindowTitle_Property) {
    for (auto& observer : observers_)
      observer.OnWindowTitleChanged(this);
    return;
  }

  // Notify WmWindowObserver of certain white-listed property changes.
  WmWindowProperty wm_property = WmWindowPropertyFromUI(name);
  if (wm_property != WmWindowProperty::INVALID_PROPERTY) {
    for (auto& observer : observers_)
      observer.OnWindowPropertyChanged(this, wm_property);
    return;
  }

  // Deal with snap to pixel.
  NOTIMPLEMENTED();
}

void WmWindowMus::OnWindowBoundsChanged(ui::Window* window,
                                        const gfx::Rect& old_bounds,
                                        const gfx::Rect& new_bounds) {
  for (auto& observer : observers_)
    observer.OnWindowBoundsChanged(this, old_bounds, new_bounds);
}

void WmWindowMus::OnWindowDestroying(ui::Window* window) {
  for (auto& observer : observers_)
    observer.OnWindowDestroying(this);
}

void WmWindowMus::OnWindowDestroyed(ui::Window* window) {
  for (auto& observer : observers_)
    observer.OnWindowDestroyed(this);
}

void WmWindowMus::OnWindowVisibilityChanging(ui::Window* window, bool visible) {
  DCHECK_EQ(window_, window);
  for (auto& observer : observers_)
    observer.OnWindowVisibilityChanging(this, visible);
}

void WmWindowMus::OnWindowVisibilityChanged(ui::Window* window, bool visible) {
  for (auto& observer : observers_)
    observer.OnWindowVisibilityChanged(Get(window), visible);
}

void WmWindowMus::OnTransientChildAdded(ui::Window* window,
                                        ui::Window* transient) {
  for (auto& observer : transient_observers_)
    observer.OnTransientChildAdded(this, Get(transient));
}

void WmWindowMus::OnTransientChildRemoved(ui::Window* window,
                                          ui::Window* transient) {
  for (auto& observer : transient_observers_)
    observer.OnTransientChildRemoved(this, Get(transient));
}

}  // namespace mus
}  // namespace ash
