// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/renderer/layout_test/layout_test_content_renderer_client.h"

#include "base/callback.h"
#include "base/command_line.h"
#include "base/debug/debugger.h"
#include "components/test_runner/mock_credential_manager_client.h"
#include "components/test_runner/web_frame_test_proxy.h"
#include "components/test_runner/web_test_interfaces.h"
#include "components/test_runner/web_test_runner.h"
#include "components/test_runner/web_view_test_proxy.h"
#include "components/web_cache/renderer/web_cache_impl.h"
#include "content/public/common/content_constants.h"
#include "content/public/common/content_switches.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_thread.h"
#include "content/public/renderer/render_view.h"
#include "content/public/test/layouttest_support.h"
#include "content/shell/common/layout_test/layout_test_switches.h"
#include "content/shell/common/shell_switches.h"
#include "content/shell/renderer/layout_test/blink_test_helpers.h"
#include "content/shell/renderer/layout_test/blink_test_runner.h"
#include "content/shell/renderer/layout_test/interface_registry_js_wrapper.h"
#include "content/shell/renderer/layout_test/layout_test_render_frame_observer.h"
#include "content/shell/renderer/layout_test/layout_test_render_thread_observer.h"
#include "content/shell/renderer/layout_test/test_media_stream_renderer_factory.h"
#include "content/shell/renderer/shell_render_view_observer.h"
#include "content/test/mock_webclipboard_impl.h"
#include "gin/modules/module_registry.h"
#include "media/media_features.h"
#include "ppapi/shared_impl/ppapi_switches.h"
#include "third_party/WebKit/public/platform/WebMediaStreamCenter.h"
#include "third_party/WebKit/public/web/WebFrameWidget.h"
#include "third_party/WebKit/public/web/WebKit.h"
#include "third_party/WebKit/public/web/WebPluginParams.h"
#include "third_party/WebKit/public/web/WebRuntimeFeatures.h"
#include "third_party/WebKit/public/web/WebTestingSupport.h"
#include "third_party/WebKit/public/web/WebView.h"
#include "ui/gfx/icc_profile.h"
#include "v8/include/v8.h"

using blink::WebAudioDevice;
using blink::WebClipboard;
using blink::WebFrame;
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

namespace {

void WebViewTestProxyCreated(RenderView* render_view,
                             test_runner::WebViewTestProxyBase* proxy) {
  test_runner::WebTestInterfaces* interfaces =
      LayoutTestRenderThreadObserver::GetInstance()->test_interfaces();

  BlinkTestRunner* test_runner = new BlinkTestRunner(render_view);
  // TODO(lukasza): Using the 1st BlinkTestRunner as the main delegate is wrong,
  // but it is difficult to change because this behavior has been baked for a
  // long time into test assumptions (i.e. which PrintMessage gets delivered to
  // the browser depends on this).
  static bool first_test_runner = true;
  if (first_test_runner) {
    first_test_runner = false;
    interfaces->SetDelegate(test_runner);
  }

  proxy->set_delegate(test_runner);
  proxy->set_view_test_client(LayoutTestRenderThreadObserver::GetInstance()
                                  ->test_interfaces()
                                  ->CreateWebViewTestClient(proxy));
  std::unique_ptr<test_runner::WebWidgetTestClient> widget_test_client =
      LayoutTestRenderThreadObserver::GetInstance()
          ->test_interfaces()
          ->CreateWebWidgetTestClient(proxy);
  proxy->set_widget_test_client(std::move(widget_test_client));
  proxy->SetInterfaces(interfaces);
}

void WebWidgetTestProxyCreated(blink::WebWidget* web_widget,
                               test_runner::WebWidgetTestProxyBase* proxy) {
  CHECK(web_widget->isWebFrameWidget());
  proxy->set_web_widget(web_widget);
  blink::WebFrameWidget* web_frame_widget =
      static_cast<blink::WebFrameWidget*>(web_widget);
  blink::WebView* web_view = web_frame_widget->localRoot()->view();
  RenderView* render_view = RenderView::FromWebView(web_view);
  test_runner::WebViewTestProxyBase* view_proxy =
      GetWebViewTestProxyBase(render_view);
  std::unique_ptr<test_runner::WebWidgetTestClient> widget_test_client =
      LayoutTestRenderThreadObserver::GetInstance()
          ->test_interfaces()
          ->CreateWebWidgetTestClient(proxy);
  proxy->set_web_view_test_proxy_base(view_proxy);
  proxy->set_widget_test_client(std::move(widget_test_client));
}

void WebFrameTestProxyCreated(RenderFrame* render_frame,
                              test_runner::WebFrameTestProxyBase* proxy) {
  test_runner::WebViewTestProxyBase* web_view_test_proxy_base =
      GetWebViewTestProxyBase(render_frame->GetRenderView());
  proxy->set_test_client(
      LayoutTestRenderThreadObserver::GetInstance()
          ->test_interfaces()
          ->CreateWebFrameTestClient(web_view_test_proxy_base, proxy));
}

}  // namespace

