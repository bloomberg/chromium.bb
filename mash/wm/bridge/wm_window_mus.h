// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MASH_WM_BRIDGE_WM_WINDOW_MUS_H_
#define MASH_WM_BRIDGE_WM_WINDOW_MUS_H_

#include <memory>

#include "ash/wm/common/wm_window.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "components/mus/public/cpp/window_observer.h"

namespace ash {
namespace wm {
class WmLayoutManager;
}
}

namespace views {
class Widget;
}

namespace mash {
namespace wm {

class MusLayoutManagerAdapter;
class WmRootWindowControllerMus;

// WmWindow implementation for mus.
//
// WmWindowMus is tied to the life of the underlying mus::Window (it is stored
// as an owned property).
class WmWindowMus : public ash::wm::WmWindow, public mus::WindowObserver {
 public:
  explicit WmWindowMus(mus::Window* window);
  // NOTE: this class is owned by the corresponding window. You shouldn't delete
  // TODO(sky): friend deleter and make private.
  ~WmWindowMus() override;

  // Returns a WmWindow for an mus::Window, creating if necessary.
  static WmWindowMus* Get(mus::Window* window);

  static WmWindowMus* Get(views::Widget* widget);

  static mus::Window* GetMusWindow(ash::wm::WmWindow* wm_window) {
    return const_cast<mus::Window*>(
        GetMusWindow(const_cast<const ash::wm::WmWindow*>(wm_window)));
  }
  static const mus::Window* GetMusWindow(const ash::wm::WmWindow* wm_window);

  static std::vector<WmWindow*> FromMusWindows(
      const std::vector<mus::Window*>& mus_windows);

  // Sets the widget associated with the window. The widget is used to query
  // state, such as min/max size. The widget is not owned by the WmWindowMus.
  void set_widget(views::Widget* widget) { widget_ = widget; }

  mus::Window* mus_window() { return window_; }
  const mus::Window* mus_window() const { return window_; }

  WmRootWindowControllerMus* GetRootWindowControllerMus() {
    return const_cast<WmRootWindowControllerMus*>(
        const_cast<const WmWindowMus*>(this)->GetRootWindowControllerMus());
  }
  const WmRootWindowControllerMus* GetRootWindowControllerMus() const;

  static WmWindowMus* AsWmWindowMus(ash::wm::WmWindow* window) {
    return static_cast<WmWindowMus*>(window);
  }
  static const WmWindowMus* AsWmWindowMus(const ash::wm::WmWindow* window) {
    return static_cast<const WmWindowMus*>(window);
  }

