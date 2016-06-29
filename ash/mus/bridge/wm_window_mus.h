// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_MUS_BRIDGE_WM_WINDOW_MUS_H_
#define ASH_MUS_BRIDGE_WM_WINDOW_MUS_H_

#include <memory>

#include "ash/common/shell_window_ids.h"
#include "ash/common/wm_window.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "components/mus/public/cpp/window_observer.h"

namespace views {
class Widget;
}

namespace ash {

namespace wm {
class WmLayoutManager;
}

namespace mus {

class MusLayoutManagerAdapter;
class WmRootWindowControllerMus;

// WmWindow implementation for mus.
//
// WmWindowMus is tied to the life of the underlying ::mus::Window (it is stored
// as an owned property).
class WmWindowMus : public WmWindow, public ::mus::WindowObserver {
 public:
  // Indicates the source of the widget creation.
  enum class WidgetCreationType {
    // The widget was created internally, and not at the request of a client.
    // For example, overview mode creates a number of widgets. These widgets are
    // created with a type of INTERNAL.
    INTERNAL,

    // The widget was created for a client. In other words there is a client
    // embedded in the mus::Window. For example, when Chrome creates a new
    // browser window the window manager is asked to create the mus::Window.
    // The window manager creates a mus::Window and a views::Widget to show the
    // non-client frame decorations. In this case the creation type is
    // FOR_CLIENT.
    FOR_CLIENT,
  };

  explicit WmWindowMus(::mus::Window* window);
  // NOTE: this class is owned by the corresponding window. You shouldn't delete
  // TODO(sky): friend deleter and make private.
  ~WmWindowMus() override;

  // Returns a WmWindow for an mus::Window, creating if necessary.
  static WmWindowMus* Get(::mus::Window* window);

  static WmWindowMus* Get(views::Widget* widget);

  static ::mus::Window* GetMusWindow(WmWindow* wm_window) {
    return const_cast<::mus::Window*>(
        GetMusWindow(const_cast<const WmWindow*>(wm_window)));
  }
  static const ::mus::Window* GetMusWindow(const WmWindow* wm_window);

  static std::vector<WmWindow*> FromMusWindows(
      const std::vector<::mus::Window*>& mus_windows);

  // Sets the widget associated with the window. The widget is used to query
  // state, such as min/max size. The widget is not owned by the WmWindowMus.
  void set_widget(views::Widget* widget, WidgetCreationType type) {
    widget_ = widget;
    widget_creation_type_ = type;
  }

  ::mus::Window* mus_window() { return window_; }
  const ::mus::Window* mus_window() const { return window_; }

  WmRootWindowControllerMus* GetRootWindowControllerMus() {
    return const_cast<WmRootWindowControllerMus*>(
        const_cast<const WmWindowMus*>(this)->GetRootWindowControllerMus());
  }
  const WmRootWindowControllerMus* GetRootWindowControllerMus() const;

  static WmWindowMus* AsWmWindowMus(WmWindow* window) {
    return static_cast<WmWindowMus*>(window);
  }
  static const WmWindowMus* AsWmWindowMus(const WmWindow* window) {
    return static_cast<const WmWindowMus*>(window);
  }

  wm::WindowState* GetWindowState() { return WmWindow::GetWindowState(); }

  // See description of |children_use_extended_hit_region_|.
  bool ShouldUseExtendedHitRegion() const;

  // Returns true if this window is considered a shell window container.
  bool IsContainer() const;

