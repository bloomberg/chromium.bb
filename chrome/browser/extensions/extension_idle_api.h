// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_IDLE_API_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_IDLE_API_H_
#pragma once

#include "chrome/browser/idle.h"
#include "chrome/browser/extensions/extension_function.h"

class Profile;

// Event router class for events related to the idle API.
class ExtensionIdleEventRouter {
 public:
  static void OnIdleStateChange(Profile* profile,
                                IdleState idleState);
 private:
  DISALLOW_COPY_AND_ASSIGN(ExtensionIdleEventRouter);
};

// Implementation of the chrome.idle.queryState API.
class ExtensionIdleQueryStateFunction : public SyncExtensionFunction {
 public:
  virtual bool RunImpl();
  DECLARE_EXTENSION_FUNCTION_NAME("idle.queryState")
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_IDLE_API_H_
