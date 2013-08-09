// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/renderer/shell_content_renderer_client.h"

#include "base/callback.h"
#include "base/command_line.h"
#include "base/debug/debugger.h"
#include "content/public/common/content_constants.h"
#include "content/public/common/content_switches.h"
#include "content/public/renderer/render_view.h"
#include "content/public/test/layouttest_support.h"
#include "content/shell/common/shell_switches.h"
#include "content/shell/renderer/shell_render_process_observer.h"
#include "content/shell/renderer/shell_render_view_observer.h"
#include "content/shell/renderer/webkit_test_runner.h"
#include "third_party/WebKit/public/platform/WebMediaStreamCenter.h"
#include "third_party/WebKit/public/testing/WebTestInterfaces.h"
#include "third_party/WebKit/public/testing/WebTestProxy.h"
#include "third_party/WebKit/public/testing/WebTestRunner.h"
#include "third_party/WebKit/public/web/WebPluginParams.h"
#include "third_party/WebKit/public/web/WebView.h"
#include "v8/include/v8.h"
#include "webkit/support/mock_webclipboard_impl.h"

using WebKit::WebAudioDevice;
using WebKit::WebClipboard;
using WebKit::WebFrame;
using WebKit::WebMIDIAccessor;
using WebKit::WebMIDIAccessorClient;
using WebKit::WebMediaStreamCenter;
using WebKit::WebMediaStreamCenterClient;
using WebKit::WebPlugin;
using WebKit::WebPluginParams;
using WebKit::WebRTCPeerConnectionHandler;
using WebKit::WebRTCPeerConnectionHandlerClient;
using WebKit::WebThemeEngine;
using WebTestRunner::WebTestDelegate;
using WebTestRunner::WebTestInterfaces;
using WebTestRunner::WebTestProxyBase;

namespace content {

namespace {
ShellContentRendererClient* g_renderer_client;
}

ShellContentRendererClient* ShellContentRendererClient::Get() {
  return g_renderer_client;
}

ShellContentRendererClient::ShellContentRendererClient() {
  DCHECK(!g_renderer_client);
  g_renderer_client = this;
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kDumpRenderTree)) {
    EnableWebTestProxyCreation(
        base::Bind(&ShellContentRendererClient::WebTestProxyCreated,
                   base::Unretained(this)));
  }
}

ShellContentRendererClient::~ShellContentRendererClient() {
  g_renderer_client = NULL;
}

void ShellContentRendererClient::RenderThreadStarted() {
  shell_observer_.reset(new ShellRenderProcessObserver());
#if defined(OS_MACOSX)
  // We need to call this once before the sandbox was initialized to cache the
  // value.
  base::debug::BeingDebugged();
#endif
}

void ShellContentRendererClient::RenderViewCreated(RenderView* render_view) {
  new ShellRenderViewObserver(render_view);

  if (!CommandLine::ForCurrentProcess()->HasSwitch(switches::kDumpRenderTree))
    return;
  WebKitTestRunner* test_runner = WebKitTestRunner::Get(render_view);
  test_runner->Reset();
  render_view->GetWebView()->setSpellCheckClient(
      test_runner->proxy()->spellCheckClient());
  render_view->GetWebView()->setValidationMessageClient(
      test_runner->proxy()->validationMessageClient());
  render_view->GetWebView()->setPermissionClient(
      ShellRenderProcessObserver::GetInstance()->test_interfaces()->testRunner()
          ->webPermissions());
  WebTestDelegate* delegate =
      ShellRenderProcessObserver::GetInstance()->test_delegate();
  if (delegate == static_cast<WebTestDelegate*>(test_runner))
    ShellRenderProcessObserver::GetInstance()->SetMainWindow(render_view);
}

bool ShellContentRendererClient::OverrideCreatePlugin(
    RenderView* render_view,
    WebFrame* frame,
    const WebPluginParams& params,
    WebPlugin** plugin) {
  std::string mime_type = params.mimeType.utf8();
  if (mime_type == content::kBrowserPluginMimeType) {
    // Allow browser plugin in content_shell only if it is forced by flag.
    // Returning true here disables the plugin.
    return !CommandLine::ForCurrentProcess()->HasSwitch(
        switches::kEnableBrowserPluginForAllViewTypes);
  }
  return false;
}

