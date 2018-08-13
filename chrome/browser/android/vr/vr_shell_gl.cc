// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/vr/vr_shell_gl.h"

#include <limits>
#include <string>

#include "base/android/android_hardware_buffer_compat.h"
#include "base/android/jni_android.h"
#include "base/bind_helpers.h"
#include "base/callback_helpers.h"
#include "base/containers/queue.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_macros.h"
#include "base/optional.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/post_task.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event_argument.h"
#include "chrome/browser/android/vr/gl_browser_interface.h"
#include "chrome/browser/android/vr/gvr_controller_delegate.h"
#include "chrome/browser/android/vr/gvr_util.h"
#include "chrome/browser/android/vr/mailbox_to_surface_bridge.h"
#include "chrome/browser/android/vr/metrics_util_android.h"
#include "chrome/browser/android/vr/scoped_gpu_trace.h"
#include "chrome/browser/android/vr/vr_controller.h"
#include "chrome/browser/android/vr/vr_shell.h"
#include "chrome/browser/vr/assets_loader.h"
#include "chrome/browser/vr/gl_texture_location.h"
#include "chrome/browser/vr/graphics_delegate.h"
#include "chrome/browser/vr/metrics/session_metrics_helper.h"
#include "chrome/browser/vr/model/assets.h"
#include "chrome/browser/vr/model/camera_model.h"
#include "chrome/browser/vr/pose_util.h"
#include "chrome/browser/vr/ui_interface.h"
#include "chrome/browser/vr/ui_test_input.h"
#include "chrome/browser/vr/vr_geometry_util.h"
#include "chrome/browser/vr/vr_gl_util.h"
#include "chrome/common/chrome_features.h"
#include "content/public/common/content_features.h"
#include "device/vr/android/gvr/gvr_delegate.h"
#include "device/vr/android/gvr/gvr_device.h"
#include "device/vr/android/gvr/gvr_gamepad_data_provider.h"
#include "gpu/config/gpu_driver_bug_workaround_type.h"
#include "gpu/ipc/common/gpu_memory_buffer_impl_android_hardware_buffer.h"
#include "ui/gfx/geometry/angle_conversions.h"
#include "ui/gfx/gpu_fence.h"
#include "ui/gl/android/scoped_java_surface.h"
#include "ui/gl/android/surface_texture.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_fence_android_native_fence_sync.h"
#include "ui/gl/gl_fence_egl.h"
#include "ui/gl/gl_image_ahardwarebuffer.h"
#include "ui/gl/gl_share_group.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/init/gl_factory.h"

