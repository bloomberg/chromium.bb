// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_GFX_UTILS_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_GFX_UTILS_H_

#include <string>
#include <vector>

namespace content {
class BrowserContext;
}

namespace gfx {
class ImageSkia;
}

namespace extensions {
namespace util {

// Returns the equivalent Playstore app package name that has been installed to
// the Chrome extension (identified by ||extension_id|). Returns an empty string
// if there is no such Playstore app.
std::string GetEquivalentInstalledArcApp(content::BrowserContext* context,
                                         const std::string& extension_id);

// Returns the equivalent Chrome extensions that have been installed to the
// Playstore app (identified by |arc_package_name|). Returns an empty vector if
// there is no such Chrome extension.
const std::vector<std::string> GetEquivalentInstalledExtensions(
    content::BrowserContext* context,
    const std::string& arc_package_name);

// May apply additional badge in order to distinguish dual apps from Chrome and
// Android side.
void MaybeApplyChromeBadge(content::BrowserContext* context,
                           const std::string& extension_id,
                           gfx::ImageSkia* icon_out);

}  // namespace util
}  // namespace extensions

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_GFX_UTILS_H_
