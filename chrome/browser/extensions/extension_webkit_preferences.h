// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_WEBKIT_PREFERENCES_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_WEBKIT_PREFERENCES_H_

#include "chrome/common/chrome_view_types.h"

class Extension;
struct WebPreferences;

namespace extension_webkit_preferences {

void SetPreferences(const Extension* extension,
                    content::ViewType render_view_type,
                    WebPreferences* webkit_prefs);

}  // namespace extension_webkit_preferences

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_WEBKIT_PREFERENCES_H_