namespace vr {

namespace {
constexpr float kZNear = 0.1f;
constexpr float kZFar = 10000.0f;

// GVR buffer indices for use with viewport->SetSourceBufferIndex
// or frame.BindBuffer. We use one for multisampled contents (Browser UI), and
// one for non-multisampled content (webVR or quad layer).
constexpr int kMultiSampleBuffer = 0;
constexpr int kNoMultiSampleBuffer = 1;

constexpr float kDefaultRenderTargetSizeScale = 0.75f;
constexpr float kLowDpiDefaultRenderTargetSizeScale = 0.9f;

// When display UI on top of WebVR, we use a seperate buffer. Normally, the
// buffer is set to recommended size to get best visual (i.e the buffer for
// rendering ChromeVR). We divide the recommended buffer size by this number to
// improve performance.
// We calculate a smaller FOV and UV per frame which includes all visible
// elements. This allows us rendering UI at the same quality with a smaller
// buffer.
// Use 2 for now, we can probably make the buffer even smaller.
constexpr float kWebVrBrowserUiSizeFactor = 2.f;

// Number of frames to use for sliding averages for pose timings,
// as used for estimating prediction times.
constexpr unsigned kWebVRSlidingAverageSize = 5;

// Timeout for checking for the WebVR rendering GL fence. If the timeout is
// reached, yield to let other tasks execute before rechecking.
constexpr base::TimeDelta kWebVRFenceCheckTimeout =
    base::TimeDelta::FromMicroseconds(2000);

// Polling interval for checking for the WebVR rendering GL fence. Used as
// an alternative to kWebVRFenceCheckTimeout if the GPU workaround is active.
// The actual interval may be longer due to PostDelayedTask's resolution.
constexpr base::TimeDelta kWebVRFenceCheckPollInterval =
    base::TimeDelta::FromMicroseconds(500);

constexpr int kWebVrInitialFrameTimeoutSeconds = 5;
constexpr int kWebVrSpinnerTimeoutSeconds = 2;

// Heuristic time limit to detect overstuffed GVR buffers for a
// >60fps capable web app.
constexpr base::TimeDelta kWebVrSlowAcquireThreshold =
    base::TimeDelta::FromMilliseconds(2);

// If running too fast, allow dropping frames occasionally to let GVR catch up.
// Drop at most one frame in MaxDropRate.
constexpr int kWebVrUnstuffMaxDropRate = 7;

// Taken from the GVR source code, this is the default vignette border fraction.
constexpr float kContentVignetteBorder = 0.04;
constexpr float kContentVignetteScale = 1.0 + (kContentVignetteBorder * 2.0);
constexpr gvr::Rectf kContentUv = {0, 1.0, 0, 1.0};

// If we're not using the SurfaceTexture, use this matrix instead of
// webvr_surface_texture_uv_transform_ for drawing to GVR.
constexpr float kWebVrIdentityUvTransform[16] = {1, 0, 0, 0, 0, 1, 0, 0,
                                                 0, 0, 1, 0, 0, 0, 0, 1};

// TODO(mthiesse, https://crbug.com/834985): We should be reading this transform
// from the surface, but it appears to be wrong for a few frames and the content
// window ends up upside-down for a few frames...
constexpr float kContentUvTransform[16] = {1, 0, 0, 0, 0, -1, 0, 0,
                                           0, 0, 1, 0, 0, 1,  0, 1};

gfx::Transform PerspectiveMatrixFromView(const gvr::Rectf& fov,
                                         float z_near,
                                         float z_far) {
  gfx::Transform result;
  const float x_left = -std::tan(gfx::DegToRad(fov.left)) * z_near;
  const float x_right = std::tan(gfx::DegToRad(fov.right)) * z_near;
  const float y_bottom = -std::tan(gfx::DegToRad(fov.bottom)) * z_near;
  const float y_top = std::tan(gfx::DegToRad(fov.top)) * z_near;

  DCHECK(x_left < x_right && y_bottom < y_top && z_near < z_far &&
         z_near > 0.0f && z_far > 0.0f);
  const float X = (2 * z_near) / (x_right - x_left);
  const float Y = (2 * z_near) / (y_top - y_bottom);
  const float A = (x_right + x_left) / (x_right - x_left);
  const float B = (y_top + y_bottom) / (y_top - y_bottom);
  const float C = (z_near + z_far) / (z_near - z_far);
  const float D = (2 * z_near * z_far) / (z_near - z_far);

  // The gfx::Transform default ctor initializes the transform to the identity,
  // so we must zero out a few values along the diagonal here.
  result.matrix().set(0, 0, X);
  result.matrix().set(0, 2, A);
  result.matrix().set(1, 1, Y);
  result.matrix().set(1, 2, B);
  result.matrix().set(2, 2, C);
  result.matrix().set(2, 3, D);
  result.matrix().set(3, 2, -1);
  result.matrix().set(3, 3, 0);

  return result;
}

gvr::Rectf UVFromGfxRect(gfx::RectF rect) {
  return {rect.x(), rect.x() + rect.width(), 1.0f - rect.bottom(),
          1.0f - rect.y()};
}

gfx::RectF GfxRectFromUV(gvr::Rectf rect) {
  return gfx::RectF(rect.left, 1.0 - rect.top, rect.right - rect.left,
                    rect.top - rect.bottom);
}

gfx::RectF ClampRect(gfx::RectF bounds) {
  bounds.AdjustToFit(gfx::RectF(0, 0, 1, 1));
  return bounds;
}

gvr::Rectf ToGvrRectf(const UiInterface::FovRectangle& rect) {
  return gvr::Rectf{rect.left, rect.right, rect.bottom, rect.top};
}

UiInterface::FovRectangle ToUiFovRect(const gvr::Rectf& rect) {
  return UiInterface::FovRectangle{rect.left, rect.right, rect.bottom,
                                   rect.top};
}

}  // namespace

VrShellGl::VrShellGl(GlBrowserInterface* browser,
                     std::unique_ptr<UiInterface> ui,
                     gvr_context* gvr_api,
                     bool reprojected_rendering,
                     bool daydream_support,
                     bool start_in_web_vr_mode,
                     bool pause_content,
                     bool low_density)
    : RenderLoop(std::move(ui), browser, kWebVRSlidingAverageSize),
      webvr_vsync_align_(
          base::FeatureList::IsEnabled(features::kWebVrVsyncAlign)),
      low_density_(low_density),
      web_vr_mode_(start_in_web_vr_mode),
      surfaceless_rendering_(reprojected_rendering),
      daydream_support_(daydream_support),
      content_paused_(pause_content),
      task_runner_(base::ThreadTaskRunnerHandle::Get()),
      presentation_binding_(this),
      frame_data_binding_(this),
      browser_(browser),
      vr_ui_fps_meter_(),
      webvr_fps_meter_(),
      webvr_js_time_(kWebVRSlidingAverageSize),
      webvr_render_time_(kWebVRSlidingAverageSize),
      webvr_js_wait_time_(kWebVRSlidingAverageSize),
      webvr_acquire_time_(kWebVRSlidingAverageSize),
      webvr_submit_time_(kWebVRSlidingAverageSize),
      weak_ptr_factory_(this) {
  GvrInit(gvr_api);
  set_controller_delegate(std::make_unique<GvrControllerDelegate>(
      std::make_unique<VrController>(gvr_api), browser_));
}

VrShellGl::~VrShellGl() {
  ClosePresentationBindings();
  webxr_.EndPresentation();
}

void VrShellGl::Initialize(
    base::WaitableEvent* gl_surface_created_event,
    base::OnceCallback<gfx::AcceleratedWidget()> callback) {
  if (surfaceless_rendering_) {
    InitializeGl(nullptr);
  } else {
    gl_surface_created_event->Wait();
    InitializeGl(std::move(callback).Run());
  }
}

void VrShellGl::InitializeGl(gfx::AcceleratedWidget window) {
  if (gl::GetGLImplementation() == gl::kGLImplementationNone &&
      !gl::init::InitializeGLOneOff()) {
    LOG(ERROR) << "gl::init::InitializeGLOneOff failed";
    ForceExitVr();
    return;
  }
  if (window) {
    DCHECK(!surfaceless_rendering_);
    surface_ = gl::init::CreateViewGLSurface(window);
  } else {
    DCHECK(surfaceless_rendering_);
    surface_ = gl::init::CreateOffscreenGLSurface(gfx::Size());
  }
  if (!surface_.get()) {
    LOG(ERROR) << "gl::init::CreateOffscreenGLSurface failed";
    ForceExitVr();
    return;
  }

  graphics_delegate_ = std::make_unique<GraphicsDelegate>(surface_);

  if (!graphics_delegate_->Initialize()) {
    ForceExitVr();
    return;
  }

  glDisable(GL_DEPTH_TEST);
  glDepthMask(GL_FALSE);

  unsigned int textures[4];
  glGenTextures(4, textures);
  webvr_texture_id_ = textures[0];
  content_texture_id_ = textures[1];
  content_overlay_texture_id_ = textures[2];
  unsigned int ui_texture_id = textures[3];

  content_surface_texture_ = gl::SurfaceTexture::Create(content_texture_id_);
  content_overlay_surface_texture_ =
      gl::SurfaceTexture::Create(content_overlay_texture_id_);
  ui_surface_texture_ = gl::SurfaceTexture::Create(ui_texture_id);

  content_surface_ =
      std::make_unique<gl::ScopedJavaSurface>(content_surface_texture_.get());
  browser_->ContentSurfaceCreated(content_surface_->j_surface().obj(),
                                  content_surface_texture_.get());
  content_overlay_surface_ = std::make_unique<gl::ScopedJavaSurface>(
      content_overlay_surface_texture_.get());
  browser_->ContentOverlaySurfaceCreated(
      content_overlay_surface_->j_surface().obj(),
      content_overlay_surface_texture_.get());

  ui_surface_ =
      std::make_unique<gl::ScopedJavaSurface>(ui_surface_texture_.get());
  browser_->DialogSurfaceCreated(ui_surface_->j_surface().obj(),
                                 ui_surface_texture_.get());

  content_surface_texture_->SetFrameAvailableCallback(base::BindRepeating(
      &VrShellGl::OnContentFrameAvailable, weak_ptr_factory_.GetWeakPtr()));
  content_overlay_surface_texture_->SetFrameAvailableCallback(
      base::BindRepeating(&VrShellGl::OnContentOverlayFrameAvailable,
                          weak_ptr_factory_.GetWeakPtr()));
  ui_surface_texture_->SetFrameAvailableCallback(base::BindRepeating(
      &VrShellGl::OnUiFrameAvailable, weak_ptr_factory_.GetWeakPtr()));

  content_surface_texture_->SetDefaultBufferSize(
      content_tex_buffer_size_.width(), content_tex_buffer_size_.height());
  content_overlay_surface_texture_->SetDefaultBufferSize(
      content_tex_buffer_size_.width(), content_tex_buffer_size_.height());

  // InitializeRenderer calls GvrDelegateReady which triggers actions such as
  // responding to RequestPresent.
  InitializeRenderer();

  ui_->OnGlInitialized(content_texture_id_, kGlTextureLocationExternal,
                       content_overlay_texture_id_, kGlTextureLocationExternal,
                       ui_texture_id);
}

void VrShellGl::OnGpuProcessConnectionReady() {
  DVLOG(1) << __func__;
  CHECK(mailbox_bridge_);

  webxr_.set_mailbox_bridge_ready(true);
  // We might have a deferred submit that was waiting for
  // mailbox_bridge_ready.
  webxr_.TryDeferredProcessing();

  // See if we can send a VSync.
  WebVrTryStartAnimatingFrame(false);
}

void VrShellGl::CreateSurfaceBridge(gl::SurfaceTexture* surface_texture) {
  DCHECK(!mailbox_bridge_);
  webxr_.set_mailbox_bridge_ready(false);
  mailbox_bridge_ = std::make_unique<MailboxToSurfaceBridge>();
  if (surface_texture) {
    mailbox_bridge_->CreateSurface(surface_texture);
  }
  mailbox_bridge_->CreateAndBindContextProvider(base::BindOnce(
      &VrShellGl::OnGpuProcessConnectionReady, weak_ptr_factory_.GetWeakPtr()));
}

void VrShellGl::CreateOrResizeWebVRSurface(const gfx::Size& size) {
  DVLOG(2) << __func__ << ": size=" << size.width() << "x" << size.height();
  if (!webvr_surface_texture_) {
    if (webxr_use_shared_buffer_draw_) {
      // We don't have a surface in SharedBuffer mode, just update the size.
      webvr_surface_size_ = size;
      return;
    }
    DLOG(ERROR) << "No WebVR surface texture available";
    return;
  }

  // ContentPhysicalBoundsChanged is getting called twice with
  // identical sizes? Avoid thrashing the existing context.
  if (mailbox_bridge_ && (size == webvr_surface_size_)) {
    DVLOG(1) << "Ignore resize, size is unchanged";
    return;
  }

  if (!size.width() || !size.height()) {
    // Defer until a new size arrives on a future bounds update.
    DVLOG(1) << "Ignore resize, invalid size";
    return;
  }

  webvr_surface_texture_->SetDefaultBufferSize(size.width(), size.height());
  webvr_surface_size_ = size;

  if (mailbox_bridge_) {
    mailbox_bridge_->ResizeSurface(size.width(), size.height());
  } else {
    CreateSurfaceBridge(webvr_surface_texture_.get());
  }
}

void VrShellGl::WebVrCreateOrResizeSharedBufferImage(WebXrSharedBuffer* buffer,
                                                     const gfx::Size& size) {
  TRACE_EVENT0("gpu", __func__);
  // Unbind previous image (if any).
  if (buffer->remote_image) {
    DVLOG(2) << ": UnbindSharedBuffer, remote_image=" << buffer->remote_image;
    mailbox_bridge_->UnbindSharedBuffer(buffer->remote_image,
                                        buffer->remote_texture);
    buffer->remote_image = 0;
  }

  DVLOG(2) << __func__ << ": width=" << size.width()
           << " height=" << size.height();
  // Remove reference to previous image (if any).
  buffer->local_glimage = nullptr;

  const gfx::BufferFormat format = gfx::BufferFormat::RGBA_8888;
  const gfx::BufferUsage usage = gfx::BufferUsage::SCANOUT;

  gfx::GpuMemoryBufferId kBufferId(webxr_.next_memory_buffer_id++);
  buffer->gmb = gpu::GpuMemoryBufferImplAndroidHardwareBuffer::Create(
      kBufferId, size, format, usage,
      gpu::GpuMemoryBufferImpl::DestructionCallback());

  buffer->remote_image = mailbox_bridge_->BindSharedBufferImage(
      buffer->gmb.get(), size, format, usage, buffer->remote_texture);
  DVLOG(2) << ": BindSharedBufferImage, remote_image=" << buffer->remote_image;

  scoped_refptr<gl::GLImageAHardwareBuffer> img(
      new gl::GLImageAHardwareBuffer(webvr_surface_size_));

  base::android::ScopedHardwareBufferHandle ahb =
      buffer->gmb->CloneHandle().android_hardware_buffer;
  bool ret = img->Initialize(ahb.get(), false /* preserved */);
  if (!ret) {
    DLOG(WARNING) << __func__ << ": ERROR: failed to initialize image!";
    // Exiting VR is a bit drastic, but this error shouldn't occur under normal
    // operation. If it's an issue in practice, look into other recovery
    // options such as shutting down the WebVR/WebXR presentation session.
    ForceExitVr();
    return;
  }
  glBindTexture(GL_TEXTURE_EXTERNAL_OES, buffer->local_texture);
  img->BindTexImage(GL_TEXTURE_EXTERNAL_OES);
  buffer->local_glimage = std::move(img);
}

void VrShellGl::WebVrPrepareSharedBuffer(const gfx::Size& size) {
  TRACE_EVENT0("gpu", __func__);

  DVLOG(2) << __func__ << ": size=" << size.width() << "x" << size.height();
  CHECK(webxr_.mailbox_bridge_ready());
  CHECK(webxr_.HaveAnimatingFrame());

  WebXrSharedBuffer* buffer;
  if (webxr_.GetAnimatingFrame()->shared_buffer) {
    buffer = webxr_.GetAnimatingFrame()->shared_buffer.get();
  } else {
    // Create buffer and do one-time setup for resources that stay valid after
    // size changes.
    webxr_.GetAnimatingFrame()->shared_buffer =
        std::make_unique<WebXrSharedBuffer>();
    buffer = webxr_.GetAnimatingFrame()->shared_buffer.get();

    // Remote resources
    buffer->mailbox_holder = std::make_unique<gpu::MailboxHolder>();
    buffer->mailbox_holder->texture_target = GL_TEXTURE_2D;
    buffer->remote_texture =
        mailbox_bridge_->CreateMailboxTexture(&buffer->mailbox_holder->mailbox);

    // Local resources
    glGenTextures(1, &buffer->local_texture);
  }

  if (size != buffer->size) {
    // Don't need the image for zero copy mode.
    WebVrCreateOrResizeSharedBufferImage(buffer, size);
    // We always need a valid sync token, even if not using
    // the image. The Renderer waits for it before using the
    // mailbox. Technically we don't need to update it
    // after resize for zero copy mode, but we do need it
    // after initial creation.
    mailbox_bridge_->GenSyncToken(&buffer->mailbox_holder->sync_token);

    // Save the size to avoid expensive reallocation next time.
    buffer->size = size;
  }
}

void VrShellGl::OnWebVRTokenSignaled(int16_t frame_index,
                                     std::unique_ptr<gfx::GpuFence> gpu_fence) {
  TRACE_EVENT1("gpu", "VrShellGl::OnWebVRTokenSignaled", "frame", frame_index);
  DVLOG(2) << __func__ << ": frame=" << frame_index;

  // Ignore if not processing a frame. This can happen on exiting presentation.
  if (!webxr_.HaveProcessingFrame())
    return;

  webxr_.GetProcessingFrame()->gvr_handoff_fence =
      gl::GLFence::CreateFromGpuFence(*gpu_fence);

  base::TimeTicks now = base::TimeTicks::Now();
  DrawFrame(frame_index, now);
}

bool VrShellGl::IsSubmitFrameExpected(int16_t frame_index) {
  // submit_client_ could be null when we exit presentation, if there were
  // pending SubmitFrame messages queued.  VRDisplayClient::OnExitPresent
  // will clean up state in blink, so it doesn't wait for
  // OnSubmitFrameTransferred or OnSubmitFrameRendered. Similarly,
  // the animating frame state is cleared when exiting presentation,
  // and we should ignore a leftover queued SubmitFrame.
  if (!submit_client_.get() || !webxr_.HaveAnimatingFrame())
    return false;

  WebXrFrame* animating_frame = webxr_.GetAnimatingFrame();

  if (animating_frame->index != frame_index) {
    DVLOG(1) << __func__ << ": wrong frame index, got " << frame_index
             << ", expected " << animating_frame->index;
    mojo::ReportBadMessage("SubmitFrame called with wrong frame index");
    presentation_binding_.Close();
    frame_data_binding_.Close();
    return false;
  }

  // Frame looks valid.
  return true;
}

void VrShellGl::SubmitFrameMissing(int16_t frame_index,
                                   const gpu::SyncToken& sync_token) {
  TRACE_EVENT1("gpu", "VrShellGl::SubmitWebVRFrame", "frame", frame_index);

  if (!IsSubmitFrameExpected(frame_index))
    return;

  // Renderer didn't submit a frame. Wait for the sync token to ensure
  // that any mailbox_bridge_ operations for the next frame happen after
  // whatever drawing the Renderer may have done before exiting.
  if (webxr_.mailbox_bridge_ready())
    mailbox_bridge_->WaitSyncToken(sync_token);

  DVLOG(2) << __func__ << ": recycle unused animating frame";
  DCHECK(webxr_.HaveAnimatingFrame());
  webxr_.RecycleUnusedAnimatingFrame();
}

bool VrShellGl::SubmitFrameCommon(int16_t frame_index,
                                  base::TimeDelta time_waited) {
  TRACE_EVENT1("gpu", "VrShellGl::SubmitWebVRFrame", "frame", frame_index);
  DVLOG(2) << __func__ << ": frame=" << frame_index;

  if (!IsSubmitFrameExpected(frame_index))
    return false;

  // If we get here, treat as a valid submit.
  DCHECK(webxr_.HaveAnimatingFrame());
  WebXrFrame* animating_frame = webxr_.GetAnimatingFrame();

  animating_frame->time_js_submit = base::TimeTicks::Now();

  // The JavaScript wait time is supplied externally and not trustworthy. Clamp
  // to a reasonable range to avoid math errors.
  if (time_waited < base::TimeDelta())
    time_waited = base::TimeDelta();
  if (time_waited > base::TimeDelta::FromSeconds(1))
    time_waited = base::TimeDelta::FromSeconds(1);
  webvr_js_wait_time_.AddSample(time_waited);
  TRACE_COUNTER1("gpu", "WebVR JS wait (ms)",
                 webvr_js_wait_time_.GetAverage().InMilliseconds());

  // Always tell the UI that we have a new WebVR frame, so that it can
  // transition the UI state to "presenting" and cancel any pending timeouts.
  // That's a prerequisite for ShouldDrawWebVr to become true, which is in turn
  // required to complete a processing frame.
  OnNewWebVRFrame();

  if (!ShouldDrawWebVr()) {
    DVLOG(1) << "Discarding received frame, UI is active";
    WebVrCancelAnimatingFrame();
    return false;
  }

  return true;
}

void VrShellGl::SubmitFrameDrawnIntoTexture(int16_t frame_index,
                                            const gpu::SyncToken& sync_token,
                                            base::TimeDelta time_waited) {
  if (!SubmitFrameCommon(frame_index, time_waited))
    return;

  webxr_.ProcessOrDefer(base::BindOnce(&VrShellGl::ProcessWebVrFrameFromGMB,
                                       weak_ptr_factory_.GetWeakPtr(),
                                       frame_index, sync_token));
}

void VrShellGl::ProcessWebVrFrameFromGMB(int16_t frame_index,
                                         const gpu::SyncToken& sync_token) {
  TRACE_EVENT0("gpu", __func__);

  mailbox_bridge_->CreateGpuFence(
      sync_token, base::BindOnce(&VrShellGl::OnWebVRTokenSignaled,
                                 weak_ptr_factory_.GetWeakPtr(), frame_index));

  // Unblock the next animating frame in case it was waiting for this
  // one to start processing.
  WebVrTryStartAnimatingFrame(false);
}

void VrShellGl::SubmitFrame(int16_t frame_index,
                            const gpu::MailboxHolder& mailbox,
                            base::TimeDelta time_waited) {
  if (!SubmitFrameCommon(frame_index, time_waited))
    return;

  webxr_.ProcessOrDefer(base::BindOnce(&VrShellGl::ProcessWebVrFrameFromMailbox,
                                       weak_ptr_factory_.GetWeakPtr(),
                                       frame_index, mailbox));
}

void VrShellGl::ProcessWebVrFrameFromMailbox(
    int16_t frame_index,
    const gpu::MailboxHolder& mailbox) {
  TRACE_EVENT0("gpu", __func__);

  // LIFECYCLE: pending_frames_ should be empty when there's no processing
  // frame. It gets one element here, and then is emptied again before leaving
  // processing state. Swapping twice on a Surface without calling
  // updateTexImage in between can lose frames, so don't draw+swap if we
  // already have a pending frame we haven't consumed yet.
  DCHECK(pending_frames_.empty());

  // LIFECYCLE: We shouldn't have gotten here unless mailbox_bridge_ is ready.
  DCHECK(webxr_.mailbox_bridge_ready());

  // Don't allow any state changes for this processing frame until it
  // arrives on the Surface. See OnWebVRFrameAvailable.
  DCHECK(webxr_.HaveProcessingFrame());
  webxr_.GetProcessingFrame()->state_locked = true;

  bool swapped = mailbox_bridge_->CopyMailboxToSurfaceAndSwap(mailbox);
  DCHECK(swapped);
  // Tell OnWebVRFrameAvailable to expect a new frame to arrive on
  // the SurfaceTexture, and save the associated frame index.
  pending_frames_.emplace(frame_index);

  // LIFECYCLE: we should have a pending frame now.
  DCHECK_EQ(pending_frames_.size(), 1U);

  // Notify the client that we're done with the mailbox so that the underlying
  // image is eligible for destruction.
  submit_client_->OnSubmitFrameTransferred(true);

  // Unblock the next animating frame in case it was waiting for this
  // one to start processing.
  WebVrTryStartAnimatingFrame(false);
}

void VrShellGl::SubmitFrameWithTextureHandle(
    int16_t frame_index,
    mojo::ScopedHandle texture_handle) {
  NOTREACHED();
}

void VrShellGl::ConnectPresentingService(
    device::mojom::VRDisplayInfoPtr display_info,
    device::mojom::XRRuntimeSessionOptionsPtr options) {
  ClosePresentationBindings();

  device::mojom::XRPresentationProviderPtr presentation_provider;
  presentation_binding_.Bind(mojo::MakeRequest(&presentation_provider));
  device::mojom::XRFrameDataProviderPtr frame_data_provider;
  frame_data_binding_.Bind(mojo::MakeRequest(&frame_data_provider));

  gfx::Size webvr_size(
      display_info->leftEye->renderWidth + display_info->rightEye->renderWidth,
      display_info->leftEye->renderHeight);
  DVLOG(1) << __func__ << ": resize initial to " << webvr_size.width() << "x"
           << webvr_size.height();

  // Decide which transport mechanism we want to use. This sets
  // the webxr_use_* options as a side effect.
  device::mojom::XRPresentationTransportOptionsPtr transport_options =
      GetWebVrFrameTransportOptions(options);

  if (webxr_use_shared_buffer_draw_) {
    // Create the mailbox bridge if it doesn't exist yet. We can continue
    // reusing the existing one if it does, its resources such as mailboxes
    // are still valid.
    if (!mailbox_bridge_)
      CreateSurfaceBridge(nullptr);
  } else {
    if (!webvr_surface_texture_) {
      webvr_surface_texture_ = gl::SurfaceTexture::Create(webvr_texture_id_);
      webvr_surface_texture_->SetFrameAvailableCallback(base::BindRepeating(
          &VrShellGl::OnWebVRFrameAvailable, weak_ptr_factory_.GetWeakPtr()));
    }
    CreateOrResizeWebVRSurface(webvr_size);
  }

  ScheduleOrCancelWebVrFrameTimeout();

  auto submit_frame_sink = device::mojom::XRPresentationConnection::New();
  submit_frame_sink->client_request = mojo::MakeRequest(&submit_client_);
  submit_frame_sink->provider = presentation_provider.PassInterface();
  submit_frame_sink->transport_options = std::move(transport_options);

  auto session = device::mojom::XRSession::New();
  session->data_provider = frame_data_provider.PassInterface();
  session->submit_frame_sink = std::move(submit_frame_sink);

  browser_->SendRequestPresentReply(std::move(session));
}

void VrShellGl::OnSwapContents(int new_content_id) {
  ui_->OnSwapContents(new_content_id);
}

void VrShellGl::EnableAlertDialog(PlatformInputHandler* input_handler,
                                  float width,
                                  float height) {
  showing_vr_dialog_ = true;
  vr_dialog_input_delegate_.reset(new PlatformUiInputDelegate(input_handler));
  vr_dialog_input_delegate_->SetSize(width, height);
  if (web_vr_mode_) {
    ui_->SetAlertDialogEnabled(true, vr_dialog_input_delegate_.get(), width,
                               height);
  } else {
    ui_->SetContentOverlayAlertDialogEnabled(
        true, vr_dialog_input_delegate_.get(),
        width / content_tex_buffer_size_.width(),
        height / content_tex_buffer_size_.width());
  }
  ScheduleOrCancelWebVrFrameTimeout();
}

void VrShellGl::DisableAlertDialog() {
  showing_vr_dialog_ = false;
  ui_->SetAlertDialogEnabled(false, nullptr, 0, 0);
  vr_dialog_input_delegate_ = nullptr;
  ScheduleOrCancelWebVrFrameTimeout();
}

void VrShellGl::SetAlertDialogSize(float width, float height) {
  if (vr_dialog_input_delegate_)
    vr_dialog_input_delegate_->SetSize(width, height);
  // If not floating, dialogs are rendered with a fixed width, so that only the
  // ratio matters. But, if they are floating, its size should be relative to
  // the contents. During a WebXR presentation, the contents might not have been
  // initialized but, in this case, the dialogs are never floating.
  if (web_vr_mode_) {
    ui_->SetAlertDialogSize(width, height);
  } else {
    ui_->SetContentOverlayAlertDialogSize(
        width / content_tex_buffer_size_.width(),
        height / content_tex_buffer_size_.width());
  }
}

void VrShellGl::SetDialogLocation(float x, float y) {
  ui_->SetDialogLocation(x, y);
}

void VrShellGl::SetDialogFloating(bool floating) {
  ui_->SetDialogFloating(floating);
}

void VrShellGl::ShowToast(const base::string16& text) {
  ui_->ShowPlatformToast(text);
}

void VrShellGl::CancelToast() {
  ui_->CancelPlatformToast();
}

void VrShellGl::ResumeContentRendering() {
  if (!content_paused_)
    return;
  content_paused_ = false;

  // Note that we have to UpdateTexImage here because OnContentFrameAvailable
  // won't be fired again until we've updated.
  content_surface_texture_->UpdateTexImage();
}

void VrShellGl::OnContentFrameAvailable() {
  if (content_paused_)
    return;
  content_surface_texture_->UpdateTexImage();
}

void VrShellGl::OnContentOverlayFrameAvailable() {
  content_overlay_surface_texture_->UpdateTexImage();
}

void VrShellGl::OnUiFrameAvailable() {
  ui_surface_texture_->UpdateTexImage();
}

void VrShellGl::OnWebVRFrameAvailable() {
  // This is called each time a frame that was drawn on the WebVR Surface
  // arrives on the SurfaceTexture.

  // This event should only occur in response to a SwapBuffers from
  // an incoming SubmitFrame call.
  DCHECK(!pending_frames_.empty()) << ": Frame arrived before SubmitFrame";

  // LIFECYCLE: we should have exactly one pending frame. This is true
  // even after exiting a session with a not-yet-surfaced frame.
  DCHECK_EQ(pending_frames_.size(), 1U);

  webvr_surface_texture_->UpdateTexImage();
  int frame_index = pending_frames_.front();
  TRACE_EVENT1("gpu", "VrShellGl::OnWebVRFrameAvailable", "frame", frame_index);
  pending_frames_.pop();

  // The usual transform matrix we get for the Surface flips Y, so we need to
  // apply it in the copy shader to get the correct image orientation:
  //  {1,  0, 0, 0,
  //   0, -1, 0, 0,
  //   0,  0, 1, 0,
  //   0,  1, 0, 1}
  webvr_surface_texture_->GetTransformMatrix(
      &webvr_surface_texture_uv_transform_[0]);

  // LIFECYCLE: we should be in processing state.
  DCHECK(webxr_.HaveProcessingFrame());
  WebXrFrame* processing_frame = webxr_.GetProcessingFrame();

  // Frame should be locked. Unlock it.
  DCHECK(processing_frame->state_locked);
  processing_frame->state_locked = false;

  if (ShouldDrawWebVr() && !processing_frame->recycle_once_unlocked) {
    DCHECK_EQ(processing_frame->index, frame_index);
    DrawFrame(frame_index, base::TimeTicks::Now());
  } else {
    // Silently consume a frame if we don't want to draw it. This can happen
    // due to an active exclusive UI such as a permission prompt, or after
    // exiting a presentation session when a pending frame arrives late.
    DVLOG(1) << __func__ << ": discarding frame, "
             << (web_vr_mode_ ? "UI is active" : "not presenting");
    WebVrCancelProcessingFrameAfterTransfer();
    // We're no longer in processing state, unblock pending processing frames.
    webxr_.TryDeferredProcessing();
  }
}

void VrShellGl::OnNewWebVRFrame() {
  ui_->OnWebVrFrameAvailable();

  if (web_vr_mode_) {
    ++webvr_frames_received_;

    webvr_fps_meter_.AddFrame(base::TimeTicks::Now());
    TRACE_COUNTER1("gpu", "WebVR FPS", webvr_fps_meter_.GetFPS());
  }

  ScheduleOrCancelWebVrFrameTimeout();
}

void VrShellGl::ScheduleOrCancelWebVrFrameTimeout() {
  // TODO(mthiesse): We should also timeout after the initial frame to prevent
  // bad experiences, but we have to be careful to handle things like splash
  // screens correctly. For now just ensure we receive a first frame.
  if (!web_vr_mode_ || webvr_frames_received_ > 0 || showing_vr_dialog_) {
    if (!webvr_frame_timeout_.IsCancelled())
      webvr_frame_timeout_.Cancel();
    if (!webvr_spinner_timeout_.IsCancelled())
      webvr_spinner_timeout_.Cancel();
    return;
  }
  if (ui_->CanSendWebVrVSync() && submit_client_) {
    webvr_spinner_timeout_.Reset(base::BindOnce(
        &VrShellGl::OnWebVrTimeoutImminent, base::Unretained(this)));
    task_runner_->PostDelayedTask(
        FROM_HERE, webvr_spinner_timeout_.callback(),
        base::TimeDelta::FromSeconds(kWebVrSpinnerTimeoutSeconds));
    webvr_frame_timeout_.Reset(base::BindOnce(&VrShellGl::OnWebVrFrameTimedOut,
                                              base::Unretained(this)));
    task_runner_->PostDelayedTask(
        FROM_HERE, webvr_frame_timeout_.callback(),
        base::TimeDelta::FromSeconds(kWebVrInitialFrameTimeoutSeconds));
  }
}

void VrShellGl::OnWebVrFrameTimedOut() {
  ui_->OnWebVrTimedOut();
}

void VrShellGl::OnWebVrTimeoutImminent() {
  ui_->OnWebVrTimeoutImminent();
}

void VrShellGl::GvrInit(gvr_context* gvr_api) {
  gvr_api_ = gvr::GvrApi::WrapNonOwned(gvr_api);

  MetricsUtilAndroid::LogVrViewerType(gvr_api_->GetViewerType());

  cardboard_ =
      (gvr_api_->GetViewerType() == gvr::ViewerType::GVR_VIEWER_TYPE_CARDBOARD);
  if (cardboard_ && web_vr_mode_) {
    browser_->ToggleCardboardGamepad(true);
  }
}

device::mojom::XRPresentationTransportOptionsPtr
VrShellGl::GetWebVrFrameTransportOptions(
    const device::mojom::XRRuntimeSessionOptionsPtr& options) {
  DVLOG(1) << __func__;

  MetricsUtilAndroid::XRRenderPath render_path =
      MetricsUtilAndroid::XRRenderPath::kClientWait;
  webxr_use_shared_buffer_draw_ = false;
  webxr_use_gpu_fence_ = false;

  std::string render_path_string = base::GetFieldTrialParamValueByFeature(
      features::kWebXrRenderPath, features::kWebXrRenderPathParamName);
  DVLOG(1) << __func__ << ": WebXrRenderPath=" << render_path_string;
  if (render_path_string == features::kWebXrRenderPathParamValueClientWait) {
    // Use the baseline kClientWait.
  } else if (render_path_string ==
             features::kWebXrRenderPathParamValueGpuFence) {
    // Use GpuFence if available. If not, fall back to kClientWait.
    if (gl::GLFence::IsGpuFenceSupported()) {
      webxr_use_gpu_fence_ = true;

      render_path = MetricsUtilAndroid::XRRenderPath::kGpuFence;
    }
  } else {
    // Default aka features::kWebXrRenderPathParamValueSharedBuffer.
    // Use that if supported, otherwise fall back to GpuFence or
    // ClientWait.
    if (gl::GLFence::IsGpuFenceSupported()) {
      webxr_use_gpu_fence_ = true;
      if (base::AndroidHardwareBufferCompat::IsSupportAvailable() &&
          !options->use_legacy_webvr_render_path) {
        // Currently, SharedBuffer mode is only supported for WebXR via
        // XRWebGlDrawingBuffer, WebVR 1.1 doesn't use that.
        webxr_use_shared_buffer_draw_ = true;
        render_path = MetricsUtilAndroid::XRRenderPath::kSharedBuffer;
      } else {
        render_path = MetricsUtilAndroid::XRRenderPath::kGpuFence;
      }
    }
  }
  DVLOG(1) << __func__ << ": render_path=" << static_cast<int>(render_path);
  MetricsUtilAndroid::LogXrRenderPathUsed(render_path);

  device::mojom::XRPresentationTransportOptionsPtr transport_options =
      device::mojom::XRPresentationTransportOptions::New();
  // Only set boolean options that we need. Default is false, and we should be
  // able to safely ignore ones that our implementation doesn't care about.
  transport_options->wait_for_transfer_notification = true;
  if (webxr_use_shared_buffer_draw_) {
    transport_options->transport_method =
        device::mojom::XRPresentationTransportMethod::DRAW_INTO_TEXTURE_MAILBOX;
    DCHECK(webxr_use_gpu_fence_);
    transport_options->wait_for_gpu_fence = true;
  } else {
    transport_options->transport_method =
        device::mojom::XRPresentationTransportMethod::SUBMIT_AS_MAILBOX_HOLDER;
    transport_options->wait_for_transfer_notification = true;
    if (webxr_use_gpu_fence_) {
      transport_options->wait_for_gpu_fence = true;
    } else {
      transport_options->wait_for_render_notification = true;
    }
  }
  return transport_options;
}

void VrShellGl::InitializeRenderer() {
  gvr_api_->InitializeGl();

  // Create multisampled and non-multisampled buffers.
  specs_.push_back(gvr_api_->CreateBufferSpec());
  specs_.push_back(gvr_api_->CreateBufferSpec());

  gvr::Sizei max_size = gvr_api_->GetMaximumEffectiveRenderTargetSize();
  float scale = low_density_ ? kLowDpiDefaultRenderTargetSizeScale
                             : kDefaultRenderTargetSizeScale;

  render_size_default_ = {max_size.width * scale, max_size.height * scale};
  render_size_webvr_ui_ = {max_size.width / kWebVrBrowserUiSizeFactor,
                           max_size.height / kWebVrBrowserUiSizeFactor};

  specs_[kMultiSampleBuffer].SetSamples(2);
  specs_[kMultiSampleBuffer].SetDepthStencilFormat(
      GVR_DEPTH_STENCIL_FORMAT_NONE);
  if (web_vr_mode_) {
    specs_[kMultiSampleBuffer].SetSize(render_size_webvr_ui_.width(),
                                       render_size_webvr_ui_.height());
  } else {
    specs_[kMultiSampleBuffer].SetSize(render_size_default_.width(),
                                       render_size_default_.height());
  }

  specs_[kNoMultiSampleBuffer].SetSamples(1);
  specs_[kNoMultiSampleBuffer].SetDepthStencilFormat(
      GVR_DEPTH_STENCIL_FORMAT_NONE);
  specs_[kNoMultiSampleBuffer].SetSize(render_size_default_.width(),
                                       render_size_default_.height());

  swap_chain_ = gvr_api_->CreateSwapChain(specs_);

  UpdateViewports();

  browser_->GvrDelegateReady(gvr_api_->GetViewerType());
}

void VrShellGl::UpdateViewports() {
  if (!viewports_need_updating_)
    return;
  viewports_need_updating_ = false;

  gvr::BufferViewportList viewport_list =
      gvr_api_->CreateEmptyBufferViewportList();

  // Set up main content viewports. The list has two elements, 0=left eye and
  // 1=right eye.
  viewport_list.SetToRecommendedBufferViewports();
  viewport_list.GetBufferViewport(0, &main_viewport_.left);
  viewport_list.GetBufferViewport(1, &main_viewport_.right);
  // Save copies of the first two viewport items for use by WebVR, it sets its
  // own UV bounds.
  viewport_list.GetBufferViewport(0, &webvr_viewport_.left);
  viewport_list.GetBufferViewport(1, &webvr_viewport_.right);
  webvr_viewport_.left.SetSourceUv(
      UVFromGfxRect(ClampRect(current_webvr_frame_bounds_.left_bounds)));
  webvr_viewport_.right.SetSourceUv(
      UVFromGfxRect(ClampRect(current_webvr_frame_bounds_.right_bounds)));

  // Set up Content UI viewports. Content will be shown as a quad layer to get
  // the best possible quality.
  viewport_list.GetBufferViewport(0, &content_underlay_viewport_.left);
  viewport_list.GetBufferViewport(1, &content_underlay_viewport_.right);
  viewport_list.GetBufferViewport(0, &webvr_overlay_viewport_.left);
  viewport_list.GetBufferViewport(1, &webvr_overlay_viewport_.right);

  main_viewport_.SetSourceBufferIndex(kMultiSampleBuffer);
  webvr_overlay_viewport_.SetSourceBufferIndex(kMultiSampleBuffer);
  webvr_viewport_.SetSourceBufferIndex(kNoMultiSampleBuffer);
  content_underlay_viewport_.SetSourceBufferIndex(kNoMultiSampleBuffer);
  content_underlay_viewport_.SetSourceUv(kContentUv);
}

bool VrShellGl::ResizeForWebVR(int16_t frame_index) {
  // Process all pending_bounds_ changes targeted for before this frame, being
  // careful of wrapping frame indices.
  static constexpr unsigned max =
      std::numeric_limits<WebXrPresentationState::FrameIndexType>::max();
  static_assert(max > WebXrPresentationState::kWebXrFrameCount * 2,
                "To detect wrapping, kPoseRingBufferSize must be smaller "
                "than half of next_frame_index_ range.");
  while (!pending_bounds_.empty()) {
    uint16_t index = pending_bounds_.front().first;
    // If index is less than the frame_index it's possible we've wrapped, so we
    // extend the range and 'un-wrap' to account for this.
    if (index < frame_index)
      index += max + 1;
    // If the pending bounds change is for an upcoming frame within our buffer
    // size, wait to apply it. Otherwise, apply it immediately. This guarantees
    // that even if we miss many frames, the queue can't fill up with stale
    // bounds.
    if (index > frame_index &&
        index <= frame_index + WebXrPresentationState::kWebXrFrameCount)
      break;

    const WebVrBounds& bounds = pending_bounds_.front().second;
    webvr_viewport_.left.SetSourceUv(UVFromGfxRect(bounds.left_bounds));
    webvr_viewport_.right.SetSourceUv(UVFromGfxRect(bounds.right_bounds));
    current_webvr_frame_bounds_ =
        bounds;  // If we recreate the viewports, keep these bounds.
    DVLOG(1) << __func__ << ": resize from pending_bounds to "
             << bounds.source_size.width() << "x"
             << bounds.source_size.height();
    CreateOrResizeWebVRSurface(bounds.source_size);
    pending_bounds_.pop();
  }

  // Resize the webvr overlay buffer, which may have been used by the content
  // quad buffer previously.
  gvr::Sizei size = swap_chain_.GetBufferSize(kMultiSampleBuffer);
  gfx::Size target_size = render_size_webvr_ui_;
  if (size.width != target_size.width() ||
      size.height != target_size.height()) {
    swap_chain_.ResizeBuffer(kMultiSampleBuffer,
                             {target_size.width(), target_size.height()});
  }

  size = swap_chain_.GetBufferSize(kNoMultiSampleBuffer);
  target_size = webvr_surface_size_;
  if (!target_size.width()) {
    // Don't try to resize to 0x0 pixels, drop frames until we get a valid
    // size.
    return false;
  }
  if (size.width != target_size.width() ||
      size.height != target_size.height()) {
    swap_chain_.ResizeBuffer(kNoMultiSampleBuffer,
                             {target_size.width(), target_size.height()});
  }
  return true;
}

void VrShellGl::UpdateEyeInfos(const gfx::Transform& head_pose,
                               const Viewport& viewport,
                               const gfx::Size& render_size,
                               RenderInfo* out_render_info) {
  for (auto eye : {GVR_LEFT_EYE, GVR_RIGHT_EYE}) {
    CameraModel& eye_info = (eye == GVR_LEFT_EYE)
                                ? out_render_info->left_eye_model
                                : out_render_info->right_eye_model;
    eye_info.eye_type =
        (eye == GVR_LEFT_EYE) ? EyeType::kLeftEye : EyeType::kRightEye;

    const gvr::BufferViewport& vp =
        (eye == GVR_LEFT_EYE) ? viewport.left : viewport.right;

    gfx::Transform eye_matrix;
    GvrMatToTransform(gvr_api_->GetEyeFromHeadMatrix(eye), &eye_matrix);
    eye_info.view_matrix = eye_matrix * head_pose;

    const gfx::RectF& rect = GfxRectFromUV(vp.GetSourceUv());
    eye_info.viewport = vr::CalculatePixelSpaceRect(render_size, rect);

    eye_info.proj_matrix =
        PerspectiveMatrixFromView(vp.GetSourceFov(), kZNear, kZFar);
    eye_info.view_proj_matrix = eye_info.proj_matrix * eye_info.view_matrix;
  }
}

void VrShellGl::UpdateContentViewportTransforms(
    const gfx::Transform& head_pose) {
  gfx::Transform quad_transform = ui_->GetContentWorldSpaceTransform();
  // The Texture Quad renderer draws quads that extend from -0.5 to 0.5 in X and
  // Y, while Daydream's quad layers are implicitly -1.0 to 1.0. Thus to ensure
  // things line up we have to scale by 0.5.
  quad_transform.Scale3d(0.5f * kContentVignetteScale,
                         0.5f * kContentVignetteScale, 1.0f);

  for (auto eye : {GVR_LEFT_EYE, GVR_RIGHT_EYE}) {
    CameraModel camera = (eye == GVR_LEFT_EYE) ? render_info_.left_eye_model
                                               : render_info_.right_eye_model;
    gfx::Transform transform = camera.view_matrix * quad_transform;
    gvr::Mat4f viewport_transform;
    TransformToGvrMat(transform, &viewport_transform);
    gvr::BufferViewport& viewport = (eye == GVR_LEFT_EYE)
                                        ? content_underlay_viewport_.left
                                        : content_underlay_viewport_.right;
    viewport.SetTransform(viewport_transform);
  }
}

void VrShellGl::DrawFrame(int16_t frame_index, base::TimeTicks current_time) {
  TRACE_EVENT1("gpu", "VrShellGl::DrawFrame", "frame", frame_index);
  if (!webvr_delayed_gvr_submit_.IsCancelled()) {
    // The last submit to GVR didn't complete, we have an acquired frame. This
    // is normal when exiting WebVR, in that case we just want to reuse the
    // frame. It's not supposed to happen during WebVR presentation.
    if (frame_index >= 0) {
      // This is a WebVR frame from OnWebVRFrameAvailable which isn't supposed
      // to be delivered while the previous frame is still processing. Drop the
      // previous work to avoid errors and reuse the acquired frame.
      DLOG(WARNING) << "Unexpected WebVR DrawFrame during acquired frame";
    }
    webvr_delayed_gvr_submit_.Cancel();
    DrawIntoAcquiredFrame(frame_index, current_time);
    return;
  }

  if (web_vr_mode_ && !ShouldDrawWebVr()) {
    // We're in a WebVR session, but don't want to draw WebVR frames, i.e.
    // because UI has taken over for a permissions prompt. Do state cleanup if
    // needed.
    if (webxr_.HaveAnimatingFrame() &&
        webxr_.GetAnimatingFrame()->deferred_start_processing) {
      // We have an animating frame. Cancel it if it's waiting to start
      // processing. If not, keep it to receive the incoming SubmitFrame.
      DVLOG(1) << __func__ << ": cancel waiting WebVR frame, UI is active";
      WebVrCancelAnimatingFrame();
    }
  }

  // From this point on, the current frame is either a pure UI frame
  // (frame_index==-1), or a WebVR frame (frame_index >= 0). If it's a WebVR
  // frame, it must be the current processing frame. Careful, we may still have
  // a processing frame in UI mode that couldn't be cancelled yet. For example
  // when showing a permission prompt, ShouldDrawWebVr() may have become false
  // in the time between SubmitFrame and OnWebVRFrameAvailable or
  // OnWebVRTokenSignaled. In that case we continue handling the current frame
  // as a WebVR frame. Also, WebVR frames can still have overlay UI drawn on top
  // of them.
  bool is_webxr_frame = frame_index >= 0;
  DCHECK(!is_webxr_frame || webxr_.HaveProcessingFrame());
  CHECK(!acquired_frame_);

  // When using async reprojection, we need to know which pose was
  // used in the WebVR app for drawing this frame and supply it when
  // submitting. Technically we don't need a pose if not reprojecting,
  // but keeping it uninitialized seems likely to cause problems down
  // the road. Copying it is cheaper than fetching a new one.
  if (is_webxr_frame) {
    // Copy into render info for overlay UI. WebVR doesn't use this.
    DCHECK(webxr_.HaveProcessingFrame());
    WebXrFrame* frame = webxr_.GetProcessingFrame();
    render_info_.head_pose = frame->head_pose;
  } else {
    device::GvrDelegate::GetGvrPoseWithNeckModel(gvr_api_.get(),
                                                 &render_info_.head_pose);
  }

  UpdateViewports();

  // If needed, resize the primary buffer for use with WebVR. Resizing
  // needs to happen before acquiring a frame.
  if (is_webxr_frame) {
    if (!ResizeForWebVR(frame_index)) {
      // We don't have a valid size yet, can't draw.
      return;
    }
  } else {
    gvr::Sizei size = swap_chain_.GetBufferSize(kMultiSampleBuffer);
    gfx::Size target_size = render_size_default_;
    if (size.width != target_size.width() ||
        size.height != target_size.height()) {
      swap_chain_.ResizeBuffer(kMultiSampleBuffer,
                               {target_size.width(), target_size.height()});
    }
    size = swap_chain_.GetBufferSize(kNoMultiSampleBuffer);
    target_size = {content_tex_buffer_size_.width() * kContentVignetteScale,
                   content_tex_buffer_size_.height() * kContentVignetteScale};
    if (size.width != target_size.width() ||
        size.height != target_size.height()) {
      swap_chain_.ResizeBuffer(kNoMultiSampleBuffer,
                               {target_size.width(), target_size.height()});
    }
  }

  TRACE_EVENT_BEGIN0("gpu", "VrShellGl::AcquireFrame");
  base::TimeTicks acquire_start = base::TimeTicks::Now();
  acquired_frame_ = swap_chain_.AcquireFrame();
  webvr_acquire_time_.AddSample(base::TimeTicks::Now() - acquire_start);
  TRACE_EVENT_END0("gpu", "VrShellGl::AcquireFrame");
  if (!acquired_frame_)
    return;

  DrawIntoAcquiredFrame(frame_index, current_time);
}

void VrShellGl::DrawIntoAcquiredFrame(int16_t frame_index,
                                      base::TimeTicks current_time) {
  TRACE_EVENT1("gpu", "VrShellGl::DrawIntoAcquiredFrame", "frame", frame_index);
  last_used_head_pose_ = render_info_.head_pose;

  bool is_webxr_frame = frame_index >= 0;
  DCHECK(!is_webxr_frame || webxr_.HaveProcessingFrame());

  UpdateUi(render_info_, current_time, is_webxr_frame ? kWebXrFrame : kUiFrame);

  gvr::Sizei primary_render_size =
      is_webxr_frame ? swap_chain_.GetBufferSize(kNoMultiSampleBuffer)
                     : swap_chain_.GetBufferSize(kMultiSampleBuffer);
  UpdateEyeInfos(render_info_.head_pose, main_viewport_,
                 {primary_render_size.width, primary_render_size.height},
                 &render_info_);
  ui_->OnProjMatrixChanged(render_info_.left_eye_model.proj_matrix);

  // Content quad can't have transparency when using the quad layer because we
  // can't blend with the quad layer.
  bool use_quad_layer = ui_->IsContentVisibleAndOpaque();

  // We can't use webvr and the quad layer at the same time because they
  // currently share the same non-multisampled buffer.
  DCHECK(!is_webxr_frame || !use_quad_layer);

  viewport_list_ = gvr_api_->CreateEmptyBufferViewportList();

  ui_->SetContentUsesQuadLayer(use_quad_layer);
  if (use_quad_layer) {
    // This should be the first layer as it needs to be rendered behind the
    // rest of the browser UI, which punches a transparent hole through to this
    // layer.
    DCHECK_EQ(viewport_list_.GetSize(), 0U);

    UpdateContentViewportTransforms(render_info_.head_pose);

    viewport_list_.SetBufferViewport(viewport_list_.GetSize(),
                                     content_underlay_viewport_.left);
    viewport_list_.SetBufferViewport(viewport_list_.GetSize(),
                                     content_underlay_viewport_.right);

    // Draw the main browser content to a quad layer.
    acquired_frame_.BindBuffer(kNoMultiSampleBuffer);

    glClear(GL_COLOR_BUFFER_BIT);

    DrawContentQuad(!ui_->IsContentOverlayTextureEmpty());

    acquired_frame_.Unbind();
  }

  if (is_webxr_frame) {
    DCHECK_EQ(viewport_list_.GetSize(), 0U);
    viewport_list_.SetBufferViewport(0, webvr_viewport_.left);
    viewport_list_.SetBufferViewport(1, webvr_viewport_.right);
    acquired_frame_.BindBuffer(kNoMultiSampleBuffer);
    // We're redrawing over the entire viewport, but it's generally more
    // efficient on mobile tiling GPUs to clear anyway as a hint that
    // we're done with the old content. TODO(klausw, https://crbug.com/700389):
    // investigate using glDiscardFramebufferEXT here since that's more
    // efficient on desktop, but it would need a capability check since
    // it's not supported on older devices such as Nexus 5X.
    glClear(GL_COLOR_BUFFER_BIT);
    DrawWebVr();
    acquired_frame_.Unbind();
  } else {
    DCHECK_LE(viewport_list_.GetSize(), 2U);
    viewport_list_.SetBufferViewport(viewport_list_.GetSize(),
                                     main_viewport_.left);
    viewport_list_.SetBufferViewport(viewport_list_.GetSize(),
                                     main_viewport_.right);
    acquired_frame_.BindBuffer(kMultiSampleBuffer);
    glClear(GL_COLOR_BUFFER_BIT);
    // At this point, we draw non-WebVR content that could, potentially, fill
    // the viewport.  NB: this is not just 2d browsing stuff, we may have a
    // splash screen showing in WebVR mode that must also fill the screen. That
    // said, while the splash screen is up ShouldDrawWebVr() will return false,
    // and we only draw UI frames, not WebVR frames.
    ui_->Draw(render_info_);
    acquired_frame_.Unbind();
  }

  std::vector<const UiElement*> overlay_elements;
  if (is_webxr_frame && ui_->HasWebXrOverlayElementsToDraw()) {
    // WebVR content may use an arbitrary size buffer. We need to draw browser
    // UI on a different buffer to make sure that our UI has enough resolution.
    acquired_frame_.BindBuffer(kMultiSampleBuffer);
    glClear(GL_COLOR_BUFFER_BIT);
    // Update recommended fov and uv per frame.
    const auto& fov_recommended_left =
        ToUiFovRect(main_viewport_.left.GetSourceFov());
    const auto& fov_recommended_right =
        ToUiFovRect(main_viewport_.right.GetSourceFov());

    // Set render info to recommended setting. It will be used as our base for
    // optimization.
    RenderInfo render_info_webvr_browser_ui;
    render_info_webvr_browser_ui.head_pose = render_info_.head_pose;

    UpdateEyeInfos(render_info_webvr_browser_ui.head_pose,
                   webvr_overlay_viewport_, render_size_webvr_ui_,
                   &render_info_webvr_browser_ui);
    auto fovs = ui_->GetMinimalFovForWebXrOverlayElements(
        render_info_webvr_browser_ui.left_eye_model.view_matrix,
        fov_recommended_left,
        render_info_webvr_browser_ui.right_eye_model.view_matrix,
        fov_recommended_right, kZNear);
    webvr_overlay_viewport_.left.SetSourceFov(ToGvrRectf(fovs.first));
    webvr_overlay_viewport_.right.SetSourceFov(ToGvrRectf(fovs.second));

    DCHECK_EQ(viewport_list_.GetSize(), 2U);
    viewport_list_.SetBufferViewport(2, webvr_overlay_viewport_.left);
    viewport_list_.SetBufferViewport(3, webvr_overlay_viewport_.right);
    UpdateEyeInfos(render_info_webvr_browser_ui.head_pose,
                   webvr_overlay_viewport_, render_size_webvr_ui_,
                   &render_info_webvr_browser_ui);

    ui_->DrawWebVrOverlayForeground(render_info_webvr_browser_ui);

    acquired_frame_.Unbind();
  }

  // GVR submit needs the exact head pose that was used for rendering.
  gfx::Transform submit_head_pose;
  if (is_webxr_frame) {
    // Don't use render_info_.head_pose here, that may have been
    // overwritten by OnVSync's controller handling. We need the pose that was
    // sent to JS.
    submit_head_pose = webxr_.GetProcessingFrame()->head_pose;
  } else {
    submit_head_pose = render_info_.head_pose;
  }
  std::unique_ptr<gl::GLFenceEGL> fence = nullptr;
  if (is_webxr_frame && surfaceless_rendering_) {
    webxr_.GetProcessingFrame()->time_copied = base::TimeTicks::Now();
    if (webxr_use_gpu_fence_) {
      // Continue with submit once the previous frame's GL fence signals that
      // it is done rendering. This avoids blocking in GVR's Submit. Fence is
      // null for the first frame, in that case the fence wait is skipped.
      if (webvr_prev_frame_completion_fence_ &&
          webvr_prev_frame_completion_fence_->HasCompleted()) {
        // The fence had already signaled. We can get the signaled time from the
        // fence and submit immediately.
        AddWebVrRenderTimeEstimate(
            frame_index,
            webvr_prev_frame_completion_fence_->GetStatusChangeTime());
        webvr_prev_frame_completion_fence_.reset();
      } else {
        fence.reset(webvr_prev_frame_completion_fence_.release());
      }
    } else {
      // Continue with submit once a GL fence signals that current drawing
      // operations have completed.
      fence = gl::GLFenceEGL::Create();
    }
  }
  if (fence) {
    webvr_delayed_gvr_submit_.Reset(base::BindRepeating(
        &VrShellGl::DrawFrameSubmitWhenReady, base::Unretained(this)));
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(webvr_delayed_gvr_submit_.callback(), frame_index,
                       submit_head_pose, base::Passed(&fence)));
  } else {
    // Continue with submit immediately.
    DrawFrameSubmitNow(frame_index, submit_head_pose);
  }
}

