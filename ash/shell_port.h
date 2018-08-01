// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELL_PORT_H_
#define ASH_SHELL_PORT_H_

#include <stdint.h>

#include <memory>
#include <vector>

#include "ash/ash_export.h"
#include "services/viz/public/interfaces/compositing/video_detector_observer.mojom.h"
#include "ui/aura/client/window_types.h"
#include "ui/base/ui_base_types.h"

namespace gfx {
class Point;
}

namespace views {
class PointerWatcher;
enum class PointerWatcherEventTypes;
}

namespace ash {
class RootWindowController;

enum class Config;

// Porting layer for Shell. This class contains the part of Shell that are
// different in classic ash and mus/mash.
// DEPRECATED: Being removed, since there is no longer a distinct "classic"
// config in ash. See https://crbug.com/866523
class ASH_EXPORT ShellPort {
 public:
  virtual ~ShellPort();

  static ShellPort* Get();
  static bool HasInstance() { return instance_ != nullptr; }

  virtual void Shutdown();

  virtual Config GetAshConfig() const = 0;

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

  // True if any touch points are down.
  virtual bool IsTouchDown() = 0;

  // TODO(jamescook): Remove this when VirtualKeyboardController has been moved.
  virtual void ToggleIgnoreExternalKeyboard() = 0;

  virtual void CreatePointerWatcherAdapter() = 0;

  // Called after the containers of |root_window_controller| have been created.
  // Allows ShellPort to install any additional state on the containers.
  virtual void OnCreatedRootWindowContainers(
      RootWindowController* root_window_controller) = 0;

  // Called any time the set up system modal and blocking containers needs to
  // sent to the server.
  virtual void UpdateSystemModalAndBlockingContainers() = 0;

  // Adds an observer for viz::VideoDetector.
  virtual void AddVideoDetectorObserver(
      viz::mojom::VideoDetectorObserverPtr observer) = 0;

 protected:
  ShellPort();

  // Called after WindowTreeHostManager::InitHosts().
  virtual void OnHostsInitialized() = 0;

 private:
  friend class Shell;

  static ShellPort* instance_;
};

}  // namespace ash

#endif  // ASH_SHELL_PORT_H_
