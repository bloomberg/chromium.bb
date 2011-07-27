// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Constants used for the Tabs API and the Windows API.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_PERMISSIONS_API_CONSTANTS_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_PERMISSIONS_API_CONSTANTS_H_
#pragma once

namespace extension_permissions_module_constants {

// Keys used in serializing permissions data and events.
extern const char kApisKey[];

// Error messages.
extern const char kCantRemoveRequiredPermissionsError[];
extern const char kNotInOptionalPermissionsError[];
extern const char kNotWhitelistedError[];
extern const char kUnknownPermissionError[];

// Event names.
extern const char kOnAdded[];
extern const char kOnRemoved[];

};  // namespace extension_permissions_module_constants

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_PERMISSIONS_API_CONSTANTS_H_
