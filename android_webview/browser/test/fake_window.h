// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_TEST_FAKE_WINDOW_H_
#define ANDROID_WEBVIEW_BROWSER_TEST_FAKE_WINDOW_H_

#include <map>

#include "android_webview/public/browser/draw_gl.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/single_thread_task_runner.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_surface.h"

namespace base {
class Thread;
class WaitableEvent;
}

namespace android_webview {

class BrowserViewRenderer;
class RenderThreadManager;

class WindowHooks {
 public:
  virtual ~WindowHooks() {}

  virtual void WillOnDraw() = 0;
  virtual void DidOnDraw(bool success) = 0;

  virtual void WillSyncOnRT(RenderThreadManager* functor) = 0;
  virtual void DidSyncOnRT(RenderThreadManager* functor) = 0;
  virtual void WillProcessOnRT(RenderThreadManager* functor) = 0;
  virtual void DidProcessOnRT(RenderThreadManager* functor) = 0;
  virtual bool WillDrawOnRT(RenderThreadManager* functor,
                            AwDrawGLInfo* draw_info) = 0;
  virtual void DidDrawOnRT(RenderThreadManager* functor) = 0;
};

class FakeWindow {
 public:
  FakeWindow(BrowserViewRenderer* view,
             RenderThreadManager* functor,
             WindowHooks* hooks,
             gfx::Rect location);
  ~FakeWindow();

  void Detach();

  // BrowserViewRendererClient methods.
  void RequestInvokeGL(bool wait_for_completion);
  void PostInvalidate();
  const gfx::Size& surface_size() { return surface_size_; }

 private:
  class ScopedMakeCurrent;

  void OnDrawHardware();
  void CheckCurrentlyOnUIThread();
  void CreateRenderThreadIfNeeded();

  void InitializeOnRT(base::WaitableEvent* sync);
  void DestroyOnRT(base::WaitableEvent* sync);
  void ProcessFunctorOnRT(base::WaitableEvent* sync);
  void DrawFunctorOnRT(base::WaitableEvent* sync);
  void CheckCurrentlyOnRT();

  // const so can be used on both threads.
  BrowserViewRenderer* const view_;
  WindowHooks* const hooks_;
  const gfx::Size surface_size_;

  // UI thread members.
  gfx::Rect location_;
  bool on_draw_hardware_pending_;
  base::SequenceChecker ui_checker_;

  // Render thread members.
  std::unique_ptr<base::Thread> render_thread_;
  base::SequenceChecker rt_checker_;
  RenderThreadManager* functor_;
  scoped_refptr<base::SingleThreadTaskRunner> render_thread_loop_;
  scoped_refptr<gfx::GLSurface> surface_;
  scoped_refptr<gfx::GLContext> context_;
  bool context_current_;

  base::WeakPtrFactory<FakeWindow> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(FakeWindow);
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_TEST_FAKE_WINDOW_H_