void VrShellGl::WebVrWaitForServerFence() {
  DCHECK(webxr_.HaveProcessingFrame());

  std::unique_ptr<gl::GLFence> gpu_fence(
      webxr_.GetProcessingFrame()->gvr_handoff_fence.release());

  DCHECK(gpu_fence);
  // IMPORTANT: wait as late as possible to insert the server wait. Doing so
  // blocks the server from doing any further work on this GL context until
  // the fence signals, this prevents any older fences such as the ones we
  // may be using for other synchronization from signaling.

  gpu_fence->ServerWait();
  // Fence will be destroyed on going out of scope here.
  return;
}

void VrShellGl::DrawFrameSubmitWhenReady(
    int16_t frame_index,
    const gfx::Transform& head_pose,
    std::unique_ptr<gl::GLFenceEGL> fence) {
  TRACE_EVENT1("gpu", "VrShellGl::DrawFrameSubmitWhenReady", "frame",
               frame_index);
  DVLOG(2) << __func__ << ": frame=" << static_cast<int>(frame_index);
  bool use_polling = webxr_.mailbox_bridge_ready() &&
                     mailbox_bridge_->IsGpuWorkaroundEnabled(
                         gpu::DONT_USE_EGLCLIENTWAITSYNC_WITH_TIMEOUT);
  if (fence) {
    if (!use_polling) {
      // Use wait-with-timeout to find out as soon as possible when rendering
      // is complete.
      fence->ClientWaitWithTimeoutNanos(
          kWebVRFenceCheckTimeout.InMicroseconds() * 1000);
    }
    if (!fence->HasCompleted()) {
      webvr_delayed_gvr_submit_.Reset(base::BindRepeating(
          &VrShellGl::DrawFrameSubmitWhenReady, base::Unretained(this)));
      if (use_polling) {
        // Poll the fence status at a short interval. This burns some CPU, but
        // avoids excessive waiting on devices which don't handle timeouts
        // correctly. Downside is that the completion status is only detected
        // with a delay of up to one polling interval.
        task_runner_->PostDelayedTask(
            FROM_HERE,
            base::BindOnce(webvr_delayed_gvr_submit_.callback(), frame_index,
                           head_pose, base::Passed(&fence)),
            kWebVRFenceCheckPollInterval);
      } else {
        task_runner_->PostTask(
            FROM_HERE,
            base::BindOnce(webvr_delayed_gvr_submit_.callback(), frame_index,
                           head_pose, base::Passed(&fence)));
      }
      return;
    }
  }

  if (fence && webxr_use_gpu_fence_) {
    // We were waiting for the fence, so the time now is the actual
    // finish time for the previous frame's rendering.
    AddWebVrRenderTimeEstimate(frame_index, base::TimeTicks::Now());
  }

  webvr_delayed_gvr_submit_.Cancel();
  DrawFrameSubmitNow(frame_index, head_pose);
}

