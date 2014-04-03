// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_ALWAYS_ON_TOP_CONTROLLER_H_
#define ASH_WM_ALWAYS_ON_TOP_CONTROLLER_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "ui/aura/window_observer.h"

namespace aura {
class Window;
}

namespace ash {

// AlwaysOnTopController puts window into proper containers based on its
// 'AlwaysOnTop' property. That is, putting a window into the worskpace
// container if its "AlwaysOnTop" property is false. Otherwise, put it in
// |always_on_top_container_|.
class AlwaysOnTopController : public aura::WindowObserver {
 public:
  AlwaysOnTopController();
  virtual ~AlwaysOnTopController();

  // Sets the container for always on top windows.
  void SetAlwaysOnTopContainer(aura::Window* always_on_top_container);

  // Gets container for given |window| based on its "AlwaysOnTop" property.
  aura::Window* GetContainer(aura::Window* window) const;

 private:
  // Overridden from aura::WindowObserver:
  virtual void OnWindowAdded(aura::Window* child) OVERRIDE;
  virtual void OnWillRemoveWindow(aura::Window* child) OVERRIDE;
  virtual void OnWindowPropertyChanged(aura::Window* window,
                                       const void* key,
                                       intptr_t old) OVERRIDE;
  virtual void OnWindowDestroyed(aura::Window* window) OVERRIDE;

  aura::Window* always_on_top_container_;

  DISALLOW_COPY_AND_ASSIGN(AlwaysOnTopController);
};

}  // namepsace ash

#endif  // ASH_WM_ALWAYS_ON_TOP_CONTROLLER_H_
