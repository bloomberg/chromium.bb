// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_PERMISSIONS_API_HELPERS_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_PERMISSIONS_API_HELPERS_H_
#pragma once

#include <string>

#include "base/memory/ref_counted.h"

namespace base {
class DictionaryValue;
}
class ExtensionPermissionSet;

namespace extensions {

namespace permissions_api {

// Converts the permission |set| to a dictionary value.
base::DictionaryValue* PackPermissionsToValue(
    const ExtensionPermissionSet* set);

// Converts the |value| to a permission set.
bool UnpackPermissionsFromValue(base::DictionaryValue* value,
                                scoped_refptr<ExtensionPermissionSet>* ptr,
                                bool* bad_message,
                                std::string* error);

}  // namespace permissions_api

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_PERMISSIONS_API_HELPERS_H__
