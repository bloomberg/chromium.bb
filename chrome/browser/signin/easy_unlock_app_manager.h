// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_EASY_UNLOCK_APP_MANAGER_H_
#define CHROME_BROWSER_SIGNIN_EASY_UNLOCK_APP_MANAGER_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/macros.h"

namespace extensions {
class ExtensionSystem;
}

// Used to manage Easy Unlock app's lifetime and to dispatch events to the app.
// It's main purpose is to abstract extension system from the rest of easy
// unlock code.
class EasyUnlockAppManager {
 public:
  virtual ~EasyUnlockAppManager();

  // Creates EasyUnlockAppManager object that should be used in production.
  static std::unique_ptr<EasyUnlockAppManager> Create(
      extensions::ExtensionSystem* extension_system,
      int manifest_id,
      const base::FilePath& app_path);

  // Wait for the extension system to get ready and invokes |ready_callback|
  // when that happens.
  // Note that the callback may be triggered after |this| is deleted.
  virtual void EnsureReady(const base::Closure& ready_callback) = 0;

  // Launches Easy Unlock setup app, if the setup app is loaded.
  virtual void LaunchSetup() = 0;

  // Loads Easy Unlock app.
  virtual void LoadApp() = 0;

  // Disables Easy Unlock app.
  virtual void DisableAppIfLoaded() = 0;

  // Reloads Easy Unlock app.
  virtual void ReloadApp() = 0;

  // Sends easyUnlockPrivate.onUserInfoUpdate event to Easy Unlock app.
  virtual bool SendUserUpdatedEvent(const std::string& user_id,
                                    bool is_logged_in,
                                    bool data_ready) = 0;

  // Sends screenlockPrivate.onAuthAttempted event to Easy Unlock app.
  virtual bool SendAuthAttemptEvent() = 0;
};

#endif  // CHROME_BROWSER_SIGNIN_EASY_UNLOCK_APP_MANAGER_H_
