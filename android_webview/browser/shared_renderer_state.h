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
  int width;
  int height;

  DrawGLInput();
};

// Set by HardwareRenderer and read by BrowserViewRenderer.
struct DrawGLResult {
  unsigned int frame_id;
  bool clip_contains_visible_rect;

  DrawGLResult();
};

// This class holds renderer state that is shared between UI and RT threads.
// Android framework will block the UI thread when RT is drawing, so no locking
// is needed in this class. In the interim, this class is also responsible for
// thread hopping that should eventually be removed once RT support work is
// complete.
class SharedRendererState {
 public:
  SharedRendererState(scoped_refptr<base::MessageLoopProxy> ui_loop,
                      BrowserViewRendererClient* client);
  ~SharedRendererState();

  void ClientRequestDrawGL();

  // This function should only be called on UI thread.
  void SetCompositorOnUiThread(content::SynchronousCompositor* compositor);

  // This function can be called on both UI and RT thread.
  content::SynchronousCompositor* GetCompositor();

  void SetMemoryPolicy(const content::SynchronousCompositorMemoryPolicy policy);
  content::SynchronousCompositorMemoryPolicy GetMemoryPolicy() const;

  void SetMemoryPolicyDirty(bool is_dirty);
  bool IsMemoryPolicyDirty() const;
  void SetDrawGLInput(const DrawGLInput& input);
  DrawGLInput GetDrawGLInput() const;

  // Set by UI and read by RT.
  void SetHardwareAllowed(bool allowed);
  bool IsHardwareAllowed() const;

  // Set by RT and read by UI.
  void SetHardwareInitialized(bool initialized);
  bool IsHardwareInitialized() const;

 private:
  void ClientRequestDrawGLOnUIThread();

  scoped_refptr<base::MessageLoopProxy> ui_loop_;
  // TODO(boliu): Remove |client_on_ui_| from shared state.
  BrowserViewRendererClient* client_on_ui_;
  base::WeakPtrFactory<SharedRendererState> weak_factory_on_ui_thread_;
  base::WeakPtr<SharedRendererState> ui_thread_weak_ptr_;

  // Accessed by both UI and RT thread.
  mutable base::Lock lock_;
  content::SynchronousCompositor* compositor_;
  content::SynchronousCompositorMemoryPolicy memory_policy_;
  // Set to true when SetMemoryPolicy called with a different memory policy.
  // Set to false when memory policy is read and enforced to compositor.
  bool memory_policy_dirty_;
  DrawGLInput draw_gl_input_;
  bool hardware_allowed_;
  bool hardware_initialized_;
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_SHARED_RENDERER_STATE_H_
