// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/in_process/synchronous_compositor_output_surface.h"

#include "base/auto_reset.h"
#include "base/logging.h"
#include "cc/output/begin_frame_args.h"
#include "cc/output/compositor_frame.h"
#include "cc/output/context_provider.h"
#include "cc/output/output_surface_client.h"
#include "cc/output/software_output_device.h"
#include "content/browser/android/in_process/synchronous_compositor_impl.h"
#include "content/common/gpu/client/webgraphicscontext3d_command_buffer_impl.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkDevice.h"
#include "ui/gfx/rect_conversions.h"
#include "ui/gfx/skia_util.h"
#include "ui/gfx/transform.h"
#include "ui/gl/gl_context.h"
#include "webkit/common/gpu/webgraphicscontext3d_in_process_command_buffer_impl.h"

using webkit::gpu::WebGraphicsContext3DInProcessCommandBufferImpl;

namespace content {

namespace {

// TODO(boliu): RenderThreadImpl should create in process contexts as well.
scoped_ptr<WebKit::WebGraphicsContext3D> CreateWebGraphicsContext3D() {
  WebKit::WebGraphicsContext3D::Attributes attributes;
  attributes.antialias = false;
  attributes.shareResources = true;
  attributes.noAutomaticFlushes = true;

  return scoped_ptr<WebKit::WebGraphicsContext3D>(
      WebGraphicsContext3DInProcessCommandBufferImpl
          ::CreateViewContext(attributes, NULL));
}

} // namespace

class SynchronousCompositorOutputSurface::SoftwareDevice
  : public cc::SoftwareOutputDevice {
 public:
  SoftwareDevice(SynchronousCompositorOutputSurface* surface)
    : surface_(surface),
      null_device_(SkBitmap::kARGB_8888_Config, 1, 1),
      null_canvas_(&null_device_) {
  }
  virtual void Resize(gfx::Size size) OVERRIDE {
    // Intentional no-op: canvas size is controlled by the embedder.
  }
  virtual SkCanvas* BeginPaint(gfx::Rect damage_rect) OVERRIDE {
    if (!surface_->current_sw_canvas_) {
      NOTREACHED() << "BeginPaint with no canvas set";
      return &null_canvas_;
    }
    LOG_IF(WARNING, surface_->did_swap_buffer_)
        << "Mutliple calls to BeginPaint per frame";
    return surface_->current_sw_canvas_;
  }
  virtual void EndPaint(cc::SoftwareFrameData* frame_data) OVERRIDE {
  }
  virtual void CopyToBitmap(gfx::Rect rect, SkBitmap* output) OVERRIDE {
    NOTIMPLEMENTED();
  }

 private:
  SynchronousCompositorOutputSurface* surface_;
  SkDevice null_device_;
  SkCanvas null_canvas_;

  DISALLOW_COPY_AND_ASSIGN(SoftwareDevice);
};

SynchronousCompositorOutputSurface::SynchronousCompositorOutputSurface(
    int routing_id)
    : cc::OutputSurface(
          scoped_ptr<cc::SoftwareOutputDevice>(new SoftwareDevice(this))),
      routing_id_(routing_id),
      needs_begin_frame_(false),
      invoking_composite_(false),
      did_swap_buffer_(false),
      current_sw_canvas_(NULL) {
  capabilities_.deferred_gl_initialization = true;
  capabilities_.adjust_deadline_for_parent = false;
  // Cannot call out to GetDelegate() here as the output surface is not
  // constructed on the correct thread.
}

SynchronousCompositorOutputSurface::~SynchronousCompositorOutputSurface() {
  DCHECK(CalledOnValidThread());
  SynchronousCompositorOutputSurfaceDelegate* delegate = GetDelegate();
  if (delegate)
    delegate->DidDestroySynchronousOutputSurface(this);
}

bool SynchronousCompositorOutputSurface::ForcedDrawToSoftwareDevice() const {
  // |current_sw_canvas_| indicates we're in a DemandDrawSw call. In addition
  // |invoking_composite_| == false indicates an attempt to draw outside of
  // the synchronous compositor's control: force it into SW path and hence to
  // the null canvas (and will log a warning there).
  return current_sw_canvas_ != NULL || !invoking_composite_;
}

bool SynchronousCompositorOutputSurface::BindToClient(
    cc::OutputSurfaceClient* surface_client) {
  DCHECK(CalledOnValidThread());
  if (!cc::OutputSurface::BindToClient(surface_client))
    return false;
  SynchronousCompositorOutputSurfaceDelegate* delegate = GetDelegate();
  if (delegate)
    delegate->DidBindOutputSurface(this);
  return true;
}

void SynchronousCompositorOutputSurface::Reshape(
    gfx::Size size, float scale_factor) {
  // Intentional no-op: surface size is controlled by the embedder.
}

void SynchronousCompositorOutputSurface::SetNeedsBeginFrame(
    bool enable) {
  DCHECK(CalledOnValidThread());
  cc::OutputSurface::SetNeedsBeginFrame(enable);
  needs_begin_frame_ = enable;
  SynchronousCompositorOutputSurfaceDelegate* delegate = GetDelegate();
  if (delegate)
    delegate->SetContinuousInvalidate(needs_begin_frame_);
}

void SynchronousCompositorOutputSurface::SwapBuffers(
    cc::CompositorFrame* frame) {
  if (!ForcedDrawToSoftwareDevice()) {
    DCHECK(context3d());
    context3d()->shallowFlushCHROMIUM();
  }
  SynchronousCompositorOutputSurfaceDelegate* delegate = GetDelegate();
  if (delegate)
    delegate->UpdateFrameMetaData(frame->metadata);

  did_swap_buffer_ = true;
  DidSwapBuffers();
}

namespace {
void AdjustTransformForClip(gfx::Transform* transform, gfx::Rect clip) {
  // The system-provided transform translates us from the screen origin to the
  // origin of the clip rect, but CC's draw origin starts at the clip.
  transform->matrix().postTranslate(-clip.x(), -clip.y(), 0);
}
} // namespace

bool SynchronousCompositorOutputSurface::InitializeHwDraw() {
  DCHECK(CalledOnValidThread());
  DCHECK(HasClient());
  DCHECK(!context3d_);

  // TODO(boliu): Get a context provider in constructor and pass here.
  return InitializeAndSetContext3D(CreateWebGraphicsContext3D().Pass(),
                                   scoped_refptr<cc::ContextProvider>());
}

bool SynchronousCompositorOutputSurface::DemandDrawHw(
    gfx::Size surface_size,
    const gfx::Transform& transform,
    gfx::Rect clip) {
  DCHECK(CalledOnValidThread());
  DCHECK(HasClient());
  DCHECK(context3d());

  // Force a GL state restore next time a GLContextVirtual is made current.
  // TODO(boliu): Move this to the end of this function after we have fixed
  // all cases of MakeCurrent calls outside of draws. Tracked in
  // crbug.com/239856.
  gfx::GLContext* current_context = gfx::GLContext::GetCurrent();
  if (current_context)
    current_context->ReleaseCurrent(NULL);

  gfx::Transform adjusted_transform = transform;
  AdjustTransformForClip(&adjusted_transform, clip);
  surface_size_ = surface_size;
  SetExternalDrawConstraints(adjusted_transform, clip);
  InvokeComposite(clip.size());

  // TODO(boliu): Check if context is lost here.

  return did_swap_buffer_;
}

bool SynchronousCompositorOutputSurface::DemandDrawSw(SkCanvas* canvas) {
  DCHECK(CalledOnValidThread());
  DCHECK(canvas);
  DCHECK(!current_sw_canvas_);
  base::AutoReset<SkCanvas*> canvas_resetter(&current_sw_canvas_, canvas);

  SkIRect canvas_clip;
  canvas->getClipDeviceBounds(&canvas_clip);
  gfx::Rect clip = gfx::SkIRectToRect(canvas_clip);

  gfx::Transform transform(gfx::Transform::kSkipInitialization);
  transform.matrix() = canvas->getTotalMatrix();  // Converts 3x3 matrix to 4x4.
  AdjustTransformForClip(&transform, clip);

  surface_size_ = gfx::Size(canvas->getDeviceSize().width(),
                            canvas->getDeviceSize().height());
  SetExternalDrawConstraints(transform, clip);

  InvokeComposite(clip.size());

  return did_swap_buffer_;
}

void SynchronousCompositorOutputSurface::InvokeComposite(
    gfx::Size damage_size) {
  DCHECK(!invoking_composite_);
  base::AutoReset<bool> invoking_composite_resetter(&invoking_composite_, true);
  did_swap_buffer_ = false;
  SetNeedsRedrawRect(gfx::Rect(damage_size));
  if (needs_begin_frame_)
    BeginFrame(cc::BeginFrameArgs::CreateForSynchronousCompositor());

  if (did_swap_buffer_)
    OnSwapBuffersComplete(NULL);
}

void SynchronousCompositorOutputSurface::PostCheckForRetroactiveBeginFrame() {
  // Synchronous compositor cannot perform retroactive begin frames, so
  // intentionally no-op here.
}

// Not using base::NonThreadSafe as we want to enforce a more exacting threading
// requirement: SynchronousCompositorOutputSurface() must only be used on the UI
// thread.
bool SynchronousCompositorOutputSurface::CalledOnValidThread() const {
  return BrowserThread::CurrentlyOn(BrowserThread::UI);
}

SynchronousCompositorOutputSurfaceDelegate*
SynchronousCompositorOutputSurface::GetDelegate() {
  return SynchronousCompositorImpl::FromRoutingID(routing_id_);
}

}  // namespace content
