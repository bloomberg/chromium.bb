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

// TODO(vangelis): Remove once the method is removed from the WebGLES2Context.
bool WebGLES2ContextImpl::initialize(WebKit::WebView* web_view) {
    return initialize(web_view, NULL);
}

bool WebGLES2ContextImpl::initialize(
    WebKit::WebView* web_view,
    WebGLES2Context* parent) {
  // Windowed contexts don't have a parent context.
  DCHECK(!(web_view && parent));

  RenderThread* render_thread = RenderThread::current();
  if (!render_thread)
    return false;
  GpuChannelHost* host = render_thread->EstablishGpuChannelSync();
  if (!host)
    return false;
  DCHECK(host->state() == GpuChannelHost::CONNECTED);

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
    ggl::Context* parent_context = NULL;

    if (parent) {
      WebGLES2ContextImpl* parent_context_impl =
          static_cast<WebGLES2ContextImpl*>(parent);
      parent_context = parent_context_impl->context();
      DCHECK(parent_context);
    }
    context_ = ggl::CreateOffscreenContext(host, parent_context,
                                           gfx::Size(1, 1));
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
  return ggl::SwapBuffers(context_);
}

void WebGLES2ContextImpl::resizeOffscreenContent(const WebKit::WebSize& size) {
  ggl::ResizeOffscreenContext(context_, size);
}

unsigned WebGLES2ContextImpl::getOffscreenContentParentTextureId() {
  return ggl::GetParentTextureId(context_);
}

#endif  // defined(ENABLE_GPU)

