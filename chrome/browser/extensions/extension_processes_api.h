// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_PROCESSES_API_H__
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_PROCESSES_API_H__
#pragma once

#include "chrome/browser/extensions/extension_function.h"

// This extension function returns the Process object for the renderer process
// currently in use by the specified Tab.
class GetProcessForTabFunction : public SyncExtensionFunction {
  virtual ~GetProcessForTabFunction() {}
  virtual bool RunImpl();
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.processes.getProcessForTab")
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_PROCESSES_API_H__