LayoutTestContentRendererClient::LayoutTestContentRendererClient() {
  EnableWebTestProxyCreation(base::Bind(&WebViewTestProxyCreated),
                             base::Bind(&WebWidgetTestProxyCreated),
                             base::Bind(&WebFrameTestProxyCreated));
}

LayoutTestContentRendererClient::~LayoutTestContentRendererClient() {
}

void LayoutTestContentRendererClient::RenderThreadStarted() {
  ShellContentRendererClient::RenderThreadStarted();
  shell_observer_.reset(new LayoutTestRenderThreadObserver());
}

void LayoutTestContentRendererClient::RenderFrameCreated(
    RenderFrame* render_frame) {
  test_runner::WebFrameTestProxyBase* frame_proxy =
      GetWebFrameTestProxyBase(render_frame);
  frame_proxy->set_web_frame(render_frame->GetWebFrame());
  new LayoutTestRenderFrameObserver(render_frame);
}

void LayoutTestContentRendererClient::RenderViewCreated(
    RenderView* render_view) {
  new ShellRenderViewObserver(render_view);

  test_runner::WebViewTestProxyBase* proxy =
      GetWebViewTestProxyBase(render_view);
  proxy->set_web_view(render_view->GetWebView());
  // TODO(lfg): We should fix the TestProxy to track the WebWidgets on every
  // local root in WebFrameTestProxy instead of having only the WebWidget for
  // the main frame in WebViewTestProxy.
  proxy->set_web_widget(render_view->GetWebView()->widget());
  proxy->Reset();

  BlinkTestRunner* test_runner = BlinkTestRunner::Get(render_view);
  test_runner->Reset(false /* for_new_test */);

  LayoutTestRenderThreadObserver::GetInstance()
      ->test_interfaces()
      ->TestRunner()
      ->InitializeWebViewWithMocks(render_view->GetWebView());
}

WebMediaStreamCenter*
LayoutTestContentRendererClient::OverrideCreateWebMediaStreamCenter(
    WebMediaStreamCenterClient* client) {
#if BUILDFLAG(ENABLE_WEBRTC)
  test_runner::WebTestInterfaces* interfaces =
      LayoutTestRenderThreadObserver::GetInstance()->test_interfaces();
  return interfaces->CreateMediaStreamCenter(client);
#else
  return NULL;
#endif
}

WebRTCPeerConnectionHandler*
LayoutTestContentRendererClient::OverrideCreateWebRTCPeerConnectionHandler(
    WebRTCPeerConnectionHandlerClient* client) {
#if BUILDFLAG(ENABLE_WEBRTC)
  test_runner::WebTestInterfaces* interfaces =
      LayoutTestRenderThreadObserver::GetInstance()->test_interfaces();
  return interfaces->CreateWebRTCPeerConnectionHandler(client);
#else
  return NULL;
#endif
}

