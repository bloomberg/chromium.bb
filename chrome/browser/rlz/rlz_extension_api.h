// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RLZ_RLZ_EXTENSION_API_H_
#define CHROME_BROWSER_RLZ_RLZ_EXTENSION_API_H_
#pragma once

#include "build/build_config.h"

#if defined(OS_WIN)

#include "chrome/browser/extensions/extension_function.h"

class RlzRecordProductEventFunction : public SyncExtensionFunction {
  virtual bool RunImpl();
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.rlz.recordProductEvent")
};

class RlzGetAccessPointRlzFunction : public SyncExtensionFunction {
  virtual bool RunImpl();
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.rlz.getAccessPointRlz")
};

class RlzSendFinancialPingFunction : public SyncExtensionFunction {
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.rlz.sendFinancialPing")
 // Making this function protected so that it can be overridden in tests.
 protected:
  virtual bool RunImpl();
};

class RlzClearProductStateFunction : public SyncExtensionFunction {
  virtual bool RunImpl();
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.rlz.clearProductState")
};

#endif  // defined(OS_WIN)

#endif  // CHROME_BROWSER_RLZ_RLZ_EXTENSION_API_H_
