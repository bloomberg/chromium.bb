// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LAUNCHER_LAUNCHER_H_
#define ASH_LAUNCHER_LAUNCHER_H_
#pragma once

#include <map>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "ui/aura/window_observer.h"
#include "ash/ash_export.h"

namespace aura {
class Window;
}

namespace views {
class Widget;
}

namespace ash {

class LauncherModel;

class ASH_EXPORT Launcher : public aura::WindowObserver {
 public:
  explicit Launcher(aura::Window* window_container);
  ~Launcher();

  // Sets the width of the status area.
  void SetStatusWidth(int width);
  int GetStatusWidth();

  LauncherModel* model() { return model_.get(); }
  views::Widget* widget() { return widget_; }

 private:
  class DelegateView;

  typedef std::map<aura::Window*, bool> WindowMap;

  // If necessary asks the delegate if an entry should be created in the
  // launcher for |window|. This only asks the delegate once for a window.
  void MaybeAdd(aura::Window* window);

  // WindowObserver overrides:
  virtual void OnWindowAdded(aura::Window* new_window) OVERRIDE;
  virtual void OnWillRemoveWindow(aura::Window* window) OVERRIDE;
  virtual void OnWindowVisibilityChanged(aura::Window* window,
                                         bool visibile) OVERRIDE;

  scoped_ptr<LauncherModel> model_;

  // Widget hosting the view.  May be hidden if we're not using a launcher,
  // e.g. Aura compact window mode.
  views::Widget* widget_;

  aura::Window* window_container_;

  // The set of windows we know about. The boolean indicates whether we've asked
  // the delegate if the window should added to the launcher.
  WindowMap known_windows_;

  // Contents view of the widget. Houses the LauncherView.
  DelegateView* delegate_view_;

  DISALLOW_COPY_AND_ASSIGN(Launcher);
};

}  // namespace ash

#endif  // ASH_LAUNCHER_LAUNCHER_H_
