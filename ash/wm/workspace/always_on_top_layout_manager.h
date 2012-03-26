// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WORKSPACE_ALWAYS_ON_TOP_LAYOUT_MANAGER_H_
#define ASH_WM_WORKSPACE_ALWAYS_ON_TOP_LAYOUT_MANAGER_H_
#pragma once

#include "ash/wm/base_layout_manager.h"
#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "ui/base/ui_base_types.h"

namespace aura {
class RootWindow;
class Window;
}

namespace ash {
namespace internal {

// LayoutManager for top level windows when WorkspaceManager is enabled.
class ASH_EXPORT AlwaysOnTopLayoutManager : public BaseLayoutManager {
 public:
  explicit AlwaysOnTopLayoutManager(aura::RootWindow* root_window);
  virtual ~AlwaysOnTopLayoutManager();

  // Overridden from BaseLayoutManager:
  virtual void OnChildWindowVisibilityChanged(aura::Window* child,
                                              bool visibile) OVERRIDE;

  // Overriden from aura::WindowObserver:
  virtual void OnWindowPropertyChanged(aura::Window* window,
                                       const void* key,
                                       intptr_t old) OVERRIDE;

 private:
  void ShowStateChanged(
      aura::Window* window,
      ui::WindowShowState last_show_state);

  DISALLOW_COPY_AND_ASSIGN(AlwaysOnTopLayoutManager);
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_WM_WORKSPACE_ALWAYS_ON_TOP_LAYOUT_MANAGER_H_
