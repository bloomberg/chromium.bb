// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APPS_SHELL_BROWSER_SHELL_APP_RUNTIME_API_H_
#define APPS_SHELL_BROWSER_SHELL_APP_RUNTIME_API_H_

#include "base/macros.h"

namespace extensions {

class EventRouter;
class Extension;

// A simplified implementation of the chrome.app.runtime API.
class ShellAppRuntimeAPI {
 public:
  // Dispatches the onLaunched event.
  static void DispatchOnLaunchedEvent(EventRouter* event_router,
                                      const Extension* extension);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(ShellAppRuntimeAPI);
};

}  // namespace extensions

#endif  // APPS_SHELL_BROWSER_SHELL_APP_RUNTIME_API_H_
