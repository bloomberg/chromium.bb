// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_COMMON_WM_WINDOW_H_
#define ASH_WM_COMMON_WM_WINDOW_H_

#include <memory>
#include <vector>

#include "ash/wm/common/ash_wm_common_export.h"
#include "base/time/time.h"
#include "ui/base/ui_base_types.h"
#include "ui/wm/core/window_animations.h"
#include "ui/wm/public/window_types.h"

namespace gfx {
class Display;
class Point;
class Rect;
class Size;
}

namespace display {
using Display = gfx::Display;
}

namespace ui {
class Layer;
}

namespace ash {
namespace wm {

class WMEvent;
class WmGlobals;
class WmLayoutManager;
class WmRootWindowController;
class WmWindowObserver;
enum class WmWindowProperty;
class WindowState;

// This class exists as a porting layer to allow ash/wm to work with
// aura::Window
// or mus::Window. See aura::Window for details on the functions.
class ASH_WM_COMMON_EXPORT WmWindow {
 public:
  WmWindow* GetRootWindow() {
    return const_cast<WmWindow*>(
        const_cast<const WmWindow*>(this)->GetRootWindow());
  }
  virtual const WmWindow* GetRootWindow() const = 0;
  virtual WmRootWindowController* GetRootWindowController() = 0;

  // TODO(sky): fix constness.
  virtual WmGlobals* GetGlobals() const = 0;

  // See wm_shell_window_ids.h for list of known ids.
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

  virtual bool IsSystemModal() const = 0;

  virtual bool GetBoolProperty(WmWindowProperty key) = 0;
  virtual int GetIntProperty(WmWindowProperty key) = 0;

  WindowState* GetWindowState() {
    return const_cast<WindowState*>(
        const_cast<const WmWindow*>(this)->GetWindowState());
  }
  virtual const WindowState* GetWindowState() const = 0;

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
  virtual std::vector<WmWindow*> GetTransientChildren() = 0;

  virtual void SetLayoutManager(
      std::unique_ptr<WmLayoutManager> layout_manager) = 0;
  virtual WmLayoutManager* GetLayoutManager() = 0;

  // |type| is WindowVisibilityAnimationType. Has to be an int to match aura.
  virtual void SetVisibilityAnimationType(int type) = 0;
  virtual void SetVisibilityAnimationDuration(base::TimeDelta delta) = 0;
  virtual void Animate(::wm::WindowAnimationType type) = 0;

  virtual void SetBounds(const gfx::Rect& bounds) = 0;
  virtual void SetBoundsWithTransitionDelay(const gfx::Rect& bounds,
                                            base::TimeDelta delta) = 0;
  // Sets the bounds in such a way that LayoutManagers are circumvented.
  virtual void SetBoundsDirect(const gfx::Rect& bounds) = 0;
  virtual void SetBoundsDirectAnimated(const gfx::Rect& bounds) = 0;
  virtual void SetBoundsDirectCrossFade(const gfx::Rect& bounds) = 0;
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

  virtual bool IsFocused() const = 0;

  virtual bool IsActive() const = 0;
  virtual void Activate() = 0;
  virtual void Deactivate() = 0;

  virtual void SetFullscreen() = 0;

  virtual void Maximize() = 0;
  virtual void Minimize() = 0;
  virtual void Unminimize() = 0;

  virtual bool CanMaximize() const = 0;
  virtual bool CanMinimize() const = 0;
  virtual bool CanResize() const = 0;
  virtual bool CanActivate() const = 0;

  virtual void StackChildAtTop(WmWindow* child) = 0;
  virtual void StackChildAtBottom(WmWindow* child) = 0;
  virtual void StackChildAbove(WmWindow* child, WmWindow* target) = 0;
  virtual void StackChildBelow(WmWindow* child, WmWindow* target) = 0;

  virtual std::vector<WmWindow*> GetChildren() = 0;

  // If an ancestor has been set to snap children to pixel boundaries, then
  // snaps the layer associated with this window to the layer associated with
  // the ancestor.
  virtual void SnapToPixelBoundaryIfNecessary() = 0;

  virtual void AddObserver(WmWindowObserver* observer) = 0;
  virtual void RemoveObserver(WmWindowObserver* observer) = 0;

 protected:
  virtual ~WmWindow() {}
};

}  // namespace wm
}  // namespace ash

#endif  // ASH_WM_COMMON_WM_WINDOW_H_
