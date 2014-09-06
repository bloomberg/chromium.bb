// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/browser_plugin/mock_browser_plugin.h"

#include "content/public/renderer/browser_plugin_delegate.h"
#include "content/renderer/render_process_impl.h"

namespace content {

MockBrowserPlugin::MockBrowserPlugin(RenderViewImpl* render_view,
                                     blink::WebFrame* frame,
                                     scoped_ptr<BrowserPluginDelegate> delegate)
    : BrowserPlugin(render_view, frame, delegate.Pass()) {
}

MockBrowserPlugin::~MockBrowserPlugin() {}

}  // namespace content
