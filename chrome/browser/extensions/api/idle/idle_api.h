// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_IDLE_IDLE_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_IDLE_IDLE_API_H_

#include "chrome/browser/idle.h"
#include "extensions/browser/extension_function.h"

namespace extensions {

// Implementation of the chrome.idle.queryState API.
class IdleQueryStateFunction : public AsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("idle.queryState", IDLE_QUERYSTATE)

 protected:
  ~IdleQueryStateFunction() override {}

  // ExtensionFunction:
  bool RunAsync() override;

 private:
  void IdleStateCallback(IdleState state);
};

// Implementation of the chrome.idle.setDetectionInterval API.
class IdleSetDetectionIntervalFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("idle.setDetectionInterval",
                             IDLE_SETDETECTIONINTERVAL)

 protected:
  ~IdleSetDetectionIntervalFunction() override {}

  // ExtensionFunction:
  bool RunSync() override;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_IDLE_IDLE_API_H_
