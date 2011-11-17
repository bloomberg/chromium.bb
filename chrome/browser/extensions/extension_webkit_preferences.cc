// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_webkit_preferences.h"

#include "chrome/common/extensions/extension.h"
#include "webkit/glue/webpreferences.h"

namespace extension_webkit_preferences {

void SetPreferences(const Extension* extension,
                    content::ViewType render_view_type,
                    WebPreferences* webkit_prefs) {
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

    // Disable anything that requires the GPU process for background pages.
    // See http://crbug.com/64512 and http://crbug.com/64841.
    if (render_view_type == chrome::VIEW_TYPE_EXTENSION_BACKGROUND_PAGE) {
      webkit_prefs->experimental_webgl_enabled = false;
      webkit_prefs->accelerated_compositing_enabled = false;
      webkit_prefs->accelerated_2d_canvas_enabled = false;
    }
  }
}

}  // extension_webkit_preferences
