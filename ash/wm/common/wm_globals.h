// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_COMMON_WM_GLOBALS_H_
#define ASH_WM_COMMON_WM_GLOBALS_H_

#include <stdint.h>

#include <memory>
#include <vector>

#include "ash/wm/common/ash_wm_common_export.h"

namespace gfx {
class Rect;
}

namespace ash {

class WindowResizer;

namespace wm {

class WindowState;
class WmActivationObserver;
class WmDisplayObserver;
class WmOverviewModeObserver;
class WmWindow;

enum class WmUserMetricsAction;

// Used for accessing global state.
class ASH_WM_COMMON_EXPORT WmGlobals {
 public:
  virtual ~WmGlobals() {}

  // This is necessary for a handful of places that is difficult to plumb
  // through context.
  static void Set(WmGlobals* instance);
  static WmGlobals* Get();

  virtual WmWindow* GetFocusedWindow() = 0;
  virtual WmWindow* GetActiveWindow() = 0;

  // Returns the root window for the specified display.
  virtual WmWindow* GetRootWindowForDisplayId(int64_t display_id) = 0;

  // Returns the root window that newly created windows should be added to.
  // NOTE: this returns the root, newly created window should be added to the
  // appropriate container in the returned window.
  virtual WmWindow* GetRootWindowForNewWindows() = 0;

  // returns the list of more recently used windows excluding modals.
  virtual std::vector<WmWindow*> GetMruWindowListIgnoreModals() = 0;

  // Returns true if the first window shown on first run should be
  // unconditionally maximized, overriding the heuristic that normally chooses
  // the window size.
  virtual bool IsForceMaximizeOnFirstRun() = 0;

  virtual bool IsUserSessionBlocked() = 0;
  virtual bool IsScreenLocked() = 0;

  // See aura::client::CursorClient for details on these.
  virtual void LockCursor() = 0;
  virtual void UnlockCursor() = 0;

  virtual std::vector<WmWindow*> GetAllRootWindows() = 0;

  virtual void RecordUserMetricsAction(WmUserMetricsAction action) = 0;

  // Returns a WindowResizer to handle dragging. |next_window_resizer| is
  // the next WindowResizer in the WindowResizer chain. This may return
  // |next_window_resizer|.
  virtual std::unique_ptr<WindowResizer> CreateDragWindowResizer(
      std::unique_ptr<WindowResizer> next_window_resizer,
      wm::WindowState* window_state) = 0;

  // TODO(sky): if WindowSelectorController can't be moved over, move these
  // onto their own local class.
  virtual bool IsOverviewModeSelecting() = 0;
  virtual bool IsOverviewModeRestoringMinimizedWindows() = 0;

  virtual void AddActivationObserver(WmActivationObserver* observer) = 0;
  virtual void RemoveActivationObserver(WmActivationObserver* observer) = 0;

  virtual void AddDisplayObserver(WmDisplayObserver* observer) = 0;
  virtual void RemoveDisplayObserver(WmDisplayObserver* observer) = 0;

  virtual void AddOverviewModeObserver(WmOverviewModeObserver* observer) = 0;
  virtual void RemoveOverviewModeObserver(WmOverviewModeObserver* observer) = 0;

 private:
  static WmGlobals* instance_;
};

}  // namespace wm
}  // namespace ash

#endif  // ASH_WM_COMMON_WM_GLOBALS_H_
