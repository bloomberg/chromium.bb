// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_APP_API_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_APP_API_H_
#pragma once

#include "chrome/browser/extensions/extension_function.h"

class AppNotifyFunction : public SyncExtensionFunction {
  virtual ~AppNotifyFunction() {}
  virtual bool RunImpl() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.app.notify");
};

class AppClearAllNotificationsFunction : public SyncExtensionFunction {
  virtual ~AppClearAllNotificationsFunction() {}
  virtual bool RunImpl() OVERRIDE;
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.app.clearAllNotifications");
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_APP_API_H_