void VrShellGl::AddWebVrRenderTimeEstimate(
    int16_t frame_index,
    const base::TimeTicks& fence_complete_time) {
  if (!webxr_.HaveRenderingFrame())
    return;

  WebXrFrame* rendering_frame = webxr_.GetRenderingFrame();
  base::TimeTicks prev_js_submit = rendering_frame->time_js_submit;
  if (webxr_use_gpu_fence_ && !prev_js_submit.is_null() &&
      !fence_complete_time.is_null()) {
    webvr_render_time_.AddSample(fence_complete_time - prev_js_submit);
  }
}

void VrShellGl::WebVrSendRenderNotification(bool was_rendered) {
  if (!submit_client_)
    return;

  TRACE_EVENT0("gpu", __func__);
  if (webxr_use_gpu_fence_) {
    // Renderer is waiting for a frame-separating GpuFence.

    if (was_rendered) {
      // Save a fence for local completion checking.
      webvr_prev_frame_completion_fence_ =
          gl::GLFenceAndroidNativeFenceSync::CreateForGpuFence();
    }

    // Create a local GpuFence and pass it to the Renderer via IPC.
    std::unique_ptr<gl::GLFence> gl_fence = gl::GLFence::CreateForGpuFence();
    std::unique_ptr<gfx::GpuFence> gpu_fence = gl_fence->GetGpuFence();
    submit_client_->OnSubmitFrameGpuFence(
        gfx::CloneHandleForIPC(gpu_fence->GetGpuFenceHandle()));
  } else {
    // Renderer is waiting for the previous frame to render, unblock it now.
    submit_client_->OnSubmitFrameRendered();
  }
}

