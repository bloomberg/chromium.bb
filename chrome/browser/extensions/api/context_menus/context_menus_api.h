// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_CONTEXT_MENUS_CONTEXT_MENUS_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_CONTEXT_MENUS_CONTEXT_MENUS_API_H_

#include "chrome/browser/extensions/extension_function.h"

namespace extensions {

class ContextMenusCreateFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("contextMenus.create")

 protected:
  virtual ~ContextMenusCreateFunction() {}

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

class ContextMenusUpdateFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("contextMenus.update")

 protected:
  virtual ~ContextMenusUpdateFunction() {}

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

class ContextMenusRemoveFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("contextMenus.remove")

 protected:
  virtual ~ContextMenusRemoveFunction() {}

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

class ContextMenusRemoveAllFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("contextMenus.removeAll")

 protected:
  virtual ~ContextMenusRemoveAllFunction() {}

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_CONTEXT_MENUS_CONTEXT_MENUS_API_H_
