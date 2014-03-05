// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_SHARED_RENDERER_STATE_H_
#define ANDROID_WEBVIEW_BROWSER_SHARED_RENDERER_STATE_H_

#include "base/message_loop/message_loop_proxy.h"
#include "base/synchronization/lock.h"
#include "content/public/browser/android/synchronous_compositor.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/vector2d.h"

namespace android_webview {

class BrowserViewRendererClient;

// Set by BrowserViewRenderer and read by HardwareRenderer.
struct DrawGLInput {
  unsigned int frame_id;
  gfx::Rect global_visible_rect;
  gfx::Vector2d scroll_offset;

  DrawGLInput();
};

// Set by HardwareRenderer and read by BrowserViewRenderer.
struct DrawGLResult {
  unsigned int frame_id;
  bool clip_contains_visible_rect;

  DrawGLResult();
};

namespace internal {

// Holds variables that are accessed on both UI and RT threads and need to be
// lock-protected when accessed. Separate struct purely for syntactic barrier
// to implement the AssertAcquired check.
struct BothThreads {
  // TODO(boliu): Remove |compositor| from shared state.
  content::SynchronousCompositor* compositor;
  DrawGLInput draw_gl_input;

  BothThreads();
};

};  // namespace internal

// This class holds renderer state that is shared between UI and RT threads
// and lock protects all access the state. In the interim, this class is
// also responsible for thread hopping and lock protection beyond simple
// state that should eventually be removed once RT support work is complete.
class SharedRendererState {
 public:
  SharedRendererState(scoped_refptr<base::MessageLoopProxy> ui_loop,
                      BrowserViewRendererClient* client);
  ~SharedRendererState();

  void ClientRequestDrawGL();

  // Holds the compositor and lock protects all calls into it.
  void SetCompositorOnUiThread(content::SynchronousCompositor* compositor);
  bool CompositorInitializeHwDraw(scoped_refptr<gfx::GLSurface> surface);
  void CompositorReleaseHwDraw();
  bool CompositorDemandDrawHw(gfx::Size surface_size,
                              const gfx::Transform& transform,
                              gfx::Rect viewport,
                              gfx::Rect clip,
                              bool stencil_enabled);
  bool CompositorDemandDrawSw(SkCanvas* canvas);
  void CompositorSetMemoryPolicy(
      const content::SynchronousCompositorMemoryPolicy& policy);
  void CompositorDidChangeRootLayerScrollOffset();

  void SetDrawGLInput(const DrawGLInput& input);
  DrawGLInput GetDrawGLInput() const;

 private:
  void ClientRequestDrawGLOnUIThread();
  internal::BothThreads& both();
  const internal::BothThreads& both() const;

  scoped_refptr<base::MessageLoopProxy> ui_loop_;
  // TODO(boliu): Remove |client_on_ui_| from shared state.
  BrowserViewRendererClient* client_on_ui_;
  base::WeakPtrFactory<SharedRendererState> weak_factory_on_ui_thread_;
  base::WeakPtr<SharedRendererState> ui_thread_weak_ptr_;

  mutable base::Lock lock_;
  internal::BothThreads both_threads_;
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_SHARED_RENDERER_STATE_H_
