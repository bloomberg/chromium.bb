// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_APP_REGISTRY_CONTROLLER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_APP_REGISTRY_CONTROLLER_H_

#include "base/callback_forward.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"

class Profile;

namespace web_app {

class WebAppSyncBridge;

// A unified sync and storage controller.
//
// While AppRegistrar is a read-only model, AppRegistryController is a
// controller for that model. AppRegistryController is responsible for:
// - Registry initialization (reading model from a persistent storage like
// LevelDb or prefs).
// - Writing all the registry updates to a persistent store and sync.
//
// For the new web apps platform, AppRegistryController is implemented as
// WebAppSyncBridge: the key class to support integration with Unified Sync and
// Storage (USS) system.
class AppRegistryController {
 public:
  explicit AppRegistryController(Profile* profile);
  virtual ~AppRegistryController();

  virtual void Init(base::OnceClosure callback) = 0;

  virtual void SetAppLaunchContainer(const AppId& app_id,
                                     LaunchContainer launch_container) = 0;

  // Safe downcast:
  virtual WebAppSyncBridge* AsWebAppSyncBridge() = 0;

 protected:
  Profile* profile() const { return profile_; }

 private:
  Profile* profile_;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_COMPONENTS_APP_REGISTRY_CONTROLLER_H_
