// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_PLUGINS_SHADOW_DOM_PLUGIN_PLACEHOLDER_H_
#define CHROME_RENDERER_PLUGINS_SHADOW_DOM_PLUGIN_PLACEHOLDER_H_

#include "base/memory/scoped_ptr.h"
#include "third_party/WebKit/public/web/WebPluginPlaceholder.h"

namespace blink {
class WebLocalFrame;
struct WebPluginParams;
}

namespace content {
class RenderFrame;
}

// This is the Chrome implementation of shadow DOM plugin placeholders,
// intended to ultimately replace those based on WebViewPlugin.
// It is guarded by the --enable-plugin-placeholder-shadow-dom switch.

// Returns |true| if shadow DOM plugin placeholders are enabled.
bool ShadowDOMPluginPlaceholderEnabled();

// Possibly creates a placeholder given plugin info output.
// Returns nullptr if it is not appropriate to create a placeholder
// (for instance, because the plugin should be allowed to load).
scoped_ptr<blink::WebPluginPlaceholder> CreateShadowDOMPlaceholderForPluginInfo(
    content::RenderFrame* render_frame,
    blink::WebLocalFrame* frame,
    const blink::WebPluginParams& orig_params);

// Creates a placeholder suitable for representing a missing plugin.
scoped_ptr<blink::WebPluginPlaceholder>
CreateShadowDOMPlaceholderForMissingPlugin();

#endif  // CHROME_RENDERER_PLUGINS_SHADOW_DOM_PLUGIN_PLACEHOLDER_H_