void VrShellGl::DrawFrameSubmitNow(int16_t frame_index,
                                   const gfx::Transform& head_pose) {
  TRACE_EVENT1("gpu", "VrShellGl::DrawFrameSubmitNow", "frame", frame_index);

  gvr::Mat4f mat;
  TransformToGvrMat(head_pose, &mat);
  bool is_webxr_frame = frame_index >= 0;
  {
    std::unique_ptr<ScopedGpuTrace> browser_gpu_trace;
    if (gl::GLFence::IsGpuFenceSupported() && !is_webxr_frame) {
      // This fence instance is created for the tracing side effect. Insert it
      // before GVR submit. Then replace the previous instance below after GVR
      // submit completes, at which point the previous fence (if any) should be
      // complete. Doing this in two steps avoids a race condition - a fence
      // that was inserted after Submit may not be complete yet when the next
      // Submit finishes.
      browser_gpu_trace = std::make_unique<ScopedGpuTrace>(
          "gpu", "VrShellGl::PostSubmitDrawOnGpu");
    }

    TRACE_EVENT0("gpu", "VrShellGl::SubmitToGvr");
    base::TimeTicks submit_start = base::TimeTicks::Now();
    acquired_frame_.Submit(viewport_list_, mat);
    base::TimeTicks submit_done = base::TimeTicks::Now();
    webvr_submit_time_.AddSample(submit_done - submit_start);
    CHECK(!acquired_frame_);

    if (browser_gpu_trace) {
      // Replacing the previous instance will record the trace result for
      // the previous instance.
      DCHECK(!gpu_trace_ || gpu_trace_->fence()->HasCompleted());
      gpu_trace_ = std::move(browser_gpu_trace);
    }
  }

  // No need to swap buffers for surfaceless rendering.
  if (!surfaceless_rendering_) {
    // TODO(mthiesse): Support asynchronous SwapBuffers.
    TRACE_EVENT0("gpu", "VrShellGl::SwapBuffers");
    surface_->SwapBuffers(base::DoNothing());
  }

  // At this point, ShouldDrawWebVr and webvr_frame_processing_ may have become
  // false for a WebVR frame. Ignore the ShouldDrawWebVr status to ensure we
  // send render notifications while paused for exclusive UI mode. Skip the
  // steps if we lost the processing state, that means presentation has ended.
  if (is_webxr_frame && webxr_.HaveProcessingFrame()) {
    // Report rendering completion to the Renderer so that it's permitted to
    // submit a fresh frame. We could do this earlier, as soon as the frame
    // got pulled off the transfer surface, but that results in overstuffed
    // buffers.
    WebVrSendRenderNotification(true);

    base::TimeTicks pose_time = webxr_.GetProcessingFrame()->time_pose;
    base::TimeTicks js_submit_time =
        webxr_.GetProcessingFrame()->time_js_submit;
    webvr_js_time_.AddSample(js_submit_time - pose_time);
    if (!webxr_use_gpu_fence_) {
      // Estimate render time from wallclock time, we waited for the pre-submit
      // render fence to signal.
      base::TimeTicks now = base::TimeTicks::Now();
      webvr_render_time_.AddSample(now - js_submit_time);
    }

    if (webxr_.HaveRenderingFrame()) {
      webxr_.EndFrameRendering();
    }
    webxr_.TransitionFrameProcessingToRendering();
  }

  // After saving the timestamp, fps will be available via GetFPS().
  // TODO(vollick): enable rendering of this framerate in a HUD.
  vr_ui_fps_meter_.AddFrame(base::TimeTicks::Now());
  DVLOG(1) << "fps: " << vr_ui_fps_meter_.GetFPS();
  TRACE_COUNTER1("gpu", "VR UI FPS", vr_ui_fps_meter_.GetFPS());
  TRACE_COUNTER2("gpu", "VR UI timing (us)", "scene update",
                 ui_processing_time().GetAverage().InMicroseconds(),
                 "controller",
                 ui_controller_update_time().GetAverage().InMicroseconds());

  if (is_webxr_frame) {
    // We finished processing a frame, this may make pending WebVR
    // work eligible to proceed.
    webxr_.TryDeferredProcessing();
  }

  if (ShouldDrawWebVr()) {
    // See if we can animate a new WebVR frame. Intentionally using
    // ShouldDrawWebVr here since we also want to run this check after
    // UI frames, i.e. transitioning from transient UI to WebVR.
    WebVrTryStartAnimatingFrame(false);
  }
}

