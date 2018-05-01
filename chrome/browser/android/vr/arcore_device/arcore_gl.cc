// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/vr/arcore_device/arcore_gl.h"

#include <algorithm>
#include <chrono>
#include <limits>
#include <utility>
#include "base/android/android_hardware_buffer_compat.h"
#include "base/android/jni_android.h"
#include "base/callback_helpers.h"
#include "base/containers/queue.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/task_scheduler/post_task.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/trace_event_argument.h"
#include "chrome/browser/android/vr/arcore_device/arcore_device.h"
#include "chrome/browser/android/vr/arcore_device/fake_arcore.h"
#include "device/vr/public/mojom/vr_service.mojom.h"
#include "gpu/ipc/common/gpu_memory_buffer_impl_android_hardware_buffer.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/angle_conversions.h"
#include "ui/gfx/gpu_fence.h"
#include "ui/gl/android/scoped_java_surface.h"
#include "ui/gl/android/surface_texture.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_fence_egl.h"
#include "ui/gl/gl_image_ahardwarebuffer.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/init/gl_factory.h"

namespace device {

namespace {

// Input display coordinates (range 0..1) used with ARCore's
// transformDisplayUvCoords to calculate the output matrix.
constexpr float kDisplayCoordinatesForTransform[6] = {0.f, 0.f, 1.f,
                                                      0.f, 0.f, 1.f};

// Number of shared buffers to use in rotation. Two would be sufficient if
// strictly sequenced, but use an extra one since we currently don't know
// exactly when the Renderer is done with it.
constexpr int kSharedBufferSwapChainSize = 3;

}  // namespace

WebXrSharedBuffer::WebXrSharedBuffer() = default;
WebXrSharedBuffer::~WebXrSharedBuffer() = default;

WebXrSwapChain::WebXrSwapChain() = default;
WebXrSwapChain::~WebXrSwapChain() = default;

ARCoreGl::ARCoreGl(
    ARCoreDevice* arcore_device,
    std::unique_ptr<vr::MailboxToSurfaceBridge> mailbox_bridge,
    scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner)
    : arcore_device_(arcore_device),
      gl_thread_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      main_thread_task_runner_(main_thread_task_runner),
      webxr_fps_meter_(),
      weak_ptr_factory_(this) {
  arcore_interface_ = std::make_unique<FakeARCore>();
  mailbox_bridge_ = std::move(mailbox_bridge);
}

ARCoreGl::~ARCoreGl() {}

void ARCoreGl::Initialize() {
  InitializeGl();
}

void ARCoreGl::InitializeGl() {
  DCHECK(IsOnGlThread());

  mailbox_bridge_->BindContextProviderToCurrentThread();

  if (gl::GetGLImplementation() == gl::kGLImplementationNone &&
      !gl::init::InitializeGLOneOff()) {
    LOG(ERROR) << "gl::init::InitializeGLOneOff failed";
    return;
  }

  surface_ = gl::init::CreateOffscreenGLSurface(gfx::Size());

  if (!surface_.get()) {
    LOG(ERROR) << "gl::init::CreateOffscreenGLSurface failed";
    return;
  }
  context_ = gl::init::CreateGLContext(nullptr, surface_.get(),
                                       gl::GLContextAttribs());
  if (!context_.get()) {
    LOG(ERROR) << "gl::init::CreateGLContext failed";
    return;
  }
  if (!context_->MakeCurrent(surface_.get())) {
    LOG(ERROR) << "gl::GLContext::MakeCurrent() failed";
    return;
  }

  glDisable(GL_DEPTH_TEST);
  glDepthMask(GL_FALSE);
  web_vr_renderer_ = std::make_unique<vr::WebVrRenderer>();
  vr::BaseQuadRenderer::CreateBuffers();
  glGenTextures(1, &camera_texture_id_arcore_);

  // Set the texture on the device to render the camera.
  arcore_interface_->SetCameraTexture(camera_texture_id_arcore_);

  SetupHardwareBuffers();

  is_initialized_ = true;
}

void ARCoreGl::ResizeSharedBuffer(WebXrSharedBuffer* buffer,
                                  const gfx::Size& size) {
  DCHECK(IsOnGlThread());

  if (buffer->size == size)
    return;

  TRACE_EVENT0("gpu", __FUNCTION__);
  // Unbind previous image (if any).
  if (buffer->remote_image_id) {
    DVLOG(2) << ": UnbindSharedBuffer, remote_image="
             << buffer->remote_image_id;
    mailbox_bridge_->UnbindSharedBuffer(buffer->remote_image_id,
                                        buffer->remote_texture_id);
    buffer->remote_image_id = 0;
  }

  DVLOG(2) << __FUNCTION__ << ": width=" << size.width()
           << " height=" << size.height();
  // Remove reference to previous image (if any).
  buffer->local_glimage = nullptr;

  const gfx::BufferFormat format = gfx::BufferFormat::RGBA_8888;
  const gfx::BufferUsage usage = gfx::BufferUsage::SCANOUT;

  gfx::GpuMemoryBufferId kBufferId(swap_chain_.next_memory_buffer_id++);
  buffer->gmb = gpu::GpuMemoryBufferImplAndroidHardwareBuffer::Create(
      kBufferId, size, format, usage,
      gpu::GpuMemoryBufferImpl::DestructionCallback());

  buffer->remote_image_id = mailbox_bridge_->BindSharedBufferImage(
      buffer->gmb.get(), size, format, usage, buffer->remote_texture_id);
  DVLOG(2) << ": BindSharedBufferImage, remote_image="
           << buffer->remote_image_id;

  scoped_refptr<gl::GLImageAHardwareBuffer> img(
      new gl::GLImageAHardwareBuffer(size));

  AHardwareBuffer* ahb = buffer->gmb->GetHandle().android_hardware_buffer;
  bool ret = img->Initialize(ahb, false /* preserved */);
  if (!ret) {
    DLOG(WARNING) << __FUNCTION__ << ": ERROR: failed to initialize image!";
    return;
  }
  glBindTexture(GL_TEXTURE_EXTERNAL_OES, buffer->local_texture_id);
  img->BindTexImage(GL_TEXTURE_EXTERNAL_OES);
  buffer->local_glimage = std::move(img);

  // Save size to avoid resize next time.
  DVLOG(1) << __FUNCTION__ << ": resized to " << size.width() << "x"
           << size.height();
  buffer->size = size;
}

void ARCoreGl::SetupHardwareBuffers() {
  DCHECK(IsOnGlThread());

  glGenFramebuffersEXT(1, &camera_fbo_);

  for (int i = 0; i < kSharedBufferSwapChainSize; ++i) {
    std::unique_ptr<WebXrSharedBuffer> buffer =
        std::make_unique<WebXrSharedBuffer>();
    // Remote resources
    std::unique_ptr<gpu::MailboxHolder> holder =
        std::make_unique<gpu::MailboxHolder>();
    mailbox_bridge_->GenerateMailbox(holder->mailbox);
    holder->texture_target = GL_TEXTURE_2D;
    buffer->mailbox_holder = std::move(holder);

    buffer->remote_texture_id =
        mailbox_bridge_->CreateMailboxTexture(buffer->mailbox_holder->mailbox);

    // Local resources
    glGenTextures(1, &buffer->local_texture_id);

    // Add to swap chain
    swap_chain_.buffers.push(std::move(buffer));
  }

  glGenFramebuffersEXT(1, &transfer_fbo_);
}

void ARCoreGl::MakeUvTransformMatrixFromSampleData(float (&mat_out)[16],
                                                   const float (&uv_in)[6]) {
  // We're creating a matrix that transforms viewport UV coordinates (for a
  // screen-filling quad, origin at bottom left, u=1 at right, v=1 at top) to
  // camera texture UV coordinates. This matrix is used with
  // vr/renderers/web_vr_renderer to get the sampler coordinates for copying an
  // appropriately cropped and rotated subsection of the camera image. The
  // SampleData is a bit unfortunate. ARCore doesn't provide a way to get a
  // matrix directly. There's a function to transform UV vectors individually,
  // which obviously can't be used from a shader, so we run that on selected
  // vectors and recreate the matrix from the result.

  // Assumes that uv_in is the result of transforming the display coordinates
  // from kDisplayCoordinatesForTransform. This combines the solved matrix with
  // a Y flip because ARCore's "normalized screen space" coordinates have the
  // origin at the top left to match 2D Android APIs, so it needs a Y flip to
  // get an origin at bottom left as used for textures.
  float u00 = uv_in[0];
  float v00 = uv_in[1];
  float u10 = uv_in[2];
  float v10 = uv_in[3];
  float u01 = uv_in[4];
  float v01 = uv_in[5];
  mat_out[0] = u10 - u00;
  mat_out[1] = v10 - v00;
  mat_out[2] = 0.f;
  mat_out[3] = 0.f;
  mat_out[4] = -(u01 - u00);
  mat_out[5] = -(v01 - v00);
  mat_out[6] = 0.f;
  mat_out[7] = 0.f;
  mat_out[8] = 0.f;
  mat_out[9] = 0.f;
  mat_out[10] = 1.f;
  mat_out[11] = 0.f;
  mat_out[12] = u01;
  mat_out[13] = v01;
  mat_out[14] = 0.f;
  mat_out[15] = 1.f;
}

// TODO(lincolnfrog): Break this method up into smaller chunks.
void ARCoreGl::ProduceCameraFrame(
    const gfx::Size& frame_size,
    display::Display::Rotation display_rotation,
    mojom::VRMagicWindowProvider::GetFrameDataCallback callback) {
  TRACE_EVENT0("gpu", __FUNCTION__);
  DCHECK(IsOnGlThread());
  // TODO(klausw): find out when a buffer is actually done being used
  // including by GL so we can know if we are overwriting one.
  DCHECK(swap_chain_.buffers.size() > 0);

  // Set display geometry before calling Update. It's a pending request that
  // applies to the next frame.
  // TODO(klausw): Only call if there was a change, this may be an expensive
  // operation. If there was no change, the previous projection matrix and UV
  // transform remain valid.
  gfx::Size transfer_size = frame_size;
  arcore_interface_->SetDisplayGeometry(transfer_size, display_rotation);

  TRACE_EVENT_BEGIN0("gpu", "ARCore Update");
  mojom::VRPosePtr ar_pose = arcore_interface_->Update();
  TRACE_EVENT_END0("gpu", "ARCore Update");

  glBindFramebufferEXT(GL_DRAW_FRAMEBUFFER, transfer_fbo_);

  std::unique_ptr<WebXrSharedBuffer> shared_buffer =
      std::move(swap_chain_.buffers.front());
  swap_chain_.buffers.pop();
  ResizeSharedBuffer(shared_buffer.get(), transfer_size);

  glFramebufferTexture2DEXT(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                            GL_TEXTURE_EXTERNAL_OES,
                            shared_buffer->local_texture_id, 0);

  if (!transfer_fbo_completeness_checked_) {
    auto status = glCheckFramebufferStatusEXT(GL_DRAW_FRAMEBUFFER);
    DVLOG(1) << __FUNCTION__ << ": framebuffer status=" << std::hex << status;
    DCHECK(status == GL_FRAMEBUFFER_COMPLETE);
    transfer_fbo_completeness_checked_ = true;
  }

  // Don't need face culling, depth testing, blending, etc. Turn it all off.
  glDisable(GL_CULL_FACE);
  glDisable(GL_SCISSOR_TEST);
  glDisable(GL_BLEND);
  glDisable(GL_POLYGON_OFFSET_FILL);
  glViewport(0, 0, transfer_size.width(), transfer_size.height());

  // Get the UV transform matrix from ARCore's UV transform applied
  // to sample data. TODO(klausw): do this only on changes, not every
  // frame.
  float uv_out[6];
  arcore_interface_->TransformDisplayUvCoords(
      6, &kDisplayCoordinatesForTransform[0], &uv_out[0]);
  float uv_transform[16];
  MakeUvTransformMatrixFromSampleData(uv_transform, uv_out);

  // Draw the ARCore texture!
  web_vr_renderer_->Draw(camera_texture_id_arcore_, uv_transform, 0, 0);

  glBindFramebufferEXT(GL_DRAW_FRAMEBUFFER, 0);

  // Make a GpuFence and place it in the GPU stream for sequencing.
  std::unique_ptr<gl::GLFence> gl_fence = gl::GLFence::CreateForGpuFence();
  std::unique_ptr<gfx::GpuFence> gpu_fence = gl_fence->GetGpuFence();
  mailbox_bridge_->WaitForClientGpuFence(gpu_fence.get());
  mailbox_bridge_->GenSyncToken(&shared_buffer->mailbox_holder->sync_token);

  mojom::VRMagicWindowFrameDataPtr frame_data =
      mojom::VRMagicWindowFrameData::New();
  frame_data->pose = std::move(ar_pose);
  frame_data->buffer_holder = *shared_buffer->mailbox_holder;
  frame_data->buffer_size = transfer_size;
  frame_data->time_delta = base::TimeTicks::Now() - base::TimeTicks();
  frame_data->projection_matrix.resize(16);
  float projection_matrix[16];
  // We need near/far distances to make a projection matrix. The actual
  // values don't matter, the Renderer will recalculate dependent values
  // based on the application's near/far settngs.
  constexpr float depth_near = 0.1f;
  constexpr float depth_far = 1000.f;
  arcore_interface_->GetProjectionMatrix(projection_matrix, depth_near,
                                         depth_far);
  for (int i = 0; i < 16; i++) {
    frame_data->projection_matrix[i] = projection_matrix[i];
  }

  main_thread_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&ARCoreDevice::OnFrameData, arcore_device_->GetWeakPtr(),
                     base::Passed(&frame_data), base::Passed(&callback)));

  // Done with the shared buffer.
  swap_chain_.buffers.push(std::move(shared_buffer));

  webxr_fps_meter_.AddFrame(base::TimeTicks::Now());
  TRACE_COUNTER1("gpu", "WebXR FPS", webxr_fps_meter_.GetFPS());
}

void ARCoreGl::RequestFrame(
    const gfx::Size& frame_size,
    display::Display::Rotation display_rotation,
    mojom::VRMagicWindowProvider::GetFrameDataCallback callback) {
  // Ensure we have fully initialized before running a frame.
  DCHECK(IsInitialized());
  DCHECK(!IsOnGlThread());
  gl_thread_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&ARCoreGl::ProduceCameraFrame, GetWeakPtr(), frame_size,
                     display_rotation, base::Passed(&callback)));
}

bool ARCoreGl::IsOnGlThread() const {
  return gl_thread_task_runner_->BelongsToCurrentThread();
}

base::WeakPtr<ARCoreGl> ARCoreGl::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace device
