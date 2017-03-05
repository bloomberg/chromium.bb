// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_SHELF_SHELF_DELEGATE_H_
#define ASH_COMMON_SHELF_SHELF_DELEGATE_H_

#include <string>

#include "ash/ash_export.h"
#include "ash/common/shelf/shelf_item_types.h"

namespace ash {

// Delegate shared by all shelf instances.
class ASH_EXPORT ShelfDelegate {
 public:
  virtual ~ShelfDelegate() {}

  // Get the shelf ID from an application ID.
  virtual ShelfID GetShelfIDForAppID(const std::string& app_id) = 0;

  // Get the shelf ID from an application ID and a launch ID.
  // The launch ID can be passed to an app when launched in order to support
  // multiple shelf items per app. This id is used together with the app_id to
  // uniquely identify each shelf item that has the same app_id.
  // For example, a single virtualization app might want to show different
  // shelf icons for different remote apps.
  virtual ShelfID GetShelfIDForAppIDAndLaunchID(
      const std::string& app_id,
      const std::string& launch_id) = 0;

  // Checks whether a mapping exists from the ShelfID |id| to an app id.
  virtual bool HasShelfIDToAppIDMapping(ShelfID id) const = 0;

  // Get the application ID for a given shelf ID.
  // |HasShelfIDToAppIDMapping(ShelfID)| should be called first to ensure the
  // ShelfID can be successfully mapped to an app id.
  virtual const std::string& GetAppIDForShelfID(ShelfID id) = 0;

  // Pins an app with |app_id| to shelf. A running instance will get pinned.
  // In case there is no running instance a new shelf item is created and
  // pinned.
  virtual void PinAppWithID(const std::string& app_id) = 0;

  // Check if the app with |app_id_| is pinned to the shelf.
  virtual bool IsAppPinned(const std::string& app_id) = 0;

  // Unpins app item with |app_id|.
  virtual void UnpinAppWithID(const std::string& app_id) = 0;
};

}  // namespace ash

#endif  // ASH_COMMON_SHELF_SHELF_DELEGATE_H_
