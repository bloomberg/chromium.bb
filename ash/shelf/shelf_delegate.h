// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELF_SHELF_DELEGATE_H_
#define ASH_SHELF_SHELF_DELEGATE_H_

#include "ash/ash_export.h"
#include "ash/launcher/launcher_types.h"

namespace ash {
class Launcher;

// Delegate for the Launcher.
class ASH_EXPORT ShelfDelegate {
 public:
  // Launcher owns the delegate.
  virtual ~ShelfDelegate() {}

  // Callback used to allow delegate to perform initialization actions that
  // depend on the Launcher being in a known state.
  virtual void OnLauncherCreated(Launcher* launcher) = 0;

  // Callback used to inform the delegate that a specific launcher no longer
  // exists.
  virtual void OnLauncherDestroyed(Launcher* launcher) = 0;

  // Get the launcher ID from an application ID.
  virtual LauncherID GetLauncherIDForAppID(const std::string& app_id) = 0;

  // Get the application ID for a given launcher ID.
  virtual const std::string& GetAppIDForLauncherID(LauncherID id) = 0;

  // Pins an app with |app_id| to launcher. A running instance will get pinned.
  // In case there is no running instance a new launcher item is created and
  // pinned.
  virtual void PinAppWithID(const std::string& app_id) = 0;

  // Check if the app with |app_id_| is pinned to the launcher.
  virtual bool IsAppPinned(const std::string& app_id) = 0;

  // Checks whether the user is allowed to pin/unpin apps. Pinning may be
  // disallowed by policy in case there is a pre-defined set of pinned apps.
  virtual bool CanPin() const = 0;

  // Unpins app item with |app_id|.
  virtual void UnpinAppWithID(const std::string& app_id) = 0;
};

}  // namespace ash

#endif  // ASH_SHELF_SHELF_DELEGATE_H_
