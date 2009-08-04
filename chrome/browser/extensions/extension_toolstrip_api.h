// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_TOOLSTRIP_API_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_TOOLSTRIP_API_H_

#include "chrome/browser/extensions/extension_function.h"
#include "chrome/browser/extensions/extension_shelf_model.h"

// Function names.
namespace extension_toolstrip_api_functions {
  extern const char kExpandFunction[];
  extern const char kCollapseFunction[];
};  // namespace extension_toolstrip_api_functions

class ToolstripFunction : public SyncExtensionFunction {
 protected:
  virtual bool RunImpl();

  ExtensionShelfModel* model_;
  ExtensionShelfModel::iterator toolstrip_;
};

class ToolstripExpandFunction : public ToolstripFunction {
  virtual bool RunImpl();
};

class ToolstripCollapseFunction : public ToolstripFunction {
  virtual bool RunImpl();
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_TOOLSTRIP_API_H_