bool VrShellGl::ShouldDrawWebVr() {
  return web_vr_mode_ && ui_->ShouldRenderWebVr() && webvr_frames_received_ > 0;
}

void VrShellGl::DrawWebVr() {
  TRACE_EVENT0("gpu", "VrShellGl::DrawWebVr");
  // Don't need face culling, depth testing, blending, etc. Turn it all off.
  glDisable(GL_CULL_FACE);
  glDisable(GL_SCISSOR_TEST);
  glDisable(GL_BLEND);
  glDisable(GL_POLYGON_OFFSET_FILL);

  glViewport(0, 0, webvr_surface_size_.width(), webvr_surface_size_.height());

  if (webxr_use_shared_buffer_draw_) {
    WebVrWaitForServerFence();
    CHECK(webxr_.HaveProcessingFrame());
    WebXrSharedBuffer* buffer =
        webxr_.GetProcessingFrame()->shared_buffer.get();
    CHECK(buffer);

    // Use an identity UV transform, the image is already oriented correctly.
    ui_->DrawWebVr(buffer->local_texture, kWebVrIdentityUvTransform, 0, 0);
  } else {
    // Apply the UV transform from the SurfaceTexture, that's usually a Y flip.
    ui_->DrawWebVr(webvr_texture_id_, webvr_surface_texture_uv_transform_, 0,
                   0);
  }
}

