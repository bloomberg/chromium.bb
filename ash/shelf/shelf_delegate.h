// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELF_SHELF_DELEGATE_H_
#define ASH_SHELF_SHELF_DELEGATE_H_

#include "ash/ash_export.h"
#include "ash/common/shelf/shelf_item_types.h"

namespace ash {
class Shelf;

// Delegate for the Shelf.
class ASH_EXPORT ShelfDelegate {
 public:
  // Shelf owns the delegate.
  virtual ~ShelfDelegate() {}

  // Callback used to allow delegate to perform initialization actions that
  // depend on the Shelf being in a known state.
  virtual void OnShelfCreated(Shelf* shelf) = 0;

  // Callback used to inform the delegate that a specific shelf no longer
  // exists.
  virtual void OnShelfDestroyed(Shelf* shelf) = 0;

  // Called when |shelf|'s alignment changes.
  virtual void OnShelfAlignmentChanged(Shelf* shelf) = 0;

  // Called when |shelf|'s auto-hide behavior changes.
  virtual void OnShelfAutoHideBehaviorChanged(Shelf* shelf) = 0;

  // Called when |shelf|'s auto-hide state changes.
  virtual void OnShelfAutoHideStateChanged(Shelf* shelf) = 0;

  // Called when |shelf|'s visibility state is committed.
  virtual void OnShelfVisibilityStateChanged(Shelf* shelf) = 0;

  // Get the shelf ID from an application ID.
  virtual ShelfID GetShelfIDForAppID(const std::string& app_id) = 0;

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

#endif  // ASH_SHELF_SHELF_DELEGATE_H_
