// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/renderer/layout_test/layout_test_content_renderer_client.h"

#include "base/callback.h"
#include "base/command_line.h"
#include "base/debug/debugger.h"
#include "components/web_cache/renderer/web_cache_render_process_observer.h"
#include "content/public/common/content_constants.h"
#include "content/public/common/content_switches.h"
#include "content/public/renderer/render_view.h"
#include "content/public/test/layouttest_support.h"
#include "content/shell/common/shell_switches.h"
#include "content/shell/common/webkit_test_helpers.h"
#include "content/shell/renderer/layout_test/layout_test_render_frame_observer.h"
#include "content/shell/renderer/layout_test/layout_test_render_process_observer.h"
#include "content/shell/renderer/layout_test/webkit_test_runner.h"
#include "content/shell/renderer/shell_render_view_observer.h"
#include "content/shell/renderer/test_runner/mock_credential_manager_client.h"
#include "content/shell/renderer/test_runner/web_test_interfaces.h"
#include "content/shell/renderer/test_runner/web_test_proxy.h"
#include "content/test/mock_webclipboard_impl.h"
#include "ppapi/shared_impl/ppapi_switches.h"
#include "third_party/WebKit/public/platform/WebMediaStreamCenter.h"
#include "third_party/WebKit/public/web/WebPluginParams.h"
#include "third_party/WebKit/public/web/WebView.h"
#include "v8/include/v8.h"

#if defined(OS_WIN)
#include "content/public/renderer/render_font_warmup_win.h"
#include "third_party/WebKit/public/web/win/WebFontRendering.h"
#include "third_party/skia/include/ports/SkFontMgr.h"
#include "ui/gfx/win/direct_write.h"
#endif

using blink::WebAudioDevice;
using blink::WebClipboard;
using blink::WebLocalFrame;
using blink::WebMIDIAccessor;
using blink::WebMIDIAccessorClient;
using blink::WebMediaStreamCenter;
using blink::WebMediaStreamCenterClient;
using blink::WebPlugin;
using blink::WebPluginParams;
using blink::WebRTCPeerConnectionHandler;
using blink::WebRTCPeerConnectionHandlerClient;
using blink::WebThemeEngine;

namespace content {

#if defined(OS_WIN)
namespace {

// DirectWrite only has access to %WINDIR%\Fonts by default. For developer
// side-loading, support kRegisterFontFiles to allow access to additional fonts.
void RegisterSideloadedTypefaces(SkFontMgr* fontmgr) {
  std::vector<std::string> files = GetSideloadFontFiles();
  for (std::vector<std::string>::const_iterator i(files.begin());
       i != files.end();
       ++i) {
    SkTypeface* typeface = fontmgr->createFromFile(i->c_str());
    DoPreSandboxWarmupForTypeface(typeface);
    blink::WebFontRendering::addSideloadedFontForTesting(typeface);
  }
}

}  // namespace
#endif  // OS_WIN

LayoutTestContentRendererClient::LayoutTestContentRendererClient() {
  EnableWebTestProxyCreation(
      base::Bind(&LayoutTestContentRendererClient::WebTestProxyCreated,
                 base::Unretained(this)));

#if defined(OS_WIN)
  if (gfx::win::ShouldUseDirectWrite())
    RegisterSideloadedTypefaces(GetPreSandboxWarmupFontMgr());
#endif
}

LayoutTestContentRendererClient::~LayoutTestContentRendererClient() {
}

void LayoutTestContentRendererClient::RenderThreadStarted() {
  ShellContentRendererClient::RenderThreadStarted();
  shell_observer_.reset(new LayoutTestRenderProcessObserver());
}

void LayoutTestContentRendererClient::RenderFrameCreated(
    RenderFrame* render_frame) {
  new LayoutTestRenderFrameObserver(render_frame);
}

void LayoutTestContentRendererClient::RenderViewCreated(
    RenderView* render_view) {
  new ShellRenderViewObserver(render_view);

  WebKitTestRunner* test_runner = WebKitTestRunner::Get(render_view);
  test_runner->Reset();
  render_view->GetWebView()->setSpellCheckClient(
      test_runner->proxy()->GetSpellCheckClient());

  render_view->GetWebView()->setCredentialManagerClient(
      test_runner->proxy()->GetCredentialManagerClientMock());
  WebTestDelegate* delegate =
      LayoutTestRenderProcessObserver::GetInstance()->test_delegate();
  if (delegate == static_cast<WebTestDelegate*>(test_runner))
    LayoutTestRenderProcessObserver::GetInstance()->SetMainWindow(render_view);
}

WebMediaStreamCenter*
LayoutTestContentRendererClient::OverrideCreateWebMediaStreamCenter(
    WebMediaStreamCenterClient* client) {
#if defined(ENABLE_WEBRTC)
  WebTestInterfaces* interfaces =
      LayoutTestRenderProcessObserver::GetInstance()->test_interfaces();
  return interfaces->CreateMediaStreamCenter(client);
#else
  return NULL;
#endif
}

WebRTCPeerConnectionHandler*
LayoutTestContentRendererClient::OverrideCreateWebRTCPeerConnectionHandler(
    WebRTCPeerConnectionHandlerClient* client) {
#if defined(ENABLE_WEBRTC)
  WebTestInterfaces* interfaces =
      LayoutTestRenderProcessObserver::GetInstance()->test_interfaces();
  return interfaces->CreateWebRTCPeerConnectionHandler(client);
#else
  return NULL;
#endif
}

WebMIDIAccessor*
LayoutTestContentRendererClient::OverrideCreateMIDIAccessor(
    WebMIDIAccessorClient* client) {
  WebTestInterfaces* interfaces =
      LayoutTestRenderProcessObserver::GetInstance()->test_interfaces();
  return interfaces->CreateMIDIAccessor(client);
}

WebAudioDevice*
LayoutTestContentRendererClient::OverrideCreateAudioDevice(
    double sample_rate) {
  WebTestInterfaces* interfaces =
      LayoutTestRenderProcessObserver::GetInstance()->test_interfaces();
  return interfaces->CreateAudioDevice(sample_rate);
}

WebClipboard* LayoutTestContentRendererClient::OverrideWebClipboard() {
  if (!clipboard_)
    clipboard_.reset(new MockWebClipboardImpl);
  return clipboard_.get();
}

WebThemeEngine* LayoutTestContentRendererClient::OverrideThemeEngine() {
  return LayoutTestRenderProcessObserver::GetInstance()
      ->test_interfaces()
      ->ThemeEngine();
}

void LayoutTestContentRendererClient::WebTestProxyCreated(
    RenderView* render_view,
    WebTestProxyBase* proxy) {
  WebKitTestRunner* test_runner = new WebKitTestRunner(render_view);
  test_runner->set_proxy(proxy);
  if (!LayoutTestRenderProcessObserver::GetInstance()->test_delegate()) {
    LayoutTestRenderProcessObserver::GetInstance()->SetTestDelegate(
        test_runner);
  }
  proxy->SetInterfaces(
      LayoutTestRenderProcessObserver::GetInstance()->test_interfaces());
  test_runner->proxy()->SetDelegate(
      LayoutTestRenderProcessObserver::GetInstance()->test_delegate());
}

}  // namespace content
