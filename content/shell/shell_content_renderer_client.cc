// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/shell_content_renderer_client.h"

#include "base/callback.h"
#include "base/command_line.h"
#include "content/public/common/content_constants.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "content/public/renderer/render_view.h"
#include "content/public/test/layouttest_support.h"
#include "content/shell/shell_render_process_observer.h"
#include "content/shell/shell_switches.h"
#include "content/shell/webkit_test_runner.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebPluginParams.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"
#include "third_party/WebKit/Tools/DumpRenderTree/chromium/TestRunner/public/WebTestPlugin.h"
#include "third_party/WebKit/Tools/DumpRenderTree/chromium/TestRunner/public/WebTestProxy.h"
#include "v8/include/v8.h"

using WebKit::WebFrame;
using WebTestRunner::WebTestPlugin;
using WebTestRunner::WebTestProxyBase;

namespace content {

namespace {

bool IsLocalhost(const std::string& host) {
  return host == "127.0.0.1" || host == "localhost";
}

bool HostIsUsedBySomeTestsToGenerateError(const std::string& host) {
  return host == "255.255.255.255";
}

bool IsExternalPage(const GURL& url) {
  return !url.host().empty() &&
         (url.SchemeIs(chrome::kHttpScheme) ||
          url.SchemeIs(chrome::kHttpsScheme)) &&
         !IsLocalhost(url.host()) &&
         !HostIsUsedBySomeTestsToGenerateError(url.host());
}

}  // namespace

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
  if (params.mimeType == WebTestPlugin::mimeType()) {
    *plugin = WebTestPlugin::create(
        frame,
        params,
        ShellRenderProcessObserver::GetInstance()->test_delegate());
    return true;
  }
  return false;
}

bool ShellContentRendererClient::WillSendRequest(
    WebFrame* frame,
    PageTransition transition_type,
    const GURL& url,
    const GURL& first_party_for_cookies,
    GURL* new_url) {
  CommandLine* command_line = CommandLine::ForCurrentProcess();
  if (!command_line->HasSwitch(switches::kDumpRenderTree))
    return false;
  ShellRenderProcessObserver* render_process_observer =
      ShellRenderProcessObserver::GetInstance();
  if (!command_line->HasSwitch(switches::kAllowExternalPages) &&
      IsExternalPage(url) && !IsExternalPage(first_party_for_cookies)) {
    render_process_observer->test_delegate()->printMessage(
        std::string("Blocked access to external URL " + url.spec() + "\n"));
    *new_url = GURL();
    return true;
  }
  *new_url = render_process_observer->test_delegate()->rewriteLayoutTestsURL(
      url.spec());
  return true;
}

void ShellContentRendererClient::WebTestProxyCreated(RenderView* render_view,
                                                     WebTestProxyBase* proxy) {
  WebKitTestRunner* test_runner = new WebKitTestRunner(render_view);
  if (!ShellRenderProcessObserver::GetInstance()->test_delegate()) {
    ShellRenderProcessObserver::GetInstance()->SetMainWindow(render_view,
                                                             test_runner,
                                                             test_runner);
  }
  test_runner->set_proxy(proxy);
  proxy->setDelegate(
      ShellRenderProcessObserver::GetInstance()->test_delegate());
  proxy->setInterfaces(
      ShellRenderProcessObserver::GetInstance()->test_interfaces());
  render_view->GetWebView()->setSpellCheckClient(proxy->spellCheckClient());
}

}  // namespace content
