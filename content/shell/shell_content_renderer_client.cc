// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/shell_content_renderer_client.h"

#include "base/callback.h"
#include "base/command_line.h"
#include "content/public/common/content_constants.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/layouttest_support.h"
#include "content/shell/shell_render_process_observer.h"
#include "content/shell/shell_switches.h"
#include "content/shell/webkit_test_runner.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebPluginParams.h"
#include "third_party/WebKit/Tools/DumpRenderTree/chromium/TestRunner/public/WebTestProxy.h"
#include "v8/include/v8.h"

using WebTestRunner::WebTestProxyBase;

namespace content {

ShellContentRendererClient::ShellContentRendererClient() {
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kDumpRenderTree)) {
    EnableWebTestProxyCreation(
        base::Bind(&ShellContentRendererClient::WebTestProxyCreated,
                   base::Unretained(this)));
  }
}

ShellContentRendererClient::~ShellContentRendererClient() {
}

void ShellContentRendererClient::RenderThreadStarted() {
  shell_observer_.reset(new ShellRenderProcessObserver());
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

void ShellContentRendererClient::WebTestProxyCreated(RenderView* render_view,
                                                     WebTestProxyBase* proxy) {
  WebKitTestRunner* test_runner = new WebKitTestRunner(render_view);
  if (!ShellRenderProcessObserver::GetInstance()->test_delegate()) {
    ShellRenderProcessObserver::GetInstance()->SetMainWindow(render_view,
                                                             test_runner);
  }
  test_runner->set_proxy(proxy);
  proxy->setDelegate(
      ShellRenderProcessObserver::GetInstance()->test_delegate());
  proxy->setInterfaces(
      ShellRenderProcessObserver::GetInstance()->test_interfaces());
}

}  // namespace content
