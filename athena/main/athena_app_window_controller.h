// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ATHENA_MAIN_ATHENA_APP_WINDOW_CONTROLLER_H_
#define ATHENA_MAIN_ATHENA_APP_WINDOW_CONTROLLER_H_

#include "base/macros.h"
#include "extensions/shell/browser/shell_app_window_controller.h"

namespace athena {

// The shell app window controller for athena. It embeds the web_contents of
// an app window into an Athena activity.
class AthenaAppWindowController : public extensions::ShellAppWindowController {
 public:
  AthenaAppWindowController();
  virtual ~AthenaAppWindowController();

  // Overridden from extensions::ShellAppWindowController:
  virtual extensions::ShellAppWindow* CreateAppWindow(
      content::BrowserContext* context,
      const extensions::Extension* extension) OVERRIDE;
  virtual void CloseAppWindows() OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(AthenaAppWindowController);
};

}  // namespace athena

#endif  // ATHENA_MAIN_ATHENA_APP_WINDOW_CONTROLLER_H_
