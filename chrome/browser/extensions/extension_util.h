// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_UTIL_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_UTIL_H_

#include <string>

#include "base/memory/scoped_ptr.h"
#include "url/gurl.h"

namespace base {
class DictionaryValue;
}

namespace content {
class BrowserContext;
}

namespace extensions {

class Extension;
struct ExtensionInfo;

namespace util {

// Returns true if |extension_id| can run in an incognito window.
bool IsIncognitoEnabled(const std::string& extension_id,
                        content::BrowserContext* context);

// Sets whether |extension_id| can run in an incognito window. Reloads the
// extension if it's enabled since this permission is applied at loading time
// only. Note that an ExtensionService must exist.
void SetIsIncognitoEnabled(const std::string& extension_id,
                           content::BrowserContext* context,
                           bool enabled);

// Returns true if |extension| can see events and data from another sub-profile
// (incognito to original profile, or vice versa).
bool CanCrossIncognito(const extensions::Extension* extension,
                       content::BrowserContext* context);

// Returns true if |extension| can be loaded in incognito.
bool CanLoadInIncognito(const extensions::Extension* extension,
                        content::BrowserContext* context);

// Returns true if this extension can inject scripts into pages with file URLs.
bool AllowFileAccess(const std::string& extension_id,
                     content::BrowserContext* context);

// Sets whether |extension_id| can inject scripts into pages with file URLs.
// Reloads the extension if it's enabled since this permission is applied at
// loading time only. Note than an ExtensionService must exist.
void SetAllowFileAccess(const std::string& extension_id,
                        content::BrowserContext* context,
                        bool allow);

// Returns true if |extension_id| can be launched (possibly only after being
// enabled).
bool IsAppLaunchable(const std::string& extension_id,
                     content::BrowserContext* context);

// Returns true if |extension_id| can be launched without being enabled first.
bool IsAppLaunchableWithoutEnabling(const std::string& extension_id,
                                    content::BrowserContext* context);

// Returns true if |extension_id| is idle and it is safe to perform actions such
// as updating.
bool IsExtensionIdle(const std::string& extension_id,
                     content::BrowserContext* context);

// Returns true if |extension_id| is installed permanently and not ephemerally.
bool IsExtensionInstalledPermanently(const std::string& extension_id,
                                     content::BrowserContext* context);

// Returns the site of the |extension_id|, given the associated |context|.
// Suitable for use with BrowserContext::GetStoragePartitionForSite().
GURL GetSiteForExtensionId(const std::string& extension_id,
                           content::BrowserContext* context);

// Sets the name, id, and icon resource path of the given extension into the
// returned dictionary.
scoped_ptr<base::DictionaryValue> GetExtensionInfo(const Extension* extension);

// Returns true if the extension has isolated storage.
bool HasIsolatedStorage(const ExtensionInfo& info);

// Returns true if the site URL corresponds to an extension or app and has
// isolated storage.
bool SiteHasIsolatedStorage(const GURL& extension_site_url,
                            content::BrowserContext* context);

}  // namespace util
}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_UTIL_H_
