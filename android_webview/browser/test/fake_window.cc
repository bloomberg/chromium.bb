// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/test/fake_window.h"

#include "android_webview/browser/browser_view_renderer.h"
#include "android_webview/browser/child_frame.h"
#include "android_webview/browser/render_thread_manager.h"
#include "base/location.h"
#include "base/synchronization/waitable_event.h"
#include "base/thread_task_runner_handle.h"
#include "base/threading/thread.h"
#include "ui/gl/gl_bindings.h"

namespace android_webview {

class FakeWindow::ScopedMakeCurrent {
 public:
  ScopedMakeCurrent(FakeWindow* view_root) : view_root_(view_root) {
    DCHECK(!view_root_->context_current_);
    view_root_->context_current_ = true;
    bool result = view_root_->context_->MakeCurrent(view_root_->surface_.get());
    DCHECK(result);
  };

  ~ScopedMakeCurrent() {
    DCHECK(view_root_->context_current_);
    view_root_->context_current_ = false;

    // Release the underlying EGLContext. This is required because the real
    // GLContextEGL may no longer be current here and to satisfy DCHECK in
    // GLContextEGL::IsCurrent.
    eglMakeCurrent(view_root_->surface_->GetDisplay(), EGL_NO_SURFACE,
                   EGL_NO_SURFACE, EGL_NO_CONTEXT);
    view_root_->context_->ReleaseCurrent(view_root_->surface_.get());
  }

 private:
  FakeWindow* view_root_;
};

FakeWindow::FakeWindow(BrowserViewRenderer* view,
                       RenderThreadManager* functor,
                       WindowHooks* hooks,
                       gfx::Rect location)
    : view_(view),
      hooks_(hooks),
      surface_size_(100, 100),
      location_(location),
      on_draw_hardware_pending_(false),
      functor_(functor),
      context_current_(false),
      weak_ptr_factory_(this) {
  CheckCurrentlyOnUIThread();
  DCHECK(view_);
  view_->OnAttachedToWindow(location_.width(), location_.height());
  view_->SetWindowVisibility(true);
  view_->SetViewVisibility(true);
}

FakeWindow::~FakeWindow() {
  CheckCurrentlyOnUIThread();
}

void FakeWindow::Detach() {
  CheckCurrentlyOnUIThread();
  functor_->DeleteHardwareRendererOnUI();
  view_->OnDetachedFromWindow();

  if (render_thread_loop_) {
    base::WaitableEvent completion(true, false);
    render_thread_loop_->PostTask(
        FROM_HERE, base::Bind(&FakeWindow::DestroyOnRT, base::Unretained(this),
                              &completion));
    completion.Wait();
  }

  render_thread_.reset();
}

void FakeWindow::RequestInvokeGL(bool wait_for_completion) {
  CheckCurrentlyOnUIThread();
  base::WaitableEvent completion(true, false);
  render_thread_loop_->PostTask(
      FROM_HERE,
      base::Bind(&FakeWindow::ProcessFunctorOnRT, base::Unretained(this),
                 wait_for_completion ? &completion : nullptr));
  if (wait_for_completion)
    completion.Wait();
}

void FakeWindow::ProcessFunctorOnRT(base::WaitableEvent* sync) {
  CheckCurrentlyOnRT();
  AwDrawGLInfo process_info;
  process_info.version = kAwDrawGLInfoVersion;
  process_info.mode = AwDrawGLInfo::kModeProcess;

  hooks_->WillProcessOnRT(functor_);
  {
    ScopedMakeCurrent make_current(this);
    functor_->DrawGL(&process_info);
  }
  hooks_->DidProcessOnRT(functor_);

  if (sync)
    sync->Signal();
}

void FakeWindow::PostInvalidate() {
  CheckCurrentlyOnUIThread();
  if (on_draw_hardware_pending_)
    return;
  on_draw_hardware_pending_ = true;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::Bind(&FakeWindow::OnDrawHardware, weak_ptr_factory_.GetWeakPtr()));
}

void FakeWindow::OnDrawHardware() {
  CheckCurrentlyOnUIThread();
  DCHECK(on_draw_hardware_pending_);
  on_draw_hardware_pending_ = false;

  view_->PrepareToDraw(gfx::Vector2d(), location_);
  hooks_->WillOnDraw();
  bool success = view_->OnDrawHardware();
  hooks_->DidOnDraw(success);
  if (success) {
    CreateRenderThreadIfNeeded();

    base::WaitableEvent completion(true, false);
    render_thread_loop_->PostTask(
        FROM_HERE, base::Bind(&FakeWindow::DrawFunctorOnRT,
                              base::Unretained(this), &completion));
    completion.Wait();
  }
}

void FakeWindow::DrawFunctorOnRT(base::WaitableEvent* sync) {
  CheckCurrentlyOnRT();
  // Ok to access UI functions until sync is signalled.
  gfx::Rect location = location_;
  {
    AwDrawGLInfo process_info;
    process_info.version = kAwDrawGLInfoVersion;
    process_info.mode = AwDrawGLInfo::kModeSync;

    hooks_->WillSyncOnRT(functor_);
    functor_->DrawGL(&process_info);
    hooks_->DidSyncOnRT(functor_);
  }
  sync->Signal();

  AwDrawGLInfo draw_info;
  draw_info.version = kAwDrawGLInfoVersion;
  draw_info.mode = AwDrawGLInfo::kModeDraw;
  draw_info.clip_left = location.x();
  draw_info.clip_top = location.y();
  draw_info.clip_right = location.x() + location.width();
  draw_info.clip_bottom = location.y() + location.height();

  if (!hooks_->WillDrawOnRT(functor_, &draw_info))
    return;

  {
    ScopedMakeCurrent make_current(this);
    functor_->DrawGL(&draw_info);
  }
  hooks_->DidDrawOnRT(functor_);
}

void FakeWindow::CheckCurrentlyOnUIThread() {
  DCHECK(ui_checker_.CalledOnValidSequencedThread());
}

void FakeWindow::CreateRenderThreadIfNeeded() {
  CheckCurrentlyOnUIThread();
  if (render_thread_) {
    DCHECK(render_thread_loop_);
    return;
  }
  render_thread_.reset(new base::Thread("TestRenderThread"));
  render_thread_->Start();
  render_thread_loop_ = render_thread_->task_runner();
  rt_checker_.DetachFromSequence();

  base::WaitableEvent completion(true, false);
  render_thread_loop_->PostTask(
      FROM_HERE, base::Bind(&FakeWindow::InitializeOnRT, base::Unretained(this),
                            &completion));
  completion.Wait();
}

void FakeWindow::InitializeOnRT(base::WaitableEvent* sync) {
  CheckCurrentlyOnRT();
  surface_ = gfx::GLSurface::CreateOffscreenGLSurface(surface_size_);
  DCHECK(surface_);
  DCHECK(surface_->GetHandle());
  context_ = gfx::GLContext::CreateGLContext(nullptr, surface_.get(),
                                             gfx::PreferDiscreteGpu);
  DCHECK(context_);
  sync->Signal();
}

void FakeWindow::DestroyOnRT(base::WaitableEvent* sync) {
  CheckCurrentlyOnRT();
  if (context_) {
    DCHECK(!context_->IsCurrent(surface_.get()));
    context_ = nullptr;
    surface_ = nullptr;
  }
  sync->Signal();
}

void FakeWindow::CheckCurrentlyOnRT() {
  DCHECK(rt_checker_.CalledOnValidSequencedThread());
}

}  // namespace android_webview
