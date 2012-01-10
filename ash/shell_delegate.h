// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELL_DELEGATE_H_
#define ASH_SHELL_DELEGATE_H_
#pragma once

#include <vector>

#include "ash/ash_export.h"
#include "base/callback.h"

namespace aura {
class Window;
}

namespace gfx {
class Rect;
}

namespace views {
class Widget;
}

namespace ash {

class AppListModel;
class AppListViewDelegate;
struct LauncherItem;

// Delegate of the Shell.
class ASH_EXPORT ShellDelegate {
 public:
  // Callback to pass back a widget used by RequestAppListWidget.
  typedef base::Callback<void(views::Widget*)> SetWidgetCallback;

  // The Shell owns the delegate.
  virtual ~ShellDelegate() {}

  // Invoked when the user clicks on button in the launcher to create a new
  // window.
  virtual void CreateNewWindow() = 0;

  // Invoked to create a new status area. Can return NULL.
  virtual views::Widget* CreateStatusArea() = 0;

  // Invoked to create app list widget. The Delegate calls the callback
  // when the widget is ready to show.
  // Deprecated.
  // TODO(xiyuan): Clean this up when switching to views app list.
  virtual void RequestAppListWidget(
      const gfx::Rect& bounds,
      const SetWidgetCallback& callback) = 0;

  // Invoked to ask the delegate to populate the |model|.
  virtual void BuildAppListModel(AppListModel* model) = 0;

  // Invoked to create an AppListViewDelegate. Shell takes the ownership of
  // the created delegate.
  virtual AppListViewDelegate* CreateAppListViewDelegate() = 0;

  // Returns a list of windows to cycle with keyboard shortcuts (e.g. alt-tab
  // or the window switching key).  Windows should be in most-recently-used
  // order with the currently active window at the front of the list.  The list
  // does not contain NULL pointers.
  virtual std::vector<aura::Window*> GetCycleWindowList() const = 0;

  // Invoked when the user clicks on a window entry in the launcher.
  virtual void LauncherItemClicked(const LauncherItem& item) = 0;

  // Invoked when a window is added. If the delegate wants the launcher to show
  // an entry for |item->window| it should configure |item| appropriately and
  // return true.
  virtual bool ConfigureLauncherItem(LauncherItem* item) = 0;
};
}  // namespace ash

#endif  // ASH_SHELL_DELEGATE_H_
