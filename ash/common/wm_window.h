// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_WM_WINDOW_H_
#define ASH_COMMON_WM_WINDOW_H_

#include <memory>
#include <vector>

#include "ash/ash_export.h"
#include "base/strings/string16.h"
#include "base/time/time.h"
#include "ui/base/ui_base_types.h"
#include "ui/compositor/layer_animation_element.h"
#include "ui/wm/core/window_animations.h"
#include "ui/wm/public/window_types.h"

namespace display {
class Display;
}

namespace gfx {
class Point;
class Rect;
class Size;
class Transform;
}

namespace ui {
class Layer;
}

namespace views {
class Widget;
}

namespace ash {

class WmLayoutManager;
class WmRootWindowController;
class WmShell;
class WmWindowObserver;
enum class WmWindowProperty;

namespace wm {
class WMEvent;
class WindowState;
}

// This class exists as a porting layer to allow ash/wm to work with
// aura::Window or mus::Window. See aura::Window for details on the functions.
class ASH_EXPORT WmWindow {
 public:
  // See comments in SetBoundsInScreen().
  enum class BoundsInScreenBehavior {
    USE_LOCAL_COORDINATES,
    USE_SCREEN_COORDINATES,
  };

  using Windows = std::vector<WmWindow*>;

  WmWindow* GetRootWindow() {
    return const_cast<WmWindow*>(
        const_cast<const WmWindow*>(this)->GetRootWindow());
  }
  virtual const WmWindow* GetRootWindow() const = 0;
  virtual WmRootWindowController* GetRootWindowController() = 0;

  // TODO(sky): fix constness.
  virtual WmShell* GetShell() const = 0;

  // Used for debugging.
  virtual void SetName(const char* name) = 0;

  virtual base::string16 GetTitle() const = 0;

  // See shell_window_ids.h for list of known ids.
  virtual void SetShellWindowId(int id) = 0;
  virtual int GetShellWindowId() const = 0;
  virtual WmWindow* GetChildByShellWindowId(int id) = 0;

  virtual ui::wm::WindowType GetType() const = 0;

  // TODO(sky): seems like this shouldn't be exposed.
  virtual ui::Layer* GetLayer() = 0;

  virtual display::Display GetDisplayNearestWindow() = 0;

  virtual bool HasNonClientArea() = 0;
  virtual int GetNonClientComponent(const gfx::Point& location) = 0;

  virtual gfx::Point ConvertPointToTarget(const WmWindow* target,
                                          const gfx::Point& point) const = 0;
  virtual gfx::Point ConvertPointToScreen(const gfx::Point& point) const = 0;
  virtual gfx::Point ConvertPointFromScreen(const gfx::Point& point) const = 0;
  virtual gfx::Rect ConvertRectToScreen(const gfx::Rect& rect) const = 0;
  virtual gfx::Rect ConvertRectFromScreen(const gfx::Rect& rect) const = 0;

  virtual gfx::Size GetMinimumSize() const = 0;
  virtual gfx::Size GetMaximumSize() const = 0;

  // Returns the visibility requested by this window. IsVisible() takes into
  // account the visibility of the layer and ancestors, where as this tracks
  // whether Show() without a Hide() has been invoked.
  virtual bool GetTargetVisibility() const = 0;

  virtual bool IsVisible() const = 0;

  virtual void SetOpacity(float opacity) = 0;
  virtual float GetTargetOpacity() const = 0;

  virtual void SetTransform(const gfx::Transform& transform) = 0;
  virtual gfx::Transform GetTargetTransform() const = 0;

  virtual bool IsSystemModal() const = 0;

  virtual bool GetBoolProperty(WmWindowProperty key) = 0;
  virtual int GetIntProperty(WmWindowProperty key) = 0;

  wm::WindowState* GetWindowState() {
    return const_cast<wm::WindowState*>(
        const_cast<const WmWindow*>(this)->GetWindowState());
  }
  virtual const wm::WindowState* GetWindowState() const = 0;

  virtual WmWindow* GetToplevelWindow() = 0;

  // See aura::client::ParentWindowWithContext() for details of what this does.
  virtual void SetParentUsingContext(WmWindow* context,
                                     const gfx::Rect& screen_bounds) = 0;
  virtual void AddChild(WmWindow* window) = 0;

  virtual WmWindow* GetParent() = 0;

  WmWindow* GetTransientParent() {
    return const_cast<WmWindow*>(
        const_cast<const WmWindow*>(this)->GetTransientParent());
  }
  virtual const WmWindow* GetTransientParent() const = 0;
  virtual Windows GetTransientChildren() = 0;

  virtual void SetLayoutManager(
      std::unique_ptr<WmLayoutManager> layout_manager) = 0;
  virtual WmLayoutManager* GetLayoutManager() = 0;

  // |type| is WindowVisibilityAnimationType. Has to be an int to match aura.
  virtual void SetVisibilityAnimationType(int type) = 0;
  virtual void SetVisibilityAnimationDuration(base::TimeDelta delta) = 0;
  virtual void SetVisibilityAnimationTransition(
      ::wm::WindowVisibilityAnimationTransition transition) = 0;
  virtual void Animate(::wm::WindowAnimationType type) = 0;
  virtual void StopAnimatingProperty(
      ui::LayerAnimationElement::AnimatableProperty property) = 0;
  virtual void SetChildWindowVisibilityChangesAnimated() = 0;

  // See description in ui::Layer.
  virtual void SetMasksToBounds(bool value) = 0;

