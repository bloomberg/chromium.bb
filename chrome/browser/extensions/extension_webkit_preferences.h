// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_WEBKIT_PREFERENCES_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_WEBKIT_PREFERENCES_H_

#include "chrome/common/chrome_view_type.h"

class Extension;

namespace webkit_glue {
struct WebPreferences;
}

namespace extension_webkit_preferences {

void SetPreferences(const Extension* extension,
                    content::ViewType render_view_type,
                    webkit_glue::WebPreferences* webkit_prefs);

}  // namespace extension_webkit_preferences

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_WEBKIT_PREFERENCES_H_
