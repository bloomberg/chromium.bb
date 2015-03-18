// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu/image_transport_surface_fbo_mac.h"

#include "content/common/gpu/gpu_messages.h"
#include "content/common/gpu/image_transport_surface_calayer_mac.h"
#include "content/common/gpu/image_transport_surface_iosurface_mac.h"
#include "ui/base/cocoa/remote_layer_api.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_surface_osmesa.h"

namespace content {

ImageTransportSurfaceFBO::ImageTransportSurfaceFBO(
    GpuChannelManager* manager,
    GpuCommandBufferStub* stub,
    gfx::PluginWindowHandle handle)
    : backbuffer_suggested_allocation_(true),
      frontbuffer_suggested_allocation_(true),
      fbo_id_(0),
      texture_id_(0),
      depth_stencil_renderbuffer_id_(0),
      has_complete_framebuffer_(false),
      context_(NULL),
      scale_factor_(1.f),
      made_current_(false),
      is_swap_buffers_send_pending_(false) {
  if (ui::RemoteLayerAPISupported())
    storage_provider_.reset(new CALayerStorageProvider(this));
  else
    storage_provider_.reset(new IOSurfaceStorageProvider(this));
  helper_.reset(new ImageTransportHelper(this, manager, stub, handle));
}

ImageTransportSurfaceFBO::~ImageTransportSurfaceFBO() {
}

bool ImageTransportSurfaceFBO::Initialize() {
  // Only support IOSurfaces if the GL implementation is the native desktop GL.
  // IO surfaces will not work with, for example, OSMesa software renderer
  // GL contexts.
  if (gfx::GetGLImplementation() != gfx::kGLImplementationDesktopGL &&
      gfx::GetGLImplementation() != gfx::kGLImplementationAppleGL)
    return false;

  if (!helper_->Initialize())
    return false;

  helper_->stub()->AddDestructionObserver(this);
  return true;
}

void ImageTransportSurfaceFBO::Destroy() {
  DestroyFramebuffer();
}

bool ImageTransportSurfaceFBO::DeferDraws() {
  storage_provider_->WillWriteToBackbuffer();
  // We should not have a pending send when we are drawing the next frame.
  DCHECK(!is_swap_buffers_send_pending_);

  // The call to WillWriteToBackbuffer could potentially force a draw. Ensure
  // that any changes made to the context's state are restored.
  context_->RestoreStateIfDirtiedExternally();
  return false;
}

bool ImageTransportSurfaceFBO::IsOffscreen() {
  return false;
}

bool ImageTransportSurfaceFBO::OnMakeCurrent(gfx::GLContext* context) {
  context_ = context;

  if (made_current_)
    return true;

  AllocateOrResizeFramebuffer(gfx::Size(1, 1), 1.f);

  made_current_ = true;
  return true;
}

void ImageTransportSurfaceFBO::NotifyWasBound() {
  // Sometimes calling glBindFramebuffer doesn't seem to be enough to get
  // rendered contents to show up in the color attachment. It appears that doing
  // a glBegin/End pair with program 0 is enough to tickle the driver into
  // actually effecting the binding.
  // http://crbug.com/435786
  DCHECK(has_complete_framebuffer_);

  // We will restore the current program after the dummy glBegin/End pair.
  // Ensure that we will be able to restore this state before attempting to
  // change it.
  GLint old_program_signed = 0;
  glGetIntegerv(GL_CURRENT_PROGRAM, &old_program_signed);
  GLuint old_program = static_cast<GLuint>(old_program_signed);
  if (old_program && glIsProgram(old_program)) {
    // A deleted program cannot be re-bound.
    GLint delete_status = GL_FALSE;
    glGetProgramiv(old_program, GL_DELETE_STATUS, &delete_status);
    if (delete_status == GL_TRUE)
      return;
    // A program which has had the most recent link fail cannot be re-bound.
    GLint link_status = GL_FALSE;
    glGetProgramiv(old_program, GL_LINK_STATUS, &link_status);
    if (link_status != GL_TRUE)
      return;
  }

  // Issue the dummy call and then restore the state.
  glUseProgram(0);
  GLenum status = glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT);
  DCHECK(status == GL_FRAMEBUFFER_COMPLETE);
  glBegin(GL_TRIANGLES);
  glEnd();
  glUseProgram(old_program);
}

unsigned int ImageTransportSurfaceFBO::GetBackingFrameBufferObject() {
  return fbo_id_;
}

bool ImageTransportSurfaceFBO::SetBackbufferAllocation(bool allocation) {
  if (backbuffer_suggested_allocation_ == allocation)
    return true;
  backbuffer_suggested_allocation_ = allocation;
  AdjustBufferAllocation();
  if (!allocation)
    storage_provider_->DiscardBackbuffer();
  return true;
}

