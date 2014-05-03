// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_IDLE_IDLE_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_IDLE_IDLE_API_H_

#include "chrome/browser/extensions/chrome_extension_function.h"
#include "chrome/browser/idle.h"

namespace extensions {

// Implementation of the chrome.idle.queryState API.
class IdleQueryStateFunction : public ChromeAsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("idle.queryState", IDLE_QUERYSTATE)

 protected:
  virtual ~IdleQueryStateFunction() {}

  // ExtensionFunction:
  virtual bool RunAsync() OVERRIDE;

 private:
  void IdleStateCallback(IdleState state);
};

// Implementation of the chrome.idle.setDetectionInterval API.
class IdleSetDetectionIntervalFunction : public ChromeSyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("idle.setDetectionInterval",
                             IDLE_SETDETECTIONINTERVAL)

 protected:
  virtual ~IdleSetDetectionIntervalFunction() {}

  // ExtensionFunction:
  virtual bool RunSync() OVERRIDE;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_IDLE_IDLE_API_H_
