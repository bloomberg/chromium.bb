// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_AURA_WM_WINDOW_AURA_H_
#define ASH_WM_AURA_WM_WINDOW_AURA_H_

#include "ash/wm/common/wm_window.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "ui/aura/window_observer.h"

namespace ash {
namespace wm {

// WmWindowAura is tied to the life of the underlying aura::Window.
class ASH_EXPORT WmWindowAura : public WmWindow, public aura::WindowObserver {
 public:
  explicit WmWindowAura(aura::Window* window);
  // NOTE: this class is owned by the corresponding window. You shouldn't delete
  // TODO(sky): friend deleter and make private.
  ~WmWindowAura() override;

  // Returns a WmWindow for an aura::Window, creating if necessary. |window| may
  // be null, in which case null is returned.
  static WmWindow* Get(aura::Window* window);

  static std::vector<WmWindow*> FromAuraWindows(
      const std::vector<aura::Window*>& aura_windows);

  static aura::Window* GetAuraWindow(WmWindow* wm_window) {
    return const_cast<aura::Window*>(
        GetAuraWindow(const_cast<const WmWindow*>(wm_window)));
  }
  static const aura::Window* GetAuraWindow(const WmWindow* wm_window);

  aura::Window* aura_window() { return window_; }
  const aura::Window* aura_window() const { return window_; }

  // WmWindow:
  const WmWindow* GetRootWindow() const override;
  WmRootWindowController* GetRootWindowController() override;
  WmGlobals* GetGlobals() const override;
  void SetName(const std::string& name) override;
  void SetShellWindowId(int id) override;
  int GetShellWindowId() override;
  WmWindow* GetChildByShellWindowId(int id) override;
  ui::wm::WindowType GetType() const override;
  ui::Layer* GetLayer() override;
  gfx::Display GetDisplayNearestWindow() override;
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
  bool GetBoolProperty(WmWindowProperty key) override;
  int GetIntProperty(WmWindowProperty key) override;
  const WindowState* GetWindowState() const override;
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
  void Animate(::wm::WindowAnimationType type) override;
  void SetBounds(const gfx::Rect& bounds) override;
  void SetBoundsWithTransitionDelay(const gfx::Rect& bounds,
                                    base::TimeDelta delta) override;
  void SetBoundsDirect(const gfx::Rect& bounds) override;
  void SetBoundsDirectAnimated(const gfx::Rect& bounds) override;
  void SetBoundsDirectCrossFade(const gfx::Rect& bounds) override;
  void SetBoundsInScreen(const gfx::Rect& bounds_in_screen,
                         const gfx::Display& dst_display) override;
  gfx::Rect GetBoundsInScreen() const override;
  const gfx::Rect& GetBounds() const override;
  gfx::Rect GetTargetBounds() override;
  void ClearRestoreBounds() override;
  void SetRestoreBoundsInScreen(const gfx::Rect& bounds) override;
  gfx::Rect GetRestoreBoundsInScreen() const override;
  void OnWMEvent(const wm::WMEvent* event) override;
  bool Contains(const WmWindow* other) const override;
  void SetShowState(ui::WindowShowState show_state) override;
  ui::WindowShowState GetShowState() const override;
  void SetRestoreShowState(ui::WindowShowState show_state) override;
  void SetLockedToRoot(bool value) override;
  void SetCapture() override;
  bool HasCapture() override;
  void ReleaseCapture() override;
  bool HasRestoreBounds() const override;
  bool CanMaximize() const override;
  bool CanMinimize() const override;
  bool CanResize() const override;
  bool CanActivate() const override;
  void StackChildAtTop(WmWindow* child) override;
  void StackChildAtBottom(WmWindow* child) override;
  void StackChildAbove(WmWindow* child, WmWindow* target) override;
  void StackChildBelow(WmWindow* child, WmWindow* target) override;
  void SetAlwaysOnTop(bool value) override;
  bool IsAlwaysOnTop() const override;
  void Hide() override;
  void Show() override;
  bool IsFocused() const override;
  bool IsActive() const override;
  void Activate() override;
  void Deactivate() override;
  void Maximize() override;
  void Minimize() override;
  void Unminimize() override;
  std::vector<WmWindow*> GetChildren() override;
  void SnapToPixelBoundaryIfNecessary() override;
  void AddObserver(WmWindowObserver* observer) override;
  void RemoveObserver(WmWindowObserver* observer) override;

 private:
  // aura::WindowObserver:
  void OnWindowHierarchyChanged(const HierarchyChangeParams& params) override;
  void OnWindowStackingChanged(aura::Window* window) override;
  void OnWindowPropertyChanged(aura::Window* window,
                               const void* key,
                               intptr_t old) override;
  void OnWindowBoundsChanged(aura::Window* window,
                             const gfx::Rect& old_bounds,
                             const gfx::Rect& new_bounds) override;
  void OnWindowDestroying(aura::Window* window) override;
  void OnWindowVisibilityChanging(aura::Window* window, bool visible) override;

  aura::Window* window_;

  base::ObserverList<WmWindowObserver> observers_;

  DISALLOW_COPY_AND_ASSIGN(WmWindowAura);
};

}  // namespace wm
}  // namespace ash

#endif  // ASH_WM_AURA_WM_WINDOW_AURA_H_