void ImageTransportSurfaceFBO::SetFrontbufferAllocation(bool allocation) {
  if (frontbuffer_suggested_allocation_ == allocation)
    return;
  frontbuffer_suggested_allocation_ = allocation;
  AdjustBufferAllocation();
}

void ImageTransportSurfaceFBO::AdjustBufferAllocation() {
  // On mac, the frontbuffer and backbuffer are the same buffer. The buffer is
  // free'd when both the browser and gpu processes have Unref'd the IOSurface.
  if (!backbuffer_suggested_allocation_ &&
      !frontbuffer_suggested_allocation_ &&
      has_complete_framebuffer_) {
    DestroyFramebuffer();
  } else if (backbuffer_suggested_allocation_ && !has_complete_framebuffer_) {
    AllocateOrResizeFramebuffer(pixel_size_, scale_factor_);
  }
}

bool ImageTransportSurfaceFBO::SwapBuffers() {
  DCHECK(backbuffer_suggested_allocation_);
  if (!frontbuffer_suggested_allocation_)
    return true;

  // It is the responsibility of the storage provider to send the swap IPC.
  is_swap_buffers_send_pending_ = true;
  storage_provider_->SwapBuffers(pixel_size_, scale_factor_);

  // The call to swapBuffers could potentially result in an immediate draw.
  // Ensure that any changes made to the context's state are restored.
  context_->RestoreStateIfDirtiedExternally();
  return true;
}

void ImageTransportSurfaceFBO::SendSwapBuffers(uint64 surface_handle,
                                               const gfx::Size pixel_size,
                                               float scale_factor) {
  GpuHostMsg_AcceleratedSurfaceBuffersSwapped_Params params;
  params.surface_handle = surface_handle;
  params.size = pixel_size;
  params.scale_factor = scale_factor;
  params.latency_info.swap(latency_info_);
  helper_->SendAcceleratedSurfaceBuffersSwapped(params);
  is_swap_buffers_send_pending_ = false;
}

void ImageTransportSurfaceFBO::SetRendererID(int renderer_id) {
  if (renderer_id)
    context_->share_group()->SetRendererID(renderer_id);
}

bool ImageTransportSurfaceFBO::PostSubBuffer(
    int x, int y, int width, int height) {
  // Mac does not support sub-buffer swaps.
  NOTREACHED();
  return false;
}

bool ImageTransportSurfaceFBO::SupportsPostSubBuffer() {
  return true;
}

gfx::Size ImageTransportSurfaceFBO::GetSize() {
  return pixel_size_;
}

void* ImageTransportSurfaceFBO::GetHandle() {
  return NULL;
}

void* ImageTransportSurfaceFBO::GetDisplay() {
  return NULL;
}

void ImageTransportSurfaceFBO::OnBufferPresented(
    const AcceleratedSurfaceMsg_BufferPresented_Params& params) {
  SetRendererID(params.renderer_id);
  storage_provider_->SwapBuffersAckedByBrowser(params.disable_throttling);
}

void ImageTransportSurfaceFBO::OnResize(gfx::Size pixel_size,
                                        float scale_factor) {
  TRACE_EVENT2("gpu", "ImageTransportSurfaceFBO::OnResize",
               "old_size", pixel_size_.ToString(),
               "new_size", pixel_size.ToString());
  // Caching |context_| from OnMakeCurrent. It should still be current.
  DCHECK(context_->IsCurrent(this));

  AllocateOrResizeFramebuffer(pixel_size, scale_factor);
}

void ImageTransportSurfaceFBO::SetLatencyInfo(
    const std::vector<ui::LatencyInfo>& latency_info) {
  for (size_t i = 0; i < latency_info.size(); i++)
    latency_info_.push_back(latency_info[i]);
}

void ImageTransportSurfaceFBO::WakeUpGpu() {
  NOTIMPLEMENTED();
}

void ImageTransportSurfaceFBO::OnWillDestroyStub() {
  helper_->stub()->RemoveDestructionObserver(this);
  Destroy();
}

void ImageTransportSurfaceFBO::DestroyFramebuffer() {
  // If we have resources to destroy, then make sure that we have a current
  // context which we can use to delete the resources.
  if (context_ || fbo_id_ || texture_id_ || depth_stencil_renderbuffer_id_) {
    DCHECK(gfx::GLContext::GetCurrent() == context_);
    DCHECK(context_->IsCurrent(this));
    DCHECK(CGLGetCurrentContext());
  }

  if (fbo_id_) {
    glDeleteFramebuffersEXT(1, &fbo_id_);
    fbo_id_ = 0;
  }

  if (texture_id_) {
    glDeleteTextures(1, &texture_id_);
    texture_id_ = 0;
  }

  if (depth_stencil_renderbuffer_id_) {
    glDeleteRenderbuffersEXT(1, &depth_stencil_renderbuffer_id_);
    depth_stencil_renderbuffer_id_ = 0;
  }

  storage_provider_->FreeColorBufferStorage();

  has_complete_framebuffer_ = false;
}

