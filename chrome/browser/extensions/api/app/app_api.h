// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_APP_APP_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_APP_APP_API_H_
#pragma once

#include "chrome/browser/extensions/extension_function.h"

class Profile;

namespace extensions {

class Extension;

class AppNotifyFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.app.notify");

 protected:
  virtual ~AppNotifyFunction() {}
  virtual bool RunImpl() OVERRIDE;
};

class AppClearAllNotificationsFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.app.clearAllNotifications");

 protected:
  virtual ~AppClearAllNotificationsFunction() {}
  virtual bool RunImpl() OVERRIDE;
};

class AppEventRouter {
 public:
  // Dispatches the onLaunched event to the given app.
  static void DispatchOnLaunchedEvent(Profile* profile,
                                      const Extension* extension);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_APP_APP_API_H_
