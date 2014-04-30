// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_CONTEXT_MENUS_CONTEXT_MENUS_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_CONTEXT_MENUS_CONTEXT_MENUS_API_H_

#include "chrome/browser/extensions/chrome_extension_function.h"

namespace extensions {

class ContextMenusCreateFunction : public ChromeSyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("contextMenus.create", CONTEXTMENUS_CREATE)

 protected:
  virtual ~ContextMenusCreateFunction() {}

  // ExtensionFunction:
  virtual bool RunSync() OVERRIDE;
};

class ContextMenusUpdateFunction : public ChromeSyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("contextMenus.update", CONTEXTMENUS_UPDATE)

 protected:
  virtual ~ContextMenusUpdateFunction() {}

  // ExtensionFunction:
  virtual bool RunSync() OVERRIDE;
};

class ContextMenusRemoveFunction : public ChromeSyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("contextMenus.remove", CONTEXTMENUS_REMOVE)

 protected:
  virtual ~ContextMenusRemoveFunction() {}

  // ExtensionFunction:
  virtual bool RunSync() OVERRIDE;
};

class ContextMenusRemoveAllFunction : public ChromeSyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("contextMenus.removeAll", CONTEXTMENUS_REMOVEALL)

 protected:
  virtual ~ContextMenusRemoveAllFunction() {}

  // ExtensionFunction:
  virtual bool RunSync() OVERRIDE;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_CONTEXT_MENUS_CONTEXT_MENUS_API_H_
