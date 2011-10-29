// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/webpreferences.h"

#include "chrome/common/extensions/extension.h"

namespace extension_webkit_preferences {

void SetPreferences(WebPreferences* webkit_prefs, const Extension* extension) {
  if (extension && !extension->is_hosted_app()) {
    // Extensions are trusted so we override any user preferences for disabling
    // javascript or images.
    webkit_prefs->loads_images_automatically = true;
    webkit_prefs->javascript_enabled = true;

    // Tabs aren't typically allowed to close windows. But extensions shouldn't
    // be subject to that.
    webkit_prefs->allow_scripts_to_close_windows = true;

    // Enable privileged WebGL extensions.
    webkit_prefs->privileged_webgl_extensions_enabled = true;
  }
}

}  // extension_webkit_preferences
