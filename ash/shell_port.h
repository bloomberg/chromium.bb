// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELL_PORT_H_
#define ASH_SHELL_PORT_H_

#include "ash/ash_export.h"
#include "ui/base/ui_base_types.h"

namespace gfx {
class Point;
}

namespace views {
class PointerWatcher;
enum class PointerWatcherEventTypes;
}

namespace ash {

// Porting layer for Shell. This class contains the part of Shell that are
// different in classic ash and mus/mash.
// DEPRECATED: Being removed, since there is no longer a distinct "classic"
// config in ash. See https://crbug.com/866523
class ASH_EXPORT ShellPort {
 public:
  virtual ~ShellPort();

  static ShellPort* Get();

  virtual void Shutdown();

  // Shows the context menu for the wallpaper or shelf at |location_in_screen|.
  void ShowContextMenu(const gfx::Point& location_in_screen,
                       ui::MenuSourceType source_type);

  // If |events| is PointerWatcherEventTypes::MOVES,
  // PointerWatcher::OnPointerEventObserved() is called for pointer move events.
  // If |events| is PointerWatcherEventTypes::DRAGS,
  // PointerWatcher::OnPointerEventObserved() is called for pointer drag events.
  // Requesting pointer moves or drags may incur a performance hit and should be
  // avoided if possible.
  virtual void AddPointerWatcher(views::PointerWatcher* watcher,
                                 views::PointerWatcherEventTypes events) = 0;
  virtual void RemovePointerWatcher(views::PointerWatcher* watcher) = 0;

  virtual void CreatePointerWatcherAdapter() = 0;

 protected:
  ShellPort();

 private:
  friend class Shell;

  static ShellPort* instance_;
};

}  // namespace ash

#endif  // ASH_SHELL_PORT_H_