WebMIDIAccessor*
LayoutTestContentRendererClient::OverrideCreateMIDIAccessor(
    WebMIDIAccessorClient* client) {
  test_runner::WebTestInterfaces* interfaces =
      LayoutTestRenderThreadObserver::GetInstance()->test_interfaces();
  return interfaces->CreateMIDIAccessor(client);
}

WebAudioDevice*
LayoutTestContentRendererClient::OverrideCreateAudioDevice(
    double sample_rate) {
  test_runner::WebTestInterfaces* interfaces =
      LayoutTestRenderThreadObserver::GetInstance()->test_interfaces();
  return interfaces->CreateAudioDevice(sample_rate);
}

WebClipboard* LayoutTestContentRendererClient::OverrideWebClipboard() {
  if (!clipboard_)
    clipboard_.reset(new MockWebClipboardImpl);
  return clipboard_.get();
}

WebThemeEngine* LayoutTestContentRendererClient::OverrideThemeEngine() {
  return LayoutTestRenderThreadObserver::GetInstance()
      ->test_interfaces()
      ->ThemeEngine();
}

std::unique_ptr<MediaStreamRendererFactory>
LayoutTestContentRendererClient::CreateMediaStreamRendererFactory() {
#if BUILDFLAG(ENABLE_WEBRTC)
  return std::unique_ptr<MediaStreamRendererFactory>(
      new TestMediaStreamRendererFactory());
#else
  return nullptr;
#endif
}

std::unique_ptr<gfx::ICCProfile>
LayoutTestContentRendererClient::GetImageDecodeColorProfile() {
  // TODO(ccameron): Make all platforms use the same color profile for layout
  // tests.
#if defined(OS_WIN)
  return base::WrapUnique(new gfx::ICCProfile(
      GetTestingICCProfile("sRGB")));
#elif defined(OS_MACOSX)
  return base::WrapUnique(new gfx::ICCProfile(
      GetTestingICCProfile("genericRGB")));
#else
  return base::WrapUnique(new gfx::ICCProfile());
#endif
}

void LayoutTestContentRendererClient::DidInitializeWorkerContextOnWorkerThread(
    v8::Local<v8::Context> context) {
  blink::WebTestingSupport::injectInternalsObject(context);
}

void LayoutTestContentRendererClient::RunScriptsAtDocumentEnd(
    RenderFrame* render_frame) {
  v8::Isolate* isolate = blink::mainThreadIsolate();
  v8::HandleScope handle_scope(isolate);
  blink::WebLocalFrame* frame = render_frame->GetWebFrame();
  v8::Local<v8::Context> context = frame->mainWorldScriptContext();
  v8::Context::Scope context_scope(context);

  gin::ModuleRegistry* registry = gin::ModuleRegistry::From(context);
  if (registry->available_modules().count(
          InterfaceRegistryJsWrapper::kPerFrameModuleName)) {
    return;
  }

  registry->AddBuiltinModule(
      isolate, InterfaceRegistryJsWrapper::kPerFrameModuleName,
      InterfaceRegistryJsWrapper::Create(isolate, context,
                                         render_frame->GetInterfaceRegistry())
          .ToV8());
  registry->AddBuiltinModule(
      isolate, InterfaceRegistryJsWrapper::kPerProcessModuleName,
      InterfaceRegistryJsWrapper::Create(
          isolate, context, RenderThread::Get()->GetInterfaceRegistry())
          .ToV8());
  registry->AttemptToLoadMoreModules(isolate);
}

void LayoutTestContentRendererClient::
    SetRuntimeFeaturesDefaultsBeforeBlinkInitialization() {
  // We always expose GC to layout tests.
  std::string flags("--expose-gc");
  v8::V8::SetFlagsFromString(flags.c_str(), static_cast<int>(flags.size()));
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kStableReleaseMode)) {
    blink::WebRuntimeFeatures::enableTestOnlyFeatures(true);
  }
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableFontAntialiasing)) {
    blink::setFontAntialiasingEnabledForTest(true);
  }
}

}  // namespace content
