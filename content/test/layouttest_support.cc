// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/layouttest_support.h"

#include "base/callback.h"
#include "base/lazy_instance.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/common/gpu/image_transport_surface.h"
#include "content/renderer/render_thread_impl.h"
#include "content/renderer/render_view_impl.h"
#include "content/renderer/renderer_webkitplatformsupport_impl.h"
#include "content/shell/renderer/test_runner/WebFrameTestProxy.h"
#include "content/shell/renderer/test_runner/WebTestProxy.h"
#include "content/test/test_media_stream_client.h"
#include "third_party/WebKit/public/platform/WebDeviceMotionData.h"
#include "third_party/WebKit/public/platform/WebDeviceOrientationData.h"
#include "third_party/WebKit/public/platform/WebGamepads.h"

#if defined(OS_MACOSX)
#include "content/browser/renderer_host/popup_menu_helper_mac.h"
#endif

using blink::WebDeviceMotionData;
using blink::WebDeviceOrientationData;
using blink::WebGamepad;
using blink::WebGamepads;
using blink::WebRect;
using blink::WebSize;
using WebTestRunner::WebFrameTestProxy;
using WebTestRunner::WebTestProxy;
using WebTestRunner::WebTestProxyBase;

namespace content {

namespace {

base::LazyInstance<base::Callback<void(RenderView*, WebTestProxyBase*)> >::Leaky
    g_callback = LAZY_INSTANCE_INITIALIZER;

RenderViewImpl* CreateWebTestProxy(RenderViewImplParams* params) {
  typedef WebTestProxy<RenderViewImpl, RenderViewImplParams*> ProxyType;
  ProxyType* render_view_proxy = new ProxyType(
      reinterpret_cast<RenderViewImplParams*>(params));
  if (g_callback == 0)
    return render_view_proxy;
  g_callback.Get().Run(
      static_cast<RenderView*>(render_view_proxy), render_view_proxy);
  return render_view_proxy;
}

WebTestProxyBase* GetWebTestProxyBase(RenderViewImpl* render_view) {
  typedef WebTestProxy<RenderViewImpl, RenderViewImplParams*> ViewProxy;

  ViewProxy* render_view_proxy = static_cast<ViewProxy*>(render_view);
  return static_cast<WebTestProxyBase*>(render_view_proxy);
}

RenderFrameImpl* CreateWebFrameTestProxy(
    RenderViewImpl* render_view,
    int32 routing_id) {
  typedef WebFrameTestProxy<RenderFrameImpl, RenderViewImpl*, int32> FrameProxy;

  FrameProxy* render_frame_proxy = new FrameProxy(render_view, routing_id);
  render_frame_proxy->setBaseProxy(GetWebTestProxyBase(render_view));

  return render_frame_proxy;
}

}  // namespace


void EnableWebTestProxyCreation(
    const base::Callback<void(RenderView*, WebTestProxyBase*)>& callback) {
  g_callback.Get() = callback;
  RenderViewImpl::InstallCreateHook(CreateWebTestProxy);
  RenderFrameImpl::InstallCreateHook(CreateWebFrameTestProxy);
}

void SetMockGamepads(const WebGamepads& pads) {
  RendererWebKitPlatformSupportImpl::SetMockGamepadsForTesting(pads);
}

void MockGamepadConnected(int index, const WebGamepad& pad) {
  RendererWebKitPlatformSupportImpl::MockGamepadConnected(index, pad);
}

void MockGamepadDisconnected(int index, const WebGamepad& pad) {
  RendererWebKitPlatformSupportImpl::MockGamepadDisconnected(index, pad);
}

void SetMockDeviceMotionData(const WebDeviceMotionData& data) {
  RendererWebKitPlatformSupportImpl::SetMockDeviceMotionDataForTesting(data);
}

void SetMockDeviceOrientationData(const WebDeviceOrientationData& data) {
  RendererWebKitPlatformSupportImpl::
      SetMockDeviceOrientationDataForTesting(data);
}

void SetMockScreenOrientation(const blink::WebScreenOrientation& orientation) {
  RendererWebKitPlatformSupportImpl::
      SetMockScreenOrientationForTesting(orientation);
}

void EnableRendererLayoutTestMode() {
  RenderThreadImpl::current()->set_layout_test_mode(true);
}

void EnableBrowserLayoutTestMode() {
#if defined(OS_MACOSX)
  ImageTransportSurface::SetAllowOSMesaForTesting(true);
  PopupMenuHelper::DontShowPopupMenuForTesting();
#endif
  RenderWidgetHostImpl::DisableResizeAckCheckForTesting();
}

int GetLocalSessionHistoryLength(RenderView* render_view) {
  return static_cast<RenderViewImpl*>(render_view)->
      GetLocalSessionHistoryLengthForTesting();
}

void SyncNavigationState(RenderView* render_view) {
  static_cast<RenderViewImpl*>(render_view)->SyncNavigationState();
}

void SetFocusAndActivate(RenderView* render_view, bool enable) {
  static_cast<RenderViewImpl*>(render_view)->
      SetFocusAndActivateForTesting(enable);
}

void ForceResizeRenderView(RenderView* render_view,
                           const WebSize& new_size) {
  RenderViewImpl* render_view_impl = static_cast<RenderViewImpl*>(render_view);
  render_view_impl->ForceResizeForTesting(new_size);
  GetWebTestProxyBase(render_view_impl)->didForceResize();
}

void SetDeviceScaleFactor(RenderView* render_view, float factor) {
  static_cast<RenderViewImpl*>(render_view)->
      SetDeviceScaleFactorForTesting(factor);
}

void UseSynchronousResizeMode(RenderView* render_view, bool enable) {
  static_cast<RenderViewImpl*>(render_view)->
      UseSynchronousResizeModeForTesting(enable);
}

void EnableAutoResizeMode(RenderView* render_view,
                          const WebSize& min_size,
                          const WebSize& max_size) {
  static_cast<RenderViewImpl*>(render_view)->
      EnableAutoResizeForTesting(min_size, max_size);
}

void DisableAutoResizeMode(RenderView* render_view, const WebSize& new_size) {
  static_cast<RenderViewImpl*>(render_view)->
      DisableAutoResizeForTesting(new_size);
}

void UseMockMediaStreams(RenderView* render_view) {
  RenderViewImpl* render_view_impl = static_cast<RenderViewImpl*>(render_view);
  render_view_impl->SetMediaStreamClientForTesting(
      new TestMediaStreamClient(render_view_impl));
}

}  // namespace content
