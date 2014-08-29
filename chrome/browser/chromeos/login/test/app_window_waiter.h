// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_TEST_APP_WINDOW_WAITER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_TEST_APP_WINDOW_WAITER_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/run_loop.h"
#include "extensions/browser/app_window/app_window_registry.h"

namespace extensions {
class AppWindow;
}

namespace chromeos {

// Helper class that monitors app windows to wait for a window to appear.
// Use a new instance for each use, one instance will only work for one Wait.
class AppWindowWaiter : public extensions::AppWindowRegistry::Observer {
 public:
  AppWindowWaiter(extensions::AppWindowRegistry* registry,
                  const std::string& app_id);
  virtual ~AppWindowWaiter();

  extensions::AppWindow* Wait();

  // AppWindowRegistry::Observer:
  virtual void OnAppWindowAdded(extensions::AppWindow* app_window) OVERRIDE;

 private:
  extensions::AppWindowRegistry* registry_;
  std::string app_id_;
  base::RunLoop run_loop_;
  extensions::AppWindow* window_;

  DISALLOW_COPY_AND_ASSIGN(AppWindowWaiter);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_TEST_APP_WINDOW_WAITER_H_