  // WmWindow:
  const ash::wm::WmWindow* GetRootWindow() const override;
  ash::wm::WmRootWindowController* GetRootWindowController() override;
  ash::wm::WmGlobals* GetGlobals() const override;
  void SetShellWindowId(int id) override;
  int GetShellWindowId() const override;
  ash::wm::WmWindow* GetChildByShellWindowId(int id) override;
  ui::wm::WindowType GetType() const override;
  ui::Layer* GetLayer() override;
  display::Display GetDisplayNearestWindow() override;
  bool HasNonClientArea() override;
  int GetNonClientComponent(const gfx::Point& location) override;
  gfx::Point ConvertPointToTarget(const ash::wm::WmWindow* target,
                                  const gfx::Point& point) const override;
  gfx::Point ConvertPointToScreen(const gfx::Point& point) const override;
  gfx::Point ConvertPointFromScreen(const gfx::Point& point) const override;
  gfx::Rect ConvertRectToScreen(const gfx::Rect& rect) const override;
  gfx::Rect ConvertRectFromScreen(const gfx::Rect& rect) const override;
  gfx::Size GetMinimumSize() const override;
  gfx::Size GetMaximumSize() const override;
  bool GetTargetVisibility() const override;
  bool IsVisible() const override;
  bool IsSystemModal() const override;
  bool GetBoolProperty(ash::wm::WmWindowProperty key) override;
  int GetIntProperty(ash::wm::WmWindowProperty key) override;
  const ash::wm::WindowState* GetWindowState() const override;
  ash::wm::WmWindow* GetToplevelWindow() override;
  void SetParentUsingContext(ash::wm::WmWindow* context,
                             const gfx::Rect& screen_bounds) override;
  void AddChild(ash::wm::WmWindow* window) override;
  ash::wm::WmWindow* GetParent() override;
  const ash::wm::WmWindow* GetTransientParent() const override;
  std::vector<ash::wm::WmWindow*> GetTransientChildren() override;
  void SetLayoutManager(
      std::unique_ptr<ash::wm::WmLayoutManager> layout_manager) override;
  ash::wm::WmLayoutManager* GetLayoutManager() override;
  void SetVisibilityAnimationType(int type) override;
  void SetVisibilityAnimationDuration(base::TimeDelta delta) override;
  void Animate(::wm::WindowAnimationType type) override;
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
  bool Contains(const ash::wm::WmWindow* other) const override;
  void SetShowState(ui::WindowShowState show_state) override;
  ui::WindowShowState GetShowState() const override;
  void SetRestoreShowState(ui::WindowShowState show_state) override;
  void SetLockedToRoot(bool value) override;
  void SetCapture() override;
  bool HasCapture() override;
  void ReleaseCapture() override;
  bool HasRestoreBounds() const override;
  void SetAlwaysOnTop(bool value) override;
  bool IsAlwaysOnTop() const override;
  void Hide() override;
  void Show() override;
  bool IsFocused() const override;
  bool IsActive() const override;
  void Activate() override;
  void Deactivate() override;
  void SetFullscreen() override;
  void Maximize() override;
  void Minimize() override;
  void Unminimize() override;
  bool CanMaximize() const override;
  bool CanMinimize() const override;
  bool CanResize() const override;
  bool CanActivate() const override;
  void StackChildAtTop(ash::wm::WmWindow* child) override;
  void StackChildAtBottom(ash::wm::WmWindow* child) override;
  void StackChildAbove(ash::wm::WmWindow* child,
                       ash::wm::WmWindow* target) override;
  void StackChildBelow(ash::wm::WmWindow* child,
                       ash::wm::WmWindow* target) override;
  std::vector<ash::wm::WmWindow*> GetChildren() override;
  void SnapToPixelBoundaryIfNecessary() override;
  void AddObserver(ash::wm::WmWindowObserver* observer) override;
  void RemoveObserver(ash::wm::WmWindowObserver* observer) override;

 private:
  void NotifyStackingChanged();

  // mus::WindowObserver:
  void OnTreeChanged(const TreeChangeParams& params) override;
  void OnWindowReordered(mus::Window* window,
                         mus::Window* relative_window,
                         mus::mojom::OrderDirection direction) override;
  void OnWindowSharedPropertyChanged(
      mus::Window* window,
      const std::string& name,
      const std::vector<uint8_t>* old_data,
      const std::vector<uint8_t>* new_data) override;
  void OnWindowBoundsChanged(mus::Window* window,
                             const gfx::Rect& old_bounds,
                             const gfx::Rect& new_bounds) override;
  void OnWindowDestroying(mus::Window* window) override;

  mus::Window* window_;
  std::unique_ptr<ash::wm::WindowState> window_state_;

  views::Widget* widget_ = nullptr;

  base::ObserverList<ash::wm::WmWindowObserver> observers_;

  std::unique_ptr<MusLayoutManagerAdapter> layout_manager_adapter_;

  std::unique_ptr<gfx::Rect> restore_bounds_in_screen_;

  ui::WindowShowState restore_show_state_ = ui::SHOW_STATE_DEFAULT;

  DISALLOW_COPY_AND_ASSIGN(WmWindowMus);
};

}  // namespace wm
}  // namespace mash

#endif  // MASH_WM_BRIDGE_WM_WINDOW_MUS_H_
