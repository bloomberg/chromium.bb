// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_MODULE_H__
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_MODULE_H__
#pragma once

#include "chrome/browser/extensions/extension_function.h"

class ExtensionPrefs;

class SetUpdateUrlDataFunction : public SyncExtensionFunction {
 protected:
  ExtensionPrefs* extension_prefs();
  virtual bool RunImpl();
  DECLARE_EXTENSION_FUNCTION_NAME("extension.setUpdateUrlData");
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_MODULE_H__