WebMediaStreamCenter*
ShellContentRendererClient::OverrideCreateWebMediaStreamCenter(
    WebMediaStreamCenterClient* client) {
  if (!CommandLine::ForCurrentProcess()->HasSwitch(switches::kDumpRenderTree))
    return NULL;
#if defined(ENABLE_WEBRTC)
  WebTestInterfaces* interfaces =
      ShellRenderProcessObserver::GetInstance()->test_interfaces();
  return interfaces->createMediaStreamCenter(client);
#else
  return NULL;
#endif
}

WebRTCPeerConnectionHandler*
ShellContentRendererClient::OverrideCreateWebRTCPeerConnectionHandler(
    WebRTCPeerConnectionHandlerClient* client) {
  if (!CommandLine::ForCurrentProcess()->HasSwitch(switches::kDumpRenderTree))
    return NULL;
#if defined(ENABLE_WEBRTC)
  WebTestInterfaces* interfaces =
      ShellRenderProcessObserver::GetInstance()->test_interfaces();
  return interfaces->createWebRTCPeerConnectionHandler(client);
#else
  return NULL;
#endif
}

WebMIDIAccessor*
ShellContentRendererClient::OverrideCreateMIDIAccessor(
    WebMIDIAccessorClient* client) {
  WebTestInterfaces* interfaces =
      ShellRenderProcessObserver::GetInstance()->test_interfaces();
  return interfaces->createMIDIAccessor(client);
}

WebAudioDevice*
ShellContentRendererClient::OverrideCreateAudioDevice(
    double sample_rate) {
  WebTestInterfaces* interfaces =
      ShellRenderProcessObserver::GetInstance()->test_interfaces();
  return interfaces->createAudioDevice(sample_rate);
}

WebClipboard* ShellContentRendererClient::OverrideWebClipboard() {
  if (!CommandLine::ForCurrentProcess()->HasSwitch(switches::kDumpRenderTree))
    return NULL;
  if (!clipboard_)
    clipboard_.reset(new MockWebClipboardImpl);
  return clipboard_.get();
}

WebKit::WebCrypto* ShellContentRendererClient::OverrideWebCrypto() {
  if (!CommandLine::ForCurrentProcess()->HasSwitch(switches::kDumpRenderTree))
    return NULL;
  WebTestInterfaces* interfaces =
      ShellRenderProcessObserver::GetInstance()->test_interfaces();
  return interfaces->crypto();
}

WebThemeEngine* ShellContentRendererClient::OverrideThemeEngine() {
  if (!CommandLine::ForCurrentProcess()->HasSwitch(switches::kDumpRenderTree))
    return NULL;
  return ShellRenderProcessObserver::GetInstance()->test_interfaces()
      ->themeEngine();
}

void ShellContentRendererClient::WebTestProxyCreated(RenderView* render_view,
                                                     WebTestProxyBase* proxy) {
  WebKitTestRunner* test_runner = new WebKitTestRunner(render_view);
  test_runner->set_proxy(proxy);
  if (!ShellRenderProcessObserver::GetInstance()->test_delegate())
    ShellRenderProcessObserver::GetInstance()->SetTestDelegate(test_runner);
  proxy->setInterfaces(
      ShellRenderProcessObserver::GetInstance()->test_interfaces());
  test_runner->proxy()->setDelegate(
      ShellRenderProcessObserver::GetInstance()->test_delegate());
}

bool ShellContentRendererClient::AllowBrowserPlugin(
    WebKit::WebPluginContainer* container) {
  if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableBrowserPluginForAllViewTypes)) {
    // Allow BrowserPlugin if forced by command line flag. This is generally
    // true for tests.
    return true;
  }
  return ContentRendererClient::AllowBrowserPlugin(container);
}

}  // namespace content
