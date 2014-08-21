// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ATHENA_CONTENT_PUBLIC_APP_CONTENT_CONTROL_DLEGATE_H_
#define ATHENA_CONTENT_PUBLIC_APP_CONTENT_CONTROL_DLEGATE_H_

#include <string>

#include "base/macros.h"

namespace content {
class BrowserContext;
class WebContents;
}

namespace athena {

// The application content delegate which can be overwritten for unit tests to
// eliminate dependencies to the content / browser system.
class AppContentControlDelegate {
 public:
  static AppContentControlDelegate* CreateAppContentControlDelegate();

  AppContentControlDelegate() {}
  virtual ~AppContentControlDelegate() {}

  // Unload an application. Returns true when unloaded.
  virtual bool UnloadApplication(const std::string& app_id,
                                 content::BrowserContext* browser_context) = 0;
  // Restarts an application. Returns true when the restart was initiated.
  virtual bool RestartApplication(const std::string& app_id,
                                  content::BrowserContext* browser_context) = 0;
  // Returns the application ID (or an empty string) for a given web content.
  virtual std::string GetApplicationID(content::WebContents* web_contents) = 0;
};

}  // namespace athena

#endif  // ATHENA_CONTENT_PUBLIC_APP_CONTENT_CONTROL_DLEGATE_H_
