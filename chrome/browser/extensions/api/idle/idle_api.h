// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_IDLE_IDLE_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_IDLE_IDLE_API_H_

#include "chrome/browser/extensions/extension_function.h"
#include "chrome/browser/idle.h"

namespace extensions {

// Implementation of the chrome.idle.queryState API.
class IdleQueryStateFunction : public AsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("idle.queryState", IDLE_QUERYSTATE)

 protected:
  virtual ~IdleQueryStateFunction() {}

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;

 private:
  void IdleStateCallback(IdleState state);
};

// Implementation of the chrome.idle.setDetectionInterval API.
class IdleSetDetectionIntervalFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("idle.setDetectionInterval",
                             IDLE_SETDETECTIONINTERVAL)

 protected:
  virtual ~IdleSetDetectionIntervalFunction() {}

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_IDLE_IDLE_API_H_