void ImageTransportSurfaceFBO::AllocateOrResizeFramebuffer(
    const gfx::Size& new_pixel_size, float new_scale_factor) {
  gfx::Size new_rounded_pixel_size =
      storage_provider_->GetRoundedSize(new_pixel_size);

  // Only recreate the surface's storage when the rounded up size has changed,
  // or the scale factor has changed.
  bool needs_new_storage =
      !has_complete_framebuffer_ ||
      new_rounded_pixel_size != rounded_pixel_size_ ||
      new_scale_factor != scale_factor_;

  // Save the new storage parameters.
  pixel_size_ = new_pixel_size;
  rounded_pixel_size_ = new_rounded_pixel_size;
  scale_factor_ = new_scale_factor;

  if (!needs_new_storage)
    return;

  TRACE_EVENT2("gpu", "ImageTransportSurfaceFBO::AllocateOrResizeFramebuffer",
               "width", new_rounded_pixel_size.width(),
               "height", new_rounded_pixel_size.height());

  // GL_TEXTURE_RECTANGLE_ARB is the best supported render target on
  // Mac OS X and is required for IOSurface interoperability.
  GLint previous_texture_id = 0;
  glGetIntegerv(GL_TEXTURE_BINDING_RECTANGLE_ARB, &previous_texture_id);

  // Free the old IO Surface first to reduce memory fragmentation.
  DestroyFramebuffer();

  glGenFramebuffersEXT(1, &fbo_id_);
  glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, fbo_id_);

  glGenTextures(1, &texture_id_);

  glBindTexture(GL_TEXTURE_RECTANGLE_ARB, texture_id_);
  glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_RECTANGLE_ARB,
                  GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_RECTANGLE_ARB,
                  GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

  glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT,
                            GL_COLOR_ATTACHMENT0_EXT,
                            GL_TEXTURE_RECTANGLE_ARB,
                            texture_id_,
                            0);

  // Search through the provided attributes; if the caller has
  // requested a stencil buffer, try to get one.

  int32 stencil_bits =
      helper_->stub()->GetRequestedAttribute(EGL_STENCIL_SIZE);
  if (stencil_bits > 0) {
    // Create and bind the stencil buffer
    bool has_packed_depth_stencil =
         GLSurface::ExtensionsContain(
             reinterpret_cast<const char *>(glGetString(GL_EXTENSIONS)),
                                            "GL_EXT_packed_depth_stencil");

    if (has_packed_depth_stencil) {
      glGenRenderbuffersEXT(1, &depth_stencil_renderbuffer_id_);
      glBindRenderbufferEXT(GL_RENDERBUFFER_EXT,
                            depth_stencil_renderbuffer_id_);
      glRenderbufferStorageEXT(GL_RENDERBUFFER_EXT, GL_DEPTH24_STENCIL8_EXT,
                              rounded_pixel_size_.width(),
                              rounded_pixel_size_.height());
      glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT,
                                  GL_STENCIL_ATTACHMENT_EXT,
                                  GL_RENDERBUFFER_EXT,
                                  depth_stencil_renderbuffer_id_);
    }

    // If we asked for stencil but the extension isn't present,
    // it's OK to silently fail; subsequent code will/must check
    // for the presence of a stencil buffer before attempting to
    // do stencil-based operations.
  }

  bool allocated_color_buffer = storage_provider_->AllocateColorBufferStorage(
      static_cast<CGLContextObj>(
          context_->GetHandle()),
          context_->GetStateWasDirtiedExternallyCallback(),
          texture_id_, rounded_pixel_size_, scale_factor_);
  if (!allocated_color_buffer) {
    DLOG(ERROR) << "Failed to allocate color buffer storage.";
    DestroyFramebuffer();
    return;
  }

  GLenum status = glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT);
  if (status != GL_FRAMEBUFFER_COMPLETE_EXT) {
    DLOG(ERROR) << "Framebuffer was incomplete: " << status;
    DestroyFramebuffer();
    return;
  }

  has_complete_framebuffer_ = true;

  glBindTexture(GL_TEXTURE_RECTANGLE_ARB, previous_texture_id);
  // The FBO remains bound for this GL context.
}

}  // namespace content
