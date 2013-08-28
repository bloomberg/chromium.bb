// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LAUNCHER_LAUNCHER_DELEGATE_H_
#define ASH_LAUNCHER_LAUNCHER_DELEGATE_H_

#include "ash/ash_export.h"
#include "ash/launcher/launcher_types.h"

namespace ash {
class Launcher;

// Delegate for the Launcher.
class ASH_EXPORT LauncherDelegate {
 public:
  // Launcher owns the delegate.
  virtual ~LauncherDelegate() {}

  // Returns the id of the item associated with the specified window, or 0 if
  // there isn't one.
  // Note: Windows of tabbed browsers will return the |LauncherID| of the
  // currently active tab or selected tab.
  virtual LauncherID GetIDByWindow(aura::Window* window) = 0;

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

  // Unpins app item with |app_id|.
  virtual void UnpinAppWithID(const std::string& app_id) = 0;
};

}  // namespace ash

#endif  // ASH_LAUNCHER_LAUNCHER_DELEGATE_H_
