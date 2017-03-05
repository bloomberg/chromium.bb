// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_LAUNCHER_LAUNCHER_ITEM_CONTROLLER_H_
#define CHROME_BROWSER_UI_ASH_LAUNCHER_LAUNCHER_ITEM_CONTROLLER_H_

#include <string>

#include "ash/common/shelf/shelf_item_delegate.h"
#include "ash/common/shelf/shelf_item_types.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "ui/events/event.h"

class AppWindowLauncherItemController;
class ChromeLauncherController;

// LauncherItemController is used by ChromeLauncherController to track one
// or more windows associated with a shelf item.
// TODO (khmel): Consider using ash::AppLauncherId instead of pair
// |app_id| and |launch_id|.
class LauncherItemController : public ash::ShelfItemDelegate {
 public:
  LauncherItemController(const std::string& app_id,
                         const std::string& launch_id,
                         ChromeLauncherController* launcher_controller);
  ~LauncherItemController() override;

  ash::ShelfID shelf_id() const { return shelf_id_; }
  void set_shelf_id(ash::ShelfID id) { shelf_id_ = id; }
  const std::string& app_id() const { return app_id_; }
  const std::string& launch_id() const { return launch_id_; }
  ChromeLauncherController* launcher_controller() const {
    return launcher_controller_;
  }

  // Lock this item to the launcher without being pinned (windowed v1 apps).
  void lock() { locked_++; }
  void unlock() {
    DCHECK(locked_);
    locked_--;
  }
  bool locked() const { return locked_ > 0; }

  bool image_set_by_controller() const { return image_set_by_controller_; }
  void set_image_set_by_controller(bool image_set_by_controller) {
    image_set_by_controller_ = image_set_by_controller;
  }

  // Returns nullptr if class is not AppWindowLauncherItemController.
  virtual AppWindowLauncherItemController* AsAppWindowLauncherItemController();

 private:
  // The application id; empty if there is no app associated with the item.
  const std::string app_id_;

  // An id that can be passed to an app when launched in order to support
  // multiple shelf items per app. This id is used together with the app_id to
  // uniquely identify each shelf item that has the same app_id.
  const std::string launch_id_;

  // A unique id assigned by the shelf model for the shelf item.
  ash::ShelfID shelf_id_;

  ChromeLauncherController* launcher_controller_;

  // The lock counter which tells the launcher if the item can be removed from
  // the launcher (0) or not (>0). It is being used for windowed V1
  // applications.
  int locked_;

  // Set to true if the launcher item image has been set by the controller.
  bool image_set_by_controller_;

  DISALLOW_COPY_AND_ASSIGN(LauncherItemController);
};

#endif  // CHROME_BROWSER_UI_ASH_LAUNCHER_LAUNCHER_ITEM_CONTROLLER_H_
