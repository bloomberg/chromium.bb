// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_webkit_preferences.h"

#include "base/command_line.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/manifest.h"
#include "webkit/glue/webpreferences.h"

namespace extension_webkit_preferences {

void SetPreferences(const extensions::Extension* extension,
                    chrome::ViewType render_view_type,
                    webkit_glue::WebPreferences* webkit_prefs) {
  if (!extension)
    return;

  if (!extension->is_hosted_app()) {
    // Extensions are trusted so we override any user preferences for disabling
    // javascript or images.
    webkit_prefs->loads_images_automatically = true;
    webkit_prefs->javascript_enabled = true;

    // Tabs aren't typically allowed to close windows. But extensions shouldn't
    // be subject to that.
    webkit_prefs->allow_scripts_to_close_windows = true;

    // Disable gpu acceleration for extension background pages to avoid
    // unecessarily creating a compositor context for them.
    if (render_view_type == chrome::VIEW_TYPE_EXTENSION_BACKGROUND_PAGE) {
      webkit_prefs->accelerated_compositing_enabled = false;
      webkit_prefs->force_compositing_mode = false;
    }
  }

  if (extension->is_platform_app()) {
    webkit_prefs->databases_enabled = false;
    webkit_prefs->local_storage_enabled = false;
    webkit_prefs->sync_xhr_in_documents_enabled = false;
    webkit_prefs->cookie_enabled = false;
  }

  // Enable WebGL features that regular pages can't access, since they add
  // more risk of fingerprinting.
  webkit_prefs->privileged_webgl_extensions_enabled = true;

  // If this is a component extension, then apply the same poliy for
  // accelerated compositing as for chrome: URLs (from
  // WebContents::GetWebkitPrefs).  This is important for component extensions
  // like the file manager which are sometimes loaded using chrome: URLs and
  // sometimes loaded with chrome-extension: URLs - we should expect the
  // performance characteristics to be similar in both cases.
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  if (extension->location() == extensions::Manifest::COMPONENT &&
      !command_line.HasSwitch(switches::kAllowWebUICompositing)) {
    webkit_prefs->accelerated_compositing_enabled = false;
    webkit_prefs->accelerated_2d_canvas_enabled = false;
  }
}

}  // extension_webkit_preferences