void VrShellGl::DrawContentQuad(bool draw_overlay_texture) {
  // Add a 2 pixel border to avoid aliasing issues at the edge of the texture.
  constexpr float kBorder = 2;
  TRACE_EVENT0("gpu", "VrShellGl::DrawContentQuad");
  // Don't need face culling, depth testing, blending, etc. Turn it all off.
  glDisable(GL_CULL_FACE);
  glDisable(GL_SCISSOR_TEST);
  glDisable(GL_POLYGON_OFFSET_FILL);
  glDisable(GL_BLEND);
  glClear(GL_COLOR_BUFFER_BIT);

  glViewport(
      content_tex_buffer_size_.width() * kContentVignetteBorder - kBorder,
      content_tex_buffer_size_.height() * kContentVignetteBorder - kBorder,
      content_tex_buffer_size_.width() + 2 * kBorder,
      content_tex_buffer_size_.height() + 2 * kBorder);
  ui_->DrawWebVr(content_texture_id_, kContentUvTransform,
                 kBorder / content_tex_buffer_size_.width(),
                 kBorder / content_tex_buffer_size_.height());
  if (draw_overlay_texture) {
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    ui_->DrawWebVr(content_overlay_texture_id_, kContentUvTransform,
                   kBorder / content_tex_buffer_size_.width(),
                   kBorder / content_tex_buffer_size_.height());
  }
}

void VrShellGl::OnPause() {
  vsync_helper_.CancelVSyncRequest();
  RenderLoop::OnPause();
  gvr_api_->PauseTracking();
  webvr_frame_timeout_.Cancel();
  webvr_spinner_timeout_.Cancel();
}

void VrShellGl::OnResume() {
  gvr_api_->RefreshViewerProfile();
  viewports_need_updating_ = true;
  gvr_api_->ResumeTracking();
  RenderLoop::OnResume();
  vsync_helper_.CancelVSyncRequest();
  OnVSync(base::TimeTicks::Now());
  if (web_vr_mode_)
    ScheduleOrCancelWebVrFrameTimeout();
}

void VrShellGl::OnExitPresent() {
  webvr_frame_timeout_.Cancel();
  webvr_spinner_timeout_.Cancel();
}

void VrShellGl::SetWebVrMode(bool enabled) {
  web_vr_mode_ = enabled;

  if (web_vr_mode_ && submit_client_) {
    ScheduleOrCancelWebVrFrameTimeout();
  } else {
    webvr_frame_timeout_.Cancel();
    webvr_frames_received_ = 0;
  }

  if (cardboard_)
    browser_->ToggleCardboardGamepad(enabled);

  if (!web_vr_mode_) {
    // Closing presentation bindings ensures we won't get any mojo calls such
    // as SubmitFrame from this session anymore. This makes it legal to cancel
    // an outstanding animating frame (if any).
    ClosePresentationBindings();

    // Ensure that re-entering VR later gets a fresh start by clearing out the
    // current session's animating frame state.
    webxr_.EndPresentation();
    // Do not clear pending_frames_ here, need to track Surface state across
    // sessions.
    if (!pending_frames_.empty()) {
      // There's a leftover pending frame. Need to wait for that to arrive on
      // the Surface, and that will clear webvr_frame_processing_ once it's
      // done. Until then, webvr_frame_processing_ will stay true to block a
      // new session from starting processing.
      // TODO(acondor): Move these DCHECKs into a unittest.
      DCHECK(webxr_.HaveProcessingFrame());
      DCHECK(webxr_.GetProcessingFrame()->state_locked);
      DCHECK(webxr_.GetProcessingFrame()->recycle_once_unlocked);
    }
  }
}

void VrShellGl::ContentBoundsChanged(int width, int height) {
  TRACE_EVENT0("gpu", "VrShellGl::ContentBoundsChanged");
  ui_->OnContentBoundsChanged(width, height);
}

void VrShellGl::BufferBoundsChanged(const gfx::Size& content_buffer_size,
                                    const gfx::Size& overlay_buffer_size) {
  content_tex_buffer_size_ = content_buffer_size;
}

base::WeakPtr<VrShellGl> VrShellGl::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

base::WeakPtr<BrowserUiInterface> VrShellGl::GetBrowserUiWeakPtr() {
  return ui_->GetBrowserUiWeakPtr();
}

bool VrShellGl::WebVrCanAnimateFrame(bool is_from_onvsync) {
  // This check needs to be first to ensure that we start the WebVR
  // first-frame timeout on presentation start.
  bool can_send_webvr_vsync = ui_->CanSendWebVrVSync();
  if (!webxr_.last_ui_allows_sending_vsync && can_send_webvr_vsync) {
    // We will start sending vsync to the WebVR page, so schedule the incoming
    // frame timeout.
    ScheduleOrCancelWebVrFrameTimeout();
  }
  webxr_.last_ui_allows_sending_vsync = can_send_webvr_vsync;
  if (!can_send_webvr_vsync) {
    DVLOG(2) << __func__ << ": waiting for can_send_webvr_vsync";
    return false;
  }

  // If we want to send vsync-aligned frames, we only allow animation to start
  // when called from OnVSync, so if we're called from somewhere else we can
  // skip all the other checks. Legacy Cardboard mode (not surfaceless) doesn't
  // use vsync aligned frames, and there's a flag to disable it for surfaceless
  // mode.
  if (surfaceless_rendering_ && webvr_vsync_align_ && !is_from_onvsync) {
    DVLOG(3) << __func__ << ": waiting for onvsync (vsync aligned)";
    return false;
  }

  if (!web_vr_mode_) {
    DVLOG(2) << __func__ << ": no active session, ignore";
    return false;
  }

  if (get_frame_data_callback_.is_null()) {
    DVLOG(2) << __func__ << ": waiting for get_frame_data_callback_";
    return false;
  }

  if (!pending_vsync_) {
    DVLOG(2) << __func__ << ": waiting for pending_vsync (too fast)";
    return false;
  }

  // If we already have a JS frame that's animating, don't send another one.
  // This check depends on the Renderer calling either SubmitFrame or
  // SubmitFrameMissing for each animated frame.
  if (webxr_.HaveAnimatingFrame()) {
    DVLOG(2) << __func__
             << ": waiting for current animating frame to start processing";
    return false;
  }

  if (webxr_use_shared_buffer_draw_ && !webxr_.mailbox_bridge_ready()) {
    // For exclusive scheduling, we need the mailbox bridge before the first
    // frame so that we can place a sync token. For shared buffer draw, we
    // need it to set up buffers before starting client rendering.
    DVLOG(2) << __func__ << ": waiting for mailbox_bridge_ready";
    return false;
  }

  if (webxr_use_shared_buffer_draw_ &&
      !(webvr_surface_size_.width() && webvr_surface_size_.height())) {
    // For shared buffer draw, wait for a nonzero size before creating
    // the shared buffer for use as a drawing destination.
    DVLOG(2) << __func__ << ": waiting for nonzero size";
    return false;
  }

  // Keep the heuristic tests last since they update a trace counter, they
  // should only be run if the remaining criteria are already met. There's no
  // corresponding WebVrTryStartAnimating call for this, the retries happen
  // via OnVSync.
  bool still_rendering = WebVrHasSlowRenderingFrame();
  bool overstuffed = WebVrHasOverstuffedBuffers();
  TRACE_COUNTER2("gpu", "WebVR frame skip", "still rendering", still_rendering,
                 "overstuffed", overstuffed);
  if (still_rendering || overstuffed) {
    DVLOG(2) << __func__ << ": waiting for backlogged frames,"
             << " still_rendering=" << still_rendering
             << " overstuffed=" << overstuffed;
    return false;
  }

  DVLOG(2) << __func__ << ": ready to animate frame";
  return true;
}

void VrShellGl::WebVrTryStartAnimatingFrame(bool is_from_onvsync) {
  if (WebVrCanAnimateFrame(is_from_onvsync)) {
    SendVSync();
  }
}

void VrShellGl::WebVrCancelAnimatingFrame() {
  DVLOG(2) << __func__;
  webxr_.RecycleUnusedAnimatingFrame();
  if (submit_client_) {
    // We haven't written to the Surface yet. Mark as transferred and rendered.
    submit_client_->OnSubmitFrameTransferred(true);
    WebVrSendRenderNotification(false);
  }
}

void VrShellGl::WebVrCancelProcessingFrameAfterTransfer() {
  DVLOG(2) << __func__;
  DCHECK(webxr_.HaveProcessingFrame());
  bool did_recycle = webxr_.RecycleProcessingFrameIfPossible();
  DCHECK(did_recycle);
  if (submit_client_) {
    // We've already sent the transferred notification.
    // Just report rendering complete.
    WebVrSendRenderNotification(false);
  }
}

void VrShellGl::OnVSync(base::TimeTicks frame_time) {
  TRACE_EVENT0("gpu", "VrShellGl::OnVSync");
  // Create a synthetic VSync trace event for the reported last-VSync time. Use
  // this specific type since it appears to be the only one which supports
  // supplying a timestamp different from the current time, which is useful
  // since we seem to be >1ms behind the vsync time when we receive this call.
  //
  // See third_party/catapult/tracing/tracing/extras/vsync/vsync_auditor.html
  std::unique_ptr<base::trace_event::TracedValue> args =
      std::make_unique<base::trace_event::TracedValue>();
  args->SetDouble(
      "frame_time_us",
      static_cast<double>((frame_time - base::TimeTicks()).InMicroseconds()));
  TRACE_EVENT_INSTANT1("viz", "DisplayScheduler::BeginFrame",
                       TRACE_EVENT_SCOPE_THREAD, "args", std::move(args));

  vsync_helper_.RequestVSync(
      base::BindRepeating(&VrShellGl::OnVSync, base::Unretained(this)));

  pending_vsync_ = true;
  pending_time_ = frame_time;
  WebVrTryStartAnimatingFrame(true);

  if (ShouldDrawWebVr()) {
    // When drawing WebVR, controller input doesn't need to be synchronized with
    // rendering as WebVR uses the gamepad api. To ensure we always handle input
    // like app button presses, process the controller here.
    device::GvrDelegate::GetGvrPoseWithNeckModel(gvr_api_.get(),
                                                 &render_info_.head_pose);
    input_states_.push_back(
        ProcessControllerInputForWebXr(render_info_, frame_time));
  } else {
    DrawFrame(-1, frame_time);
  }
}

