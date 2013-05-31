// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/in_process/synchronous_compositor_impl.h"

#include "base/lazy_instance.h"
#include "base/message_loop.h"
#include "content/public/browser/android/synchronous_compositor_client.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/renderer/android/synchronous_compositor_factory.h"

namespace content {

namespace {

int GetInProcessRendererId() {
  content::RenderProcessHost::iterator it =
      content::RenderProcessHost::AllHostsIterator();
  if (it.IsAtEnd()) {
    // There should always be one RPH in single process mode.
    NOTREACHED();
    return 0;
  }

  int id = it.GetCurrentValue()->GetID();
  it.Advance();
  DCHECK(it.IsAtEnd());  // Not multiprocess compatible.
  return id;
}

class SynchronousCompositorFactoryImpl : public SynchronousCompositorFactory {
 public:
  SynchronousCompositorFactoryImpl() {
    SynchronousCompositorFactory::SetInstance(this);
  }

  // SynchronousCompositorFactory
  virtual scoped_refptr<base::MessageLoopProxy>
      GetCompositorMessageLoop() OVERRIDE {
    return BrowserThread::GetMessageLoopProxyForThread(BrowserThread::UI);
  }

  virtual scoped_ptr<cc::OutputSurface> CreateOutputSurface(
      int routing_id) OVERRIDE {
    scoped_ptr<SynchronousCompositorOutputSurface> output_surface(
        new SynchronousCompositorOutputSurface(routing_id));
    return output_surface.PassAs<cc::OutputSurface>();
  }
};

base::LazyInstance<SynchronousCompositorFactoryImpl>::Leaky g_factory =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

DEFINE_WEB_CONTENTS_USER_DATA_KEY(SynchronousCompositorImpl);

// static
SynchronousCompositorImpl* SynchronousCompositorImpl::FromRoutingID(
    int routing_id) {
  RenderViewHost* rvh = RenderViewHost::FromID(GetInProcessRendererId(),
                                               routing_id);
  if (!rvh)
    return NULL;
  WebContents* contents = WebContents::FromRenderViewHost(rvh);
  if (!contents)
    return NULL;
  return FromWebContents(contents);
}

SynchronousCompositorImpl::SynchronousCompositorImpl(WebContents* contents)
    : compositor_client_(NULL),
      output_surface_(NULL),
      contents_(contents) {
}

SynchronousCompositorImpl::~SynchronousCompositorImpl() {
  if (compositor_client_)
    compositor_client_->DidDestroyCompositor(this);
}

bool SynchronousCompositorImpl::IsHwReady() {
  DCHECK(CalledOnValidThread());
  DCHECK(output_surface_);

  return output_surface_->IsHwReady();
}

void SynchronousCompositorImpl::SetClient(
    SynchronousCompositorClient* compositor_client) {
  DCHECK(CalledOnValidThread());
  compositor_client_ = compositor_client;
}

bool SynchronousCompositorImpl::DemandDrawSw(SkCanvas* canvas) {
  DCHECK(CalledOnValidThread());
  DCHECK(output_surface_);

  return output_surface_->DemandDrawSw(canvas);
}

bool SynchronousCompositorImpl::DemandDrawHw(
      gfx::Size view_size,
      const gfx::Transform& transform,
      gfx::Rect damage_area) {
  DCHECK(CalledOnValidThread());
  DCHECK(output_surface_);

  return output_surface_->DemandDrawHw(view_size, transform, damage_area);
}

void SynchronousCompositorImpl::DidBindOutputSurface(
      SynchronousCompositorOutputSurface* output_surface) {
  DCHECK(CalledOnValidThread());
  output_surface_ = output_surface;
  if (compositor_client_)
    compositor_client_->DidInitializeCompositor(this);
}

void SynchronousCompositorImpl::DidDestroySynchronousOutputSurface(
       SynchronousCompositorOutputSurface* output_surface) {
  DCHECK(CalledOnValidThread());
  // Allow for transient hand-over when two output surfaces may refer to
  // a single delegate.
  if (output_surface_ == output_surface) {
    output_surface_ = NULL;
    if (compositor_client_)
      compositor_client_->DidDestroyCompositor(this);
    compositor_client_ = NULL;
  }
}

void SynchronousCompositorImpl::SetContinuousInvalidate(bool enable) {
  DCHECK(CalledOnValidThread());
  if (compositor_client_)
    compositor_client_->SetContinuousInvalidate(enable);
}

// Not using base::NonThreadSafe as we want to enforce a more exacting threading
// requirement: SynchronousCompositorImpl() must only be used on the UI thread.
bool SynchronousCompositorImpl::CalledOnValidThread() const {
  return BrowserThread::CurrentlyOn(BrowserThread::UI);
}

// static
void SynchronousCompositor::SetClientForWebContents(
    WebContents* contents,
    SynchronousCompositorClient* client) {
  DCHECK(contents);
  if (client) {
    g_factory.Get();  // Ensure it's initialized.
    SynchronousCompositorImpl::CreateForWebContents(contents);
  }
  if (SynchronousCompositorImpl* instance =
      SynchronousCompositorImpl::FromWebContents(contents)) {
    instance->SetClient(client);
  }
}

}  // namespace content
