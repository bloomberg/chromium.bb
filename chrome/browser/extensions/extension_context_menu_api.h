// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_CONTEXT_MENU_API_H__
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_CONTEXT_MENU_API_H__

#include "chrome/browser/extensions/extension_function.h"

class CreateContextMenuFunction : public SyncExtensionFunction {
  ~CreateContextMenuFunction() {}
  virtual bool RunImpl();
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.contextMenu.create")
};

class RemoveContextMenuFunction : public SyncExtensionFunction {
  ~RemoveContextMenuFunction() {}
  virtual bool RunImpl();
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.contextMenu.remove")
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_CONTEXT_MENU_API_H__
