// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/shell_content_renderer_client.h"

#include "base/command_line.h"
#include "content/public/common/content_constants.h"
#include "content/public/common/content_switches.h"
#include "content/shell/shell_render_process_observer.h"
#include "content/shell/shell_switches.h"
#include "content/shell/webkit_test_runner.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebPluginParams.h"
#include "v8/include/v8.h"

namespace content {

ShellContentRendererClient::ShellContentRendererClient() {
}

ShellContentRendererClient::~ShellContentRendererClient() {
}

void ShellContentRendererClient::RenderThreadStarted() {
  shell_observer_.reset(new ShellRenderProcessObserver());
}

void ShellContentRendererClient::RenderViewCreated(RenderView* render_view) {
  if (!CommandLine::ForCurrentProcess()->HasSwitch(switches::kDumpRenderTree))
    return;

  new WebKitTestRunner(render_view);
}

bool ShellContentRendererClient::OverrideCreatePlugin(
    RenderView* render_view,
    WebKit::WebFrame* frame,
    const WebKit::WebPluginParams& params,
    WebKit::WebPlugin** plugin) {
  std::string mime_type = params.mimeType.utf8();
  if (mime_type == content::kBrowserPluginMimeType) {
    // Allow browser plugin in content_shell only if it is forced by flag.
    // Returning true here disables the plugin.
    return !CommandLine::ForCurrentProcess()->HasSwitch(
        switches::kEnableBrowserPluginForAllViewTypes);
  }
  return false;
}

}  // namespace content
