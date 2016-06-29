// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_AURA_WM_WINDOW_AURA_H_
#define ASH_AURA_WM_WINDOW_AURA_H_

#include "ash/ash_export.h"
#include "ash/common/wm_window.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "ui/aura/window_observer.h"

namespace ash {

// WmWindowAura is tied to the life of the underlying aura::Window.
class ASH_EXPORT WmWindowAura : public WmWindow, public aura::WindowObserver {
 public:
  explicit WmWindowAura(aura::Window* window);
  // NOTE: this class is owned by the corresponding window. You shouldn't delete
  // TODO(sky): friend deleter and make private.
  ~WmWindowAura() override;

  // Returns a WmWindow for an aura::Window, creating if necessary. |window| may
  // be null, in which case null is returned.
  static WmWindow* Get(aura::Window* window) {
    return const_cast<WmWindow*>(Get(const_cast<const aura::Window*>(window)));
  }
  static const WmWindow* Get(const aura::Window* window);

  static std::vector<WmWindow*> FromAuraWindows(
      const std::vector<aura::Window*>& aura_windows);
  static std::vector<aura::Window*> ToAuraWindows(
      const std::vector<WmWindow*>& windows);

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
  void SetExcludedFromMru(bool excluded_from_mru) override;
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
  // aura::WindowObserver:
  void OnWindowHierarchyChanging(const HierarchyChangeParams& params) override;
  void OnWindowHierarchyChanged(const HierarchyChangeParams& params) override;
  void OnWindowStackingChanged(aura::Window* window) override;
  void OnWindowPropertyChanged(aura::Window* window,
                               const void* key,
                               intptr_t old) override;
  void OnWindowBoundsChanged(aura::Window* window,
                             const gfx::Rect& old_bounds,
                             const gfx::Rect& new_bounds) override;
  void OnWindowDestroying(aura::Window* window) override;
  void OnWindowDestroyed(aura::Window* window) override;
  void OnWindowVisibilityChanging(aura::Window* window, bool visible) override;
  void OnWindowTitleChanged(aura::Window* window) override;

  aura::Window* window_;

  base::ObserverList<WmWindowObserver> observers_;

  DISALLOW_COPY_AND_ASSIGN(WmWindowAura);
};

}  // namespace ash

#endif  // ASH_AURA_WM_WINDOW_AURA_H_