void VrShellGl::GetFrameData(
    device::mojom::XRFrameDataProvider::GetFrameDataCallback callback) {
  TRACE_EVENT0("gpu", __func__);
  if (!get_frame_data_callback_.is_null()) {
    DLOG(WARNING) << ": previous get_frame_data_callback_ was not used yet";
    mojo::ReportBadMessage(
        "Requested VSync before waiting for response to previous request.");
    ClosePresentationBindings();
    return;
  }

  get_frame_data_callback_ = std::move(callback);
  WebVrTryStartAnimatingFrame(false);
}

namespace {
bool ValidateRect(const gfx::RectF& bounds) {
  // Bounds should be between 0 and 1, with positive width/height.
  // We simply clamp to [0,1], but still validate that the bounds are not NAN.
  return !std::isnan(bounds.width()) && !std::isnan(bounds.height()) &&
         !std::isnan(bounds.x()) && !std::isnan(bounds.y());
}

}  // namespace

void VrShellGl::UpdateLayerBounds(int16_t frame_index,
                                  const gfx::RectF& left_bounds,
                                  const gfx::RectF& right_bounds,
                                  const gfx::Size& source_size) {
  if (!ValidateRect(left_bounds) || !ValidateRect(right_bounds)) {
    mojo::ReportBadMessage("UpdateLayerBounds called with invalid bounds");
    presentation_binding_.Close();
    frame_data_binding_.Close();
    return;
  }

  if (frame_index >= 0 && !webxr_.HaveAnimatingFrame()) {
    // The optional UpdateLayerBounds call must happen before SubmitFrame.
    mojo::ReportBadMessage("UpdateLayerBounds called without animating frame");
    presentation_binding_.Close();
    frame_data_binding_.Close();
    return;
  }

  if (frame_index < 0) {
    current_webvr_frame_bounds_ =
        WebVrBounds(left_bounds, right_bounds, source_size);
    webvr_viewport_.left.SetSourceUv(UVFromGfxRect(ClampRect(left_bounds)));
    webvr_viewport_.right.SetSourceUv(UVFromGfxRect(ClampRect(right_bounds)));
    CreateOrResizeWebVRSurface(source_size);

    // clear all pending bounds
    pending_bounds_ = base::queue<
        std::pair<WebXrPresentationState::FrameIndexType, WebVrBounds>>();
  } else {
    pending_bounds_.emplace(
        frame_index, WebVrBounds(left_bounds, right_bounds, source_size));
  }
}

base::TimeDelta VrShellGl::GetPredictedFrameTime() {
  base::TimeDelta frame_interval = vsync_helper_.DisplayVSyncInterval();
  // If we aim to submit at vsync, that frame will start scanning out
  // one vsync later. Add a half frame to split the difference between
  // left and right eye.
  base::TimeDelta js_time = webvr_js_time_.GetAverageOrDefault(frame_interval);
  base::TimeDelta render_time =
      webvr_render_time_.GetAverageOrDefault(frame_interval);
  base::TimeDelta overhead_time = frame_interval * 3 / 2;
  base::TimeDelta expected_frame_time = js_time + render_time + overhead_time;
  TRACE_COUNTER2("gpu", "WebVR frame time (ms)", "javascript",
                 js_time.InMilliseconds(), "rendering",
                 render_time.InMilliseconds());
  TRACE_COUNTER2("gpu", "GVR frame time (ms)", "acquire",
                 webvr_acquire_time_.GetAverage().InMilliseconds(), "submit",
                 webvr_submit_time_.GetAverage().InMilliseconds());
  TRACE_COUNTER1("gpu", "WebVR pose prediction (ms)",
                 expected_frame_time.InMilliseconds());
  return expected_frame_time;
}

bool VrShellGl::WebVrHasSlowRenderingFrame() {
  // Disable heuristic for traditional render path where we submit completed
  // frames.
  if (!webxr_use_gpu_fence_)
    return false;

  base::TimeDelta frame_interval = vsync_helper_.DisplayVSyncInterval();
  base::TimeDelta mean_render_time =
      webvr_render_time_.GetAverageOrDefault(frame_interval);

  // Check estimated completion of the rendering frame, that's two frames back.
  // It might not exist, i.e. for the first couple of frames when starting
  // presentation, or if the app failed to submit a frame in its rAF loop.
  // Also, AddWebVrRenderTimeEstimate zeroes the submit time once the rendered
  // frame is complete. In all of those cases, we don't need to wait for render
  // completion.
  if (webxr_.HaveRenderingFrame() && webxr_.HaveProcessingFrame()) {
    base::TimeTicks prev_js_submit = webxr_.GetRenderingFrame()->time_js_submit;
    base::TimeDelta mean_js_time = webvr_js_time_.GetAverage();
    base::TimeDelta mean_js_wait = webvr_js_wait_time_.GetAverage();
    base::TimeDelta prev_render_time_left =
        mean_render_time - (base::TimeTicks::Now() - prev_js_submit);
    // We don't want the next animating frame to arrive too early. Estimated
    // time-to-submit is the net JavaScript time, not counting time spent
    // waiting. JS is blocked from submitting if the rendering frame (two
    // frames back) is not complete yet, so there's no point submitting earlier
    // than that. There's also a processing frame (one frame back), so we have
    // at least a VSync interval spare time after that. Aim for submitting 3/4
    // of a VSync interval after the rendering frame completes to keep a bit of
    // safety margin. We're currently scheduling at VSync granularity, so skip
    // this VSync if we'd arrive a full VSync interval early.
    if (mean_js_time - mean_js_wait + frame_interval <
        prev_render_time_left + frame_interval * 3 / 4) {
      return true;
    }
  }
  return false;
}

bool VrShellGl::WebVrHasOverstuffedBuffers() {
  base::TimeDelta frame_interval = vsync_helper_.DisplayVSyncInterval();
  base::TimeDelta mean_render_time =
      webvr_render_time_.GetAverageOrDefault(frame_interval);

  if (webvr_unstuff_ratelimit_frames_ > 0) {
    --webvr_unstuff_ratelimit_frames_;
  } else if (webvr_acquire_time_.GetAverage() >= kWebVrSlowAcquireThreshold &&
             mean_render_time < frame_interval) {
    // This is a fast app with average render time less than the frame
    // interval. If GVR acquire is slow, that means its internal swap chain was
    // already full when we tried to give it the next frame. We can skip a
    // SendVSync to drain one frame from the GVR queue. That should reduce
    // latency by one frame.
    webvr_unstuff_ratelimit_frames_ = kWebVrUnstuffMaxDropRate;
    return true;
  }
  return false;
}

void VrShellGl::SendVSync() {
  DCHECK(!get_frame_data_callback_.is_null());
  DCHECK(pending_vsync_);

  // Mark the VSync as consumed.
  pending_vsync_ = false;

  device::mojom::XRFrameDataPtr frame_data = device::mojom::XRFrameData::New();

  // The internal frame index is an uint8_t that generates a wrapping 0.255
  // frame number. We store it in an int16_t to match mojo APIs, and to avoid
  // it appearing as a char in debug logs.
  frame_data->frame_id = webxr_.StartFrameAnimating();
  DVLOG(2) << __func__ << " frame=" << frame_data->frame_id;

  if (webxr_use_shared_buffer_draw_) {
    WebVrPrepareSharedBuffer(webvr_surface_size_);
  }

  if (webxr_use_shared_buffer_draw_) {
    CHECK(webxr_.mailbox_bridge_ready());
    CHECK(webxr_.HaveAnimatingFrame());
    WebXrSharedBuffer* buffer = webxr_.GetAnimatingFrame()->shared_buffer.get();
    DCHECK(buffer);
    frame_data->buffer_holder = *buffer->mailbox_holder;
  }

  int64_t prediction_nanos = GetPredictedFrameTime().InMicroseconds() * 1000;

  gfx::Transform head_mat;
  TRACE_EVENT_BEGIN0("gpu", "VrShellGl::GetVRPosePtrWithNeckModel");
  device::mojom::VRPosePtr pose =
      device::GvrDelegate::GetVRPosePtrWithNeckModel(gvr_api_.get(), &head_mat,
                                                     prediction_nanos);
  TRACE_EVENT_END0("gpu", "VrShellGl::GetVRPosePtrWithNeckModel");

  // Process all events. Check for ones we wish to react to.
  gvr::Event last_event;
  while (gvr_api_->PollEvent(&last_event)) {
    pose->pose_reset |= last_event.type == GVR_EVENT_RECENTER;
  }

  TRACE_EVENT0("gpu", "VrShellGl::XRInput");
  if (cardboard_) {
    std::vector<device::mojom::XRInputSourceStatePtr> input_states;
    input_states.push_back(GetGazeInputSourceState());
    pose->input_state = std::move(input_states);
  } else {
    pose->input_state = std::move(input_states_);
  }

  frame_data->pose = std::move(pose);

  WebXrFrame* frame = webxr_.GetAnimatingFrame();
  frame->head_pose = head_mat;
  frame->time_pose = base::TimeTicks::Now();

  frame_data->time_delta = pending_time_ - base::TimeTicks();

  TRACE_EVENT0("gpu", "VrShellGl::RunCallback");
  base::ResetAndReturn(&get_frame_data_callback_).Run(std::move(frame_data));
}

void VrShellGl::ClosePresentationBindings() {
  webvr_frame_timeout_.Cancel();
  submit_client_.reset();
  if (!get_frame_data_callback_.is_null()) {
    // When this Presentation provider is going away we have to respond to
    // pending callbacks, so instead of providing a VSync, tell the requester
    // the connection is closing.
    base::ResetAndReturn(&get_frame_data_callback_).Run(nullptr);
  }
  presentation_binding_.Close();
  frame_data_binding_.Close();
}

device::mojom::XRInputSourceStatePtr VrShellGl::GetGazeInputSourceState() {
  device::mojom::XRInputSourceStatePtr state =
      device::mojom::XRInputSourceState::New();

  // Only one gaze input source to worry about, so it can have a static id.
  state->source_id = 1;

  // Report any trigger state changes made since the last call and reset the
  // state here.
  state->primary_input_pressed = cardboard_trigger_pressed_;
  state->primary_input_clicked = cardboard_trigger_clicked_;
  cardboard_trigger_clicked_ = false;

  state->description = device::mojom::XRInputSourceDescription::New();

  // It's a gaze-cursor-based device.
  state->description->target_ray_mode = device::mojom::XRTargetRayMode::GAZING;
  state->description->emulated_position = true;

  // No implicit handedness
  state->description->handedness = device::mojom::XRHandedness::NONE;

  // Pointer and grip transforms are omitted since this is a gaze-based source.

  return state;
}

void VrShellGl::OnTriggerEvent(bool pressed) {
  if (pressed) {
    cardboard_trigger_pressed_ = true;
  } else if (cardboard_trigger_pressed_) {
    cardboard_trigger_pressed_ = false;
    cardboard_trigger_clicked_ = true;
  }
}

void VrShellGl::AcceptDoffPromptForTesting() {
  ui_->AcceptDoffPromptForTesting();
}

}  // namespace vr
