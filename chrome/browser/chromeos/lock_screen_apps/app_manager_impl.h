// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOCK_SCREEN_APPS_APP_MANAGER_IMPL_H_
#define CHROME_BROWSER_CHROMEOS_LOCK_SCREEN_APPS_APP_MANAGER_IMPL_H_

#include "base/callback.h"
#include "base/macros.h"
#include "chrome/browser/chromeos/lock_screen_apps/app_manager.h"

class Profile;

namespace lock_screen_apps {

// The default implementation of lock_screen_apps::AppManager.
class AppManagerImpl : public AppManager {
 public:
  AppManagerImpl();
  ~AppManagerImpl() override;

  // AppManager implementation:
  void Initialize(Profile* primary_profile,
                  Profile* lock_screen_profile) override;
  void Start(const base::Closure& note_taking_changed_callback) override;
  void Stop() override;
  bool LaunchNoteTaking() override;
  bool IsNoteTakingAppAvailable() const override;
  std::string GetNoteTakingAppId() const override;

 private:
  Profile* primary_profile_ = nullptr;
  Profile* lock_screen_profile_ = nullptr;

  base::Closure note_taking_changed_callback_;

  DISALLOW_COPY_AND_ASSIGN(AppManagerImpl);
};

}  // namespace lock_screen_apps

#endif  // CHROME_BROWSER_CHROMEOS_LOCK_SCREEN_APPS_APP_MANAGER_IMPL_H_
