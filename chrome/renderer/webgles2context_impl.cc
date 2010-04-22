// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if defined(ENABLE_GPU)

#include "chrome/renderer/webgles2context_impl.h"

#include "chrome/renderer/gpu_channel_host.h"
#include "chrome/renderer/render_thread.h"
#include "chrome/renderer/render_view.h"
#include "third_party/WebKit/WebKit/chromium/public/WebView.h"


WebGLES2ContextImpl::WebGLES2ContextImpl() : context_(NULL) {
}

WebGLES2ContextImpl::~WebGLES2ContextImpl() {
  destroy();
}

bool WebGLES2ContextImpl::initialize(WebKit::WebView* web_view) {
  RenderThread* render_thread = RenderThread::current();
  if (!render_thread)
    return false;
  GpuChannelHost* host = render_thread->EstablishGpuChannelSync();
  if (!host)
    return false;
  DCHECK(host->ready());

  // If a WebView is passed then create a context rendering directly
  // into the window used by the WebView, otherwise create an offscreen
  // context.
  if (web_view) {
    RenderView* renderview = RenderView::FromWebView(web_view);
    if (!renderview)
      return false;
    gfx::NativeViewId view_id = renderview->host_window();
    context_ = ggl::CreateViewContext(host, view_id);
  } else {
    context_ = ggl::CreateOffscreenContext(host, NULL, gfx::Size(1, 1));
  }

  return (context_ != NULL);
}

bool WebGLES2ContextImpl::makeCurrent() {
  return ggl::MakeCurrent(context_);
}

bool WebGLES2ContextImpl::destroy() {
  return ggl::DestroyContext(context_);
}

bool WebGLES2ContextImpl::swapBuffers() {
  return ggl::SwapBuffers();
}

#endif  // defined(ENABLE_GPU)

