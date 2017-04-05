// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_SHELF_ITEM_DELEGATE_H_
#define ASH_PUBLIC_CPP_SHELF_ITEM_DELEGATE_H_

#include <string>

#include "ash/public/cpp/app_launch_id.h"
#include "ash/public/cpp/ash_public_export.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/public/interfaces/shelf.mojom.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "ui/events/event.h"

class AppWindowLauncherItemController;

namespace ash {

using MenuItemList = std::vector<mojom::MenuItemPtr>;

// ShelfItemDelegate tracks some item state and serves as a base class for
// various subclasses that implement the mojo interface.
class ASH_PUBLIC_EXPORT ShelfItemDelegate : public mojom::ShelfItemDelegate {
 public:
  explicit ShelfItemDelegate(const AppLaunchId& app_launch_id);
  ~ShelfItemDelegate() override;

  ShelfID shelf_id() const { return shelf_id_; }
  void set_shelf_id(ShelfID id) { shelf_id_ = id; }
  const AppLaunchId& app_launch_id() const { return app_launch_id_; }
  const std::string& app_id() const { return app_launch_id_.app_id(); }
  const std::string& launch_id() const { return app_launch_id_.launch_id(); }

  bool image_set_by_controller() const { return image_set_by_controller_; }
  void set_image_set_by_controller(bool image_set_by_controller) {
    image_set_by_controller_ = image_set_by_controller;
  }

  // Returns items for the application menu; used for convenience and testing.
  virtual MenuItemList GetAppMenuItems(int event_flags);

  // Returns nullptr if class is not AppWindowLauncherItemController.
  virtual AppWindowLauncherItemController* AsAppWindowLauncherItemController();

 private:
  // The app launch id; empty if there is no app associated with the item.
  // Besides the application id, AppLaunchId also contains a launch id, which is
  // an id that can be passed to an app when launched in order to support
  // multiple shelf items per app. This id is used together with the app_id to
  // uniquely identify each shelf item that has the same app_id.
  const AppLaunchId app_launch_id_;

  // A unique id assigned by the shelf model for the shelf item.
  ShelfID shelf_id_;

  // Set to true if the launcher item image has been set by the controller.
  bool image_set_by_controller_;

  DISALLOW_COPY_AND_ASSIGN(ShelfItemDelegate);
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_SHELF_ITEM_DELEGATE_H_