  virtual void SetBounds(const gfx::Rect& bounds) = 0;
  virtual void SetBoundsWithTransitionDelay(const gfx::Rect& bounds,
                                            base::TimeDelta delta) = 0;
  // Sets the bounds in such a way that LayoutManagers are circumvented.
  virtual void SetBoundsDirect(const gfx::Rect& bounds) = 0;
  virtual void SetBoundsDirectAnimated(const gfx::Rect& bounds) = 0;
  virtual void SetBoundsDirectCrossFade(const gfx::Rect& bounds) = 0;

  // Sets the bounds in two distinct ways. The exact behavior is dictated by
  // the value of BoundsInScreenBehavior set on the parent:
  //
  // USE_LOCAL_COORDINATES: the bounds are applied as is to the window. In other
  //   words this behaves the same as if SetBounds(bounds_in_screen) was used.
  //   This is the default.
  // USE_SCREEN_COORDINATES: the bounds are actual screen bounds and converted
  //   from the display. In this case the window may move to a different
  //   display if allowed (see SetDescendantsStayInSameRootWindow()).
  virtual void SetBoundsInScreen(const gfx::Rect& bounds_in_screen,
                                 const display::Display& dst_display) = 0;
  virtual gfx::Rect GetBoundsInScreen() const = 0;
  virtual const gfx::Rect& GetBounds() const = 0;
  virtual gfx::Rect GetTargetBounds() = 0;

  virtual void ClearRestoreBounds() = 0;
  virtual void SetRestoreBoundsInScreen(const gfx::Rect& bounds) = 0;
  virtual gfx::Rect GetRestoreBoundsInScreen() const = 0;

  virtual bool Contains(const WmWindow* other) const = 0;

  virtual void SetShowState(ui::WindowShowState show_state) = 0;
  virtual ui::WindowShowState GetShowState() const = 0;

  virtual void SetRestoreShowState(ui::WindowShowState show_state) = 0;

  // Sets the restore bounds and show state overrides. These values take
  // precedence over the restore bounds and restore show state (if set).
  // If |bounds_override| is empty the values are cleared.
  virtual void SetRestoreOverrides(
      const gfx::Rect& bounds_override,
      ui::WindowShowState window_state_override) = 0;

  // If |value| is true the window can not be moved to another root, regardless
  // of the bounds set on it.
  virtual void SetLockedToRoot(bool value) = 0;

  virtual void SetCapture() = 0;
  virtual bool HasCapture() = 0;
  virtual void ReleaseCapture() = 0;

  virtual bool HasRestoreBounds() const = 0;

  virtual void SetAlwaysOnTop(bool value) = 0;
  virtual bool IsAlwaysOnTop() const = 0;

  virtual void Hide() = 0;
  virtual void Show() = 0;

  // Returns the widget associated with this window, or null if not associated
  // with a widget. Only ash system UI widgets are returned, not widgets created
  // by the mus window manager code to show a non-client frame.
  virtual views::Widget* GetInternalWidget() = 0;

  // Requests the window to close and destroy itself. This is intended to
  // forward to an associated widget.
  virtual void CloseWidget() = 0;

  virtual bool IsFocused() const = 0;

  virtual bool IsActive() const = 0;
  virtual void Activate() = 0;
  virtual void Deactivate() = 0;

  virtual void SetFullscreen() = 0;

  virtual void Maximize() = 0;
  virtual void Minimize() = 0;
  virtual void Unminimize() = 0;
  virtual void SetExcludedFromMru(bool excluded_from_mru) = 0;

  virtual bool CanMaximize() const = 0;
  virtual bool CanMinimize() const = 0;
  virtual bool CanResize() const = 0;
  virtual bool CanActivate() const = 0;

  virtual void StackChildAtTop(WmWindow* child) = 0;
  virtual void StackChildAtBottom(WmWindow* child) = 0;
  virtual void StackChildAbove(WmWindow* child, WmWindow* target) = 0;
  virtual void StackChildBelow(WmWindow* child, WmWindow* target) = 0;

  virtual Windows GetChildren() = 0;

  // Shows/hides the resize shadow. |component| is the component to show the
  // shadow for (one of the constants in ui/base/hit_test.h).
  virtual void ShowResizeShadow(int component) = 0;
  virtual void HideResizeShadow() = 0;

  // See description in SetBoundsInScreen().
  virtual void SetBoundsInScreenBehaviorForChildren(BoundsInScreenBehavior) = 0;

  // See description of SnapToPixelBoundaryIfNecessary().
  virtual void SetSnapsChildrenToPhysicalPixelBoundary() = 0;

  // If an ancestor has been set to snap children to pixel boundaries, then
  // snaps the layer associated with this window to the layer associated with
  // the ancestor.
  virtual void SnapToPixelBoundaryIfNecessary() = 0;

  // Makes the hit region for children slightly larger for easier resizing.
  virtual void SetChildrenUseExtendedHitRegion() = 0;

  // Sets whether descendants of this should not be moved to a different
  // container. This is used by SetBoundsInScreen().
  virtual void SetDescendantsStayInSameRootWindow(bool value) = 0;

  virtual void AddObserver(WmWindowObserver* observer) = 0;
  virtual void RemoveObserver(WmWindowObserver* observer) = 0;
  virtual bool HasObserver(const WmWindowObserver* observer) const = 0;

 protected:
  virtual ~WmWindow() {}
};

}  // namespace ash

#endif  // ASH_COMMON_WM_WINDOW_H_
