// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_CONTEXT_MENU_API_H__
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_CONTEXT_MENU_API_H__
#pragma once

#include "chrome/browser/extensions/extension_function.h"
#include "chrome/browser/extensions/extension_menu_manager.h"
#include "chrome/common/extensions/extension_extent.h"

class DictionaryValue;
class ExtensionMenuItem;

class ExtensionContextMenuFunction : public SyncExtensionFunction {
 public:
  ~ExtensionContextMenuFunction() {}

 protected:
  // Helper function to read and parse a list of menu item contexts.
  bool ParseContexts(const DictionaryValue& properties,
                     const char* key,
                     ExtensionMenuItem::ContextList* result);

  // Looks in properties for the "type" key, and reads the value in |result|. On
  // error, returns false and puts an error message into error_. If the key is
  // not present, |result| is set to |default_value| and the return value is
  // true.
  bool ParseType(const DictionaryValue& properties,
                 const ExtensionMenuItem::Type& default_value,
                 ExtensionMenuItem::Type* result);

  // Helper to read and parse the "checked" property.
  bool ParseChecked(ExtensionMenuItem::Type type,
                    const DictionaryValue& properties,
                    bool default_value,
                    bool* checked);

  // Helper to read in a set of url patterns from a property with the given key
  // name.
  bool ParseURLPatterns(const DictionaryValue& properties,
                        const char* key,
                        ExtensionExtent* result);

  // Reads in any document and targetUrl patterns from |properties| and sets
  // them on |item|.
  bool SetURLPatterns(const DictionaryValue& properties,
                      ExtensionMenuItem* item);

  // If the parentId key was specified in properties, this will try looking up
  // an ExtensionMenuItem with that id and set it into |result|. Returns false
  // on error, with an explanation written into error_. Note that if the
  // parentId key is not in properties, this will return true and leave |result|
  // unset. Also, it is considered an error if the item found has a type other
  // than NORMAL.
  bool GetParent(const DictionaryValue& properties,
                 const ExtensionMenuManager& manager,
                 ExtensionMenuItem** result);
};

class CreateContextMenuFunction : public ExtensionContextMenuFunction {
  ~CreateContextMenuFunction() {}
  virtual bool RunImpl();
  DECLARE_EXTENSION_FUNCTION_NAME("contextMenus.create")
};

class UpdateContextMenuFunction : public ExtensionContextMenuFunction {
  ~UpdateContextMenuFunction() {}
  virtual bool RunImpl();
  DECLARE_EXTENSION_FUNCTION_NAME("contextMenus.update")
};

class RemoveContextMenuFunction : public ExtensionContextMenuFunction {
  ~RemoveContextMenuFunction() {}
  virtual bool RunImpl();
  DECLARE_EXTENSION_FUNCTION_NAME("contextMenus.remove")
};

class RemoveAllContextMenusFunction : public ExtensionContextMenuFunction {
  ~RemoveAllContextMenusFunction() {}
  virtual bool RunImpl();
  DECLARE_EXTENSION_FUNCTION_NAME("contextMenus.removeAll")
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_CONTEXT_MENU_API_H__