  // WmWindow:
  const WmWindow* GetRootWindow() const override;
  WmRootWindowController* GetRootWindowController() override;
  WmShell* GetShell() const override;
  void SetName(const char* name) override;
  base::string16 GetTitle() const override;
  void SetShellWindowId(int id) override;
  int GetShellWindowId() const override;
  WmWindow* GetChildByShellWindowId(int id) override;
  ui::wm::WindowType GetType() const override;
  ui::Layer* GetLayer() override;
  display::Display GetDisplayNearestWindow() override;
  bool HasNonClientArea() override;
  int GetNonClientComponent(const gfx::Point& location) override;
  gfx::Point ConvertPointToTarget(const WmWindow* target,
                                  const gfx::Point& point) const override;
  gfx::Point ConvertPointToScreen(const gfx::Point& point) const override;
  gfx::Point ConvertPointFromScreen(const gfx::Point& point) const override;
  gfx::Rect ConvertRectToScreen(const gfx::Rect& rect) const override;
  gfx::Rect ConvertRectFromScreen(const gfx::Rect& rect) const override;
  gfx::Size GetMinimumSize() const override;
  gfx::Size GetMaximumSize() const override;
  bool GetTargetVisibility() const override;
  bool IsVisible() const override;
  void SetOpacity(float opacity) override;
  float GetTargetOpacity() const override;
  void SetTransform(const gfx::Transform& transform) override;
  gfx::Transform GetTargetTransform() const override;
  bool IsSystemModal() const override;
  bool GetBoolProperty(WmWindowProperty key) override;
  int GetIntProperty(WmWindowProperty key) override;
  const wm::WindowState* GetWindowState() const override;
  WmWindow* GetToplevelWindow() override;
  void SetParentUsingContext(WmWindow* context,
                             const gfx::Rect& screen_bounds) override;
  void AddChild(WmWindow* window) override;
  WmWindow* GetParent() override;
  const WmWindow* GetTransientParent() const override;
  std::vector<WmWindow*> GetTransientChildren() override;
  void SetLayoutManager(
      std::unique_ptr<WmLayoutManager> layout_manager) override;
  WmLayoutManager* GetLayoutManager() override;
  void SetVisibilityAnimationType(int type) override;
  void SetVisibilityAnimationDuration(base::TimeDelta delta) override;
  void SetVisibilityAnimationTransition(
      ::wm::WindowVisibilityAnimationTransition transition) override;
  void Animate(::wm::WindowAnimationType type) override;
  void StopAnimatingProperty(
      ui::LayerAnimationElement::AnimatableProperty property) override;
  void SetChildWindowVisibilityChangesAnimated() override;
  void SetMasksToBounds(bool value) override;
  void SetBounds(const gfx::Rect& bounds) override;
  void SetBoundsWithTransitionDelay(const gfx::Rect& bounds,
                                    base::TimeDelta delta) override;
  void SetBoundsDirect(const gfx::Rect& bounds) override;
  void SetBoundsDirectAnimated(const gfx::Rect& bounds) override;
  void SetBoundsDirectCrossFade(const gfx::Rect& bounds) override;
  void SetBoundsInScreen(const gfx::Rect& bounds_in_screen,
                         const display::Display& dst_display) override;
  gfx::Rect GetBoundsInScreen() const override;
  const gfx::Rect& GetBounds() const override;
  gfx::Rect GetTargetBounds() override;
  void ClearRestoreBounds() override;
  void SetRestoreBoundsInScreen(const gfx::Rect& bounds) override;
  gfx::Rect GetRestoreBoundsInScreen() const override;
  bool Contains(const WmWindow* other) const override;
  void SetShowState(ui::WindowShowState show_state) override;
  ui::WindowShowState GetShowState() const override;
  void SetRestoreShowState(ui::WindowShowState show_state) override;
  void SetRestoreOverrides(const gfx::Rect& bounds_override,
                           ui::WindowShowState window_state_override) override;
  void SetLockedToRoot(bool value) override;
  void SetCapture() override;
  bool HasCapture() override;
  void ReleaseCapture() override;
  bool HasRestoreBounds() const override;
  void SetAlwaysOnTop(bool value) override;
  bool IsAlwaysOnTop() const override;
  void Hide() override;
  void Show() override;
  views::Widget* GetInternalWidget() override;
  void CloseWidget() override;
  bool IsFocused() const override;
  bool IsActive() const override;
  void Activate() override;
  void Deactivate() override;
  void SetFullscreen() override;
  void Maximize() override;
  void Minimize() override;
  void Unminimize() override;
  void SetExcludedFromMru(bool) override;
  bool CanMaximize() const override;
  bool CanMinimize() const override;
  bool CanResize() const override;
  bool CanActivate() const override;
  void StackChildAtTop(WmWindow* child) override;
  void StackChildAtBottom(WmWindow* child) override;
  void StackChildAbove(WmWindow* child, WmWindow* target) override;
  void StackChildBelow(WmWindow* child, WmWindow* target) override;
  std::vector<WmWindow*> GetChildren() override;
  void ShowResizeShadow(int component) override;
  void HideResizeShadow() override;
  void SetBoundsInScreenBehaviorForChildren(
      BoundsInScreenBehavior behavior) override;
  void SetSnapsChildrenToPhysicalPixelBoundary() override;
  void SnapToPixelBoundaryIfNecessary() override;
  void SetChildrenUseExtendedHitRegion() override;
  void SetDescendantsStayInSameRootWindow(bool value) override;
  void AddObserver(WmWindowObserver* observer) override;
  void RemoveObserver(WmWindowObserver* observer) override;
  bool HasObserver(const WmWindowObserver* observer) const override;

 private:
  // mus::WindowObserver:
  void OnTreeChanging(const TreeChangeParams& params) override;
  void OnTreeChanged(const TreeChangeParams& params) override;
  void OnWindowReordered(::mus::Window* window,
                         ::mus::Window* relative_window,
                         ::mus::mojom::OrderDirection direction) override;
  void OnWindowSharedPropertyChanged(
      ::mus::Window* window,
      const std::string& name,
      const std::vector<uint8_t>* old_data,
      const std::vector<uint8_t>* new_data) override;
  void OnWindowBoundsChanged(::mus::Window* window,
                             const gfx::Rect& old_bounds,
                             const gfx::Rect& new_bounds) override;
  void OnWindowDestroying(::mus::Window* window) override;
  void OnWindowDestroyed(::mus::Window* window) override;

  ::mus::Window* window_;

  // The shell window id of this window. Shell window ids are defined in
  // ash/common/shell_window_ids.h.
  int shell_window_id_ = kShellWindowId_Invalid;

  std::unique_ptr<wm::WindowState> window_state_;

  views::Widget* widget_ = nullptr;

  WidgetCreationType widget_creation_type_ = WidgetCreationType::INTERNAL;

  base::ObserverList<WmWindowObserver> observers_;

  std::unique_ptr<MusLayoutManagerAdapter> layout_manager_adapter_;

  std::unique_ptr<gfx::Rect> restore_bounds_in_screen_;

  ui::WindowShowState restore_show_state_ = ui::SHOW_STATE_DEFAULT;

  bool snap_children_to_pixel_boundary_ = false;

  // If true child windows should get a slightly larger hit region to make
  // resizing easier.
  bool children_use_extended_hit_region_ = false;

  DISALLOW_COPY_AND_ASSIGN(WmWindowMus);
};

}  // namespace mus
}  // namespace ash

#endif  // ASH_MUS_BRIDGE_WM_WINDOW_MUS_H_
