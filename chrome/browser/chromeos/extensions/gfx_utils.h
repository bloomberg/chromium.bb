// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_GFX_UTILS_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_GFX_UTILS_H_

#include <string>

namespace content {
class BrowserContext;
}

namespace gfx {
class ImageSkia;
}

namespace extensions {
namespace util {

// May apply additional badge in order to distinguish dual apps from Chrome and
// Android side.
void MaybeApplyChromeBadge(content::BrowserContext* context,
                           const std::string& extension_id,
                           gfx::ImageSkia* icon_out);

}  // namespace util
}  // namespace extensions

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_GFX_UTILS_H_
