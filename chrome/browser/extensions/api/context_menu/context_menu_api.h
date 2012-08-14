// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_CONTEXT_MENU_CONTEXT_MENU_API_H__
#define CHROME_BROWSER_EXTENSIONS_API_CONTEXT_MENU_CONTEXT_MENU_API_H__

#include "chrome/browser/extensions/extension_function.h"

namespace extensions {

class CreateContextMenuFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("contextMenus.create")

 protected:
  virtual ~CreateContextMenuFunction() {}

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

class UpdateContextMenuFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("contextMenus.update")

 protected:
  virtual ~UpdateContextMenuFunction() {}

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

class RemoveContextMenuFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("contextMenus.remove")

 protected:
  virtual ~RemoveContextMenuFunction() {}

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

class RemoveAllContextMenusFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("contextMenus.removeAll")

 protected:
  virtual ~RemoveAllContextMenusFunction() {}

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_CONTEXT_MENU_CONTEXT_MENU_API_H__
