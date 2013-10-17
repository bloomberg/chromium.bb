// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_UTIL_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_UTIL_H_

#include <string>

namespace extensions {
class Extension;
}

class ExtensionService;

namespace extension_util {

// Whether this extension can run in an incognito window.
bool IsIncognitoEnabled(const std::string& extension_id,
                        const ExtensionService* service);

// Will reload the extension since this permission is applied at loading time
// only.
void SetIsIncognitoEnabled(const std::string& extension_id,
                           ExtensionService* service,
                           bool enabled);

// Returns true if the given extension can see events and data from another
// sub-profile (incognito to original profile, or vice versa).
bool CanCrossIncognito(const extensions::Extension* extension,
                       const ExtensionService* service);

// Returns true if the given extension can be loaded in incognito.
bool CanLoadInIncognito(const extensions::Extension* extension,
                        const ExtensionService* service);

// Whether this extension can inject scripts into pages with file URLs.
bool AllowFileAccess(const extensions::Extension* extension,
                     const ExtensionService* service);

// Will reload the extension since this permission is applied at loading time
// only.
void SetAllowFileAccess(const extensions::Extension* extension,
                        ExtensionService* service,
                        bool allow);

}  // namespace extension_util

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_UTIL_H_
