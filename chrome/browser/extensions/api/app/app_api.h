// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_APP_APP_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_APP_APP_API_H_

#include "chrome/browser/extensions/extension_function.h"

namespace extensions {

class AppNotifyFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("experimental.app.notify", EXPERIMENTAL_APP_NOTIFY)

 protected:
  virtual ~AppNotifyFunction() {}
  virtual bool RunImpl() OVERRIDE;
};

class AppClearAllNotificationsFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("experimental.app.clearAllNotifications",
                             EXPERIMENTAL_APP_CLEARALLNOTIFICATIONS)

 protected:
  virtual ~AppClearAllNotificationsFunction() {}
  virtual bool RunImpl() OVERRIDE;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_APP_APP_API_H_
