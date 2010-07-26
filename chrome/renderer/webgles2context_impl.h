// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_WEBGLES2CONTEXT_IMPL_H_
#define CHROME_RENDERER_WEBGLES2CONTEXT_IMPL_H_
#pragma once

#if defined(ENABLE_GPU)

#include "chrome/renderer/ggl/ggl.h"
#include "third_party/WebKit/WebKit/chromium/public/WebGLES2Context.h"
#include "third_party/WebKit/WebKit/chromium/public/WebSize.h"

class WebGLES2ContextImpl : public WebKit::WebGLES2Context {
 public:
  WebGLES2ContextImpl();
  virtual ~WebGLES2ContextImpl();

  // TODO(vangelis): Remove once method is removed from WebGLES2Context.
  virtual bool initialize(WebKit::WebView*);
  virtual bool initialize(WebKit::WebView*, WebGLES2Context* parent);
  virtual bool makeCurrent();
  virtual bool destroy();
  virtual bool swapBuffers();

  virtual void resizeOffscreenContent(const WebKit::WebSize&);
  virtual unsigned getOffscreenContentParentTextureId();

  ggl::Context* context() { return context_; }

 private:
  // The GGL context we use for OpenGL rendering.
  ggl::Context* context_;
  WebKit::WebView* web_view_;
};

#endif  // defined(ENABLE_GPU)
#endif  // CHROME_RENDERER_WEBGLES2CONTEXT_IMPL_H_

