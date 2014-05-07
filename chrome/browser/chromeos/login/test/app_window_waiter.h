// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_TEST_APP_WINDOW_WAITER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_TEST_APP_WINDOW_WAITER_H_

#include <string>

#include "apps/app_window_registry.h"
#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/run_loop.h"

namespace apps {
class AppWindow;
}

namespace chromeos {

// Helper class that monitors app windows to wait for a window to appear.
// Use a new instance for each use, one instance will only work for one Wait.
class AppWindowWaiter : public apps::AppWindowRegistry::Observer {
 public:
  AppWindowWaiter(apps::AppWindowRegistry* registry,
                  const std::string& app_id);
  virtual ~AppWindowWaiter();

  apps::AppWindow* Wait();

  // AppWindowRegistry::Observer:
  virtual void OnAppWindowAdded(apps::AppWindow* app_window) OVERRIDE;

 private:
  apps::AppWindowRegistry* registry_;
  std::string app_id_;
  base::RunLoop run_loop_;
  apps::AppWindow* window_;

  DISALLOW_COPY_AND_ASSIGN(AppWindowWaiter);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_TEST_APP_WINDOW_WAITER_H_
