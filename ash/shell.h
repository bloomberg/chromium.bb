// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELL_H_
#define ASH_SHELL_H_
#pragma once

#include <utility>
#include <vector>

#include "ash/ash_export.h"
#include "base/basictypes.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/task.h"
#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"

class CommandLine;

namespace aura {
class EventFilter;
class RootWindow;
class Window;
}
namespace gfx {
class Rect;
class Size;
}

namespace ash {

class AcceleratorController;
class Launcher;
class PowerButtonController;
class ShellDelegate;

namespace internal {
class ActivationController;
class AcceleratorFilter;
class AppList;
class DragDropController;
class InputMethodEventFilter;
class ShadowController;
class StackingController;
class TooltipController;
class WorkspaceController;
}

// Shell is a singleton object that presents the Shell API and implements the
// RootWindow's delegate interface.
class ASH_EXPORT Shell {
 public:
  // Upon creation, the Shell sets itself as the RootWindow's delegate, which
  // takes ownership of the Shell.

  // A shell must be explicitly created so that it can call |Init()| with the
  // delegate set. |delegate| can be NULL (if not required for initialization).
  static Shell* CreateInstance(ShellDelegate* delegate);

  // Should never be called before |CreateInstance()|.
  static Shell* GetInstance();

  static void DeleteInstance();

  aura::Window* GetContainer(int container_id);
  const aura::Window* GetContainer(int container_id) const;

  // Adds or removes |filter| from the RootWindowEventFilter.
  void AddRootWindowEventFilter(aura::EventFilter* filter);
  void RemoveRootWindowEventFilter(aura::EventFilter* filter);
  size_t GetRootWindowEventFilterCount() const;

  // Toggles between overview mode and normal mode.
  void ToggleOverview();

  // Toggles app list.
  void ToggleAppList();

  // Returns true if the screen is locked.
  bool IsScreenLocked() const;

  AcceleratorController* accelerator_controller() {
    return accelerator_controller_.get();
  }
  internal::TooltipController* tooltip_controller() {
    return tooltip_controller_.get();
  }
  PowerButtonController* power_button_controller() {
    return power_button_controller_.get();
  }

  ShellDelegate* delegate() { return delegate_.get(); }

  // May return NULL if we're not using a launcher (e.g. laptop-mode).
  Launcher* launcher() { return launcher_.get(); }

  // Made available for tests.
  internal::ShadowController* shadow_controller() {
    return shadow_controller_.get();
  }

 private:
  FRIEND_TEST_ALL_PREFIXES(ShellTest, DefaultToCompactWindowMode);

  typedef std::pair<aura::Window*, gfx::Rect> WindowAndBoundsPair;

  explicit Shell(ShellDelegate* delegate);
  virtual ~Shell();

  void Init();

  // Returns true if the |monitor_size| is narrow and the user has not set
  // an explicit window mode flag on the |command_line|.
  bool DefaultToCompactWindowMode(const gfx::Size& monitor_size,
                                  CommandLine* command_line) const;

  void InitLayoutManagers(aura::RootWindow* root_window);

  // Enables WorkspaceManager.
  void EnableWorkspaceManager();

  static Shell* instance_;

  std::vector<WindowAndBoundsPair> to_restore_;

  base::WeakPtrFactory<Shell> method_factory_;

  scoped_ptr<AcceleratorController> accelerator_controller_;

  scoped_ptr<ShellDelegate> delegate_;

  scoped_ptr<Launcher> launcher_;

  scoped_ptr<internal::AppList> app_list_;

  scoped_ptr<internal::StackingController> stacking_controller_;
  scoped_ptr<internal::ActivationController> activation_controller_;
  scoped_ptr<internal::DragDropController> drag_drop_controller_;
  scoped_ptr<internal::WorkspaceController> workspace_controller_;
  scoped_ptr<internal::ShadowController> shadow_controller_;
  scoped_ptr<internal::TooltipController> tooltip_controller_;
  scoped_ptr<PowerButtonController> power_button_controller_;

  // An event filter that pre-handles all key events to send them to an IME.
  scoped_ptr<internal::InputMethodEventFilter> input_method_filter_;
  // An event filter that pre-handles global accelerators.
  scoped_ptr<internal::AcceleratorFilter> accelerator_filter_;


  DISALLOW_COPY_AND_ASSIGN(Shell);
};

}  // namespace ash

#endif  // ASH_SHELL_H_
