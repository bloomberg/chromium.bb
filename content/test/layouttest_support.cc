// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/layouttest_support.h"

#include "base/callback.h"
#include "base/lazy_instance.h"
#include "content/common/gpu/image_transport_surface.h"
#include "content/renderer/devtools/devtools_client.h"
#include "content/renderer/render_thread_impl.h"
#include "content/renderer/render_view_impl.h"
#include "content/renderer/renderer_webapplicationcachehost_impl.h"
#include "content/renderer/renderer_webkitplatformsupport_impl.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebGamepads.h"
#include "third_party/WebKit/Tools/DumpRenderTree/chromium/TestRunner/public/WebTestProxy.h"

using WebKit::WebGamepads;
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

}  // namespace


void EnableWebTestProxyCreation(
    const base::Callback<void(RenderView*, WebTestProxyBase*)>& callback) {
  g_callback.Get() = callback;
  RenderViewImpl::InstallCreateHook(CreateWebTestProxy);
}

void SetMockGamepads(const WebGamepads& pads) {
  RendererWebKitPlatformSupportImpl::SetMockGamepadsForTesting(pads);
}

void DisableAppCacheLogging() {
  RendererWebApplicationCacheHostImpl::DisableLoggingForTesting();
}

void EnableDevToolsFrontendTesting() {
  DevToolsClient::EnableDevToolsFrontendTesting();
}

int GetLocalSessionHistoryLength(RenderView* render_view) {
  return static_cast<RenderViewImpl*>(render_view)
      ->GetLocalSessionHistoryLengthForTesting();
}

void SetAllowOSMesaImageTransportForTesting() {
#if defined(OS_MACOSX)
  ImageTransportSurface::SetAllowOSMesaForTesting(true);
#endif
}

void DoNotRequireUserGestureForFocusChanges() {
  RenderThreadImpl::current()->set_require_user_gesture_for_focus(false);
}

void SyncNavigationState(RenderView* render_view) {
  static_cast<RenderViewImpl*>(render_view)->SyncNavigationState();
}

void SetFocusAndActivate(RenderView* render_view, bool enable) {
  static_cast<RenderViewImpl*>(render_view)
      ->SetFocusAndActivateForTesting(enable);
}

}  // namespace content
