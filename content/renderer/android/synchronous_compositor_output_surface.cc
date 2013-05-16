// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/android/synchronous_compositor_output_surface.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "base/time.h"
#include "cc/output/compositor_frame.h"
#include "cc/output/compositor_frame_ack.h"
#include "cc/output/output_surface_client.h"
#include "cc/output/software_output_device.h"
#include "content/common/gpu/client/webgraphicscontext3d_command_buffer_impl.h"
#include "content/public/common/content_switches.h"
#include "content/public/renderer/android/synchronous_compositor_client.h"
#include "content/public/renderer/content_renderer_client.h"
#include "skia/ext/refptr.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkDevice.h"
#include "third_party/skia/include/core/SkPicture.h"
#include "ui/gfx/rect_conversions.h"
#include "ui/gfx/skia_util.h"
#include "ui/gfx/transform.h"
#include "webkit/gpu/webgraphicscontext3d_in_process_command_buffer_impl.h"

using webkit::gpu::WebGraphicsContext3DInProcessCommandBufferImpl;

namespace content {

namespace {

// TODO(boliu): RenderThreadImpl should create in process contexts as well.
scoped_ptr<WebKit::WebGraphicsContext3D> CreateWebGraphicsContext3D() {
  if (!CommandLine::ForCurrentProcess()->HasSwitch("testing-webview-gl-mode"))
    return scoped_ptr<WebKit::WebGraphicsContext3D>();

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
    DCHECK(surface_->current_sw_canvas_);
    if (surface_->current_sw_canvas_)
      return surface_->current_sw_canvas_;
    return &null_canvas_;
  }
  virtual void EndPaint(cc::SoftwareFrameData* frame_data) OVERRIDE {
    surface_->current_sw_canvas_ = NULL;
  }
  virtual void CopyToBitmap(gfx::Rect rect, SkBitmap* output) OVERRIDE {
    NOTIMPLEMENTED();
  }
  virtual void Scroll(gfx::Vector2d delta,
                      gfx::Rect clip_rect) OVERRIDE {
    NOTIMPLEMENTED();
  }
  virtual void ReclaimDIB(const TransportDIB::Id& id) OVERRIDE {
    NOTIMPLEMENTED();
  }

 private:
  SynchronousCompositorOutputSurface* surface_;
  SkDevice null_device_;
  SkCanvas null_canvas_;

  DISALLOW_COPY_AND_ASSIGN(SoftwareDevice);
};

SynchronousCompositorOutputSurface::SynchronousCompositorOutputSurface(
    int32 routing_id)
    : cc::OutputSurface(CreateWebGraphicsContext3D(),
                        scoped_ptr<cc::SoftwareOutputDevice>(
                            new SoftwareDevice(this))),
      compositor_client_(NULL),
      routing_id_(routing_id),
      vsync_enabled_(false),
      did_swap_buffer_(false),
      current_sw_canvas_(NULL) {
}

SynchronousCompositorOutputSurface::~SynchronousCompositorOutputSurface() {
  DCHECK(CalledOnValidThread());
  if (compositor_client_)
    compositor_client_->DidDestroyCompositor(this);
}

bool SynchronousCompositorOutputSurface::ForcedDrawToSoftwareDevice() const {
  return current_sw_canvas_ != NULL;
}

bool SynchronousCompositorOutputSurface::BindToClient(
    cc::OutputSurfaceClient* surface_client) {
  DCHECK(CalledOnValidThread());
  if (!cc::OutputSurface::BindToClient(surface_client))
    return false;
  GetContentClient()->renderer()->DidCreateSynchronousCompositor(routing_id_,
                                                                 this);
  return true;
}

void SynchronousCompositorOutputSurface::Reshape(gfx::Size size) {
  // Intentional no-op: surface size is controlled by the embedder.
}

void SynchronousCompositorOutputSurface::SendFrameToParentCompositor(
    cc::CompositorFrame* frame) {
  NOTREACHED();
  // TODO(joth): Route page scale to the client, see http://crbug.com/237006
}

void SynchronousCompositorOutputSurface::EnableVSyncNotification(
    bool enable_vsync) {
  DCHECK(CalledOnValidThread());
  vsync_enabled_ = enable_vsync;
  UpdateCompositorClientSettings();
}

void SynchronousCompositorOutputSurface::SwapBuffers(
    const cc::LatencyInfo& info) {
  context3d()->finish();
  did_swap_buffer_ = true;
}

void SynchronousCompositorOutputSurface::SetClient(
    SynchronousCompositorClient* compositor_client) {
  DCHECK(CalledOnValidThread());
  compositor_client_ = compositor_client;
  UpdateCompositorClientSettings();
}

bool SynchronousCompositorOutputSurface::IsHwReady() {
  return context3d() != NULL;
}

bool SynchronousCompositorOutputSurface::DemandDrawSw(SkCanvas* canvas) {
  DCHECK(CalledOnValidThread());
  DCHECK(canvas);
  DCHECK(!current_sw_canvas_);
  current_sw_canvas_ = canvas;

  SkRect canvas_clip;
  gfx::Rect damage_area;
  if (canvas->getClipBounds(&canvas_clip)) {
    damage_area = gfx::ToEnclosedRect(gfx::SkRectToRectF(canvas_clip));
  } else {
    damage_area = gfx::Rect(kint16max, kint16max);
  }

  gfx::Transform transform;
  transform.matrix() = canvas->getTotalMatrix();  // Converts 3x3 matrix to 4x4.

  InvokeComposite(transform, damage_area);

  bool finished_draw = current_sw_canvas_ == NULL;
  current_sw_canvas_ = NULL;
  return finished_draw;
}

bool SynchronousCompositorOutputSurface::DemandDrawHw(
      gfx::Size view_size,
      const gfx::Transform& transform,
      gfx::Rect damage_area) {
  DCHECK(CalledOnValidThread());
  DCHECK(client_);

  did_swap_buffer_ = false;

  InvokeComposite(transform, damage_area);

  return did_swap_buffer_;
}

void SynchronousCompositorOutputSurface::InvokeComposite(
    const gfx::Transform& transform,
    gfx::Rect damage_area) {
  // TODO(boliu): This assumes |transform| is identity and |damage_area| is the
  // whole view. Tracking bug to implement this: crbug.com/230463.
  client_->SetNeedsRedrawRect(damage_area);
  if (vsync_enabled_)
    client_->DidVSync(base::TimeTicks::Now());
}

void SynchronousCompositorOutputSurface::UpdateCompositorClientSettings() {
  if (compositor_client_) {
    compositor_client_->SetContinuousInvalidate(vsync_enabled_);
  }
}

// Not using base::NonThreadSafe as we want to enforce a more exacting threading
// requirement: SynchronousCompositorOutputSurface() must only be used by
// embedders that supply their own compositor loop via
// OverrideCompositorMessageLoop().
bool SynchronousCompositorOutputSurface::CalledOnValidThread() const {
  return base::MessageLoop::current() && (base::MessageLoop::current() ==
      GetContentClient()->renderer()->OverrideCompositorMessageLoop());
}

}  // namespace content
