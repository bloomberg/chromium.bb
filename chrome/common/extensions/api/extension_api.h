// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_EXTENSIONS_API_EXTENSION_API_H_
#define CHROME_COMMON_EXTENSIONS_API_EXTENSION_API_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/singleton.h"

namespace base {
class DictionaryValue;
class ListValue;
}

namespace extensions {

// C++ Wrapper for the extension_api.json file.
class ExtensionAPI {
 public:
  // Returns the single instance of this class.
  static ExtensionAPI* GetInstance();

  const base::ListValue* value() const {
    return value_.get();
  }

  // Returns ture if |name| is a privileged API. Privileged APIs can only be
  // called from extension code which is running in its own designated extension
  // process. They cannot be called from extension code running in content
  // scripts, or other low-privileged processes.
  bool IsPrivileged(const std::string& name) const;

 private:
  friend struct DefaultSingletonTraits<ExtensionAPI>;

  ExtensionAPI();
  ~ExtensionAPI();

  // Find an item in |list| with the specified property name and value, or NULL
  // if no such item exists.
  base::DictionaryValue* FindListItem(base::ListValue* list,
                                      const std::string& property_name,
                                      const std::string& property_value) const;

  // Returns true if the function or event under |namespace_node| with
  // the specified |child_name| is privileged, or false otherwise. If the name
  // is not found, defaults to privileged.
  bool IsChildNamePrivileged(base::DictionaryValue* namespace_node,
                             const std::string& child_kind,
                             const std::string& child_name) const;

  static ExtensionAPI* instance_;

  scoped_ptr<base::ListValue> value_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionAPI);
};

}  // extensions

#endif  // CHROME_COMMON_EXTENSIONS_API_EXTENSION_API_H_
