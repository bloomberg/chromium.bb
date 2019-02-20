// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_BOOKMARK_APP_EXTENSION_UTIL_H_
#define CHROME_BROWSER_EXTENSIONS_BOOKMARK_APP_EXTENSION_UTIL_H_

#include "extensions/common/constants.h"

class Profile;

namespace content {
class WebContents;
}

namespace extensions {

class Extension;

void BookmarkAppCreateOsShortcuts(Profile* profile, const Extension* extension);

void BookmarkAppReparentTab(Profile* profile,
                            content::WebContents* contents,
                            const Extension* extension,
                            LaunchType launch_type);

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_BOOKMARK_APP_EXTENSION_UTIL_H_
