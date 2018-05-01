// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_VR_ARCORE_DEVICE_ARCORE_GL_H_
#define CHROME_BROWSER_ANDROID_VR_ARCORE_DEVICE_ARCORE_GL_H_

#include <memory>
#include <utility>
#include <vector>
#include "base/cancelable_callback.h"
#include "base/containers/queue.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "chrome/browser/android/vr/mailbox_to_surface_bridge.h"
#include "chrome/browser/vr/fps_meter.h"
#include "chrome/browser/vr/renderers/web_vr_renderer.h"
#include "device/vr/public/mojom/vr_service.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "ui/display/display.h"
#include "ui/gfx/geometry/quaternion.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size_f.h"
#include "ui/gfx/native_widget_types.h"

namespace gl {
class GLContext;
class GLImageEGL;
class GLSurface;
}  // namespace gl

namespace gpu {
struct MailboxHolder;
class GpuMemoryBufferImplAndroidHardwareBuffer;
}  // namespace gpu

namespace device {
class ARCoreDevice;
class ARCoreInterface;

// TODO(klausw): share this with VrShellGl
struct WebXrSharedBuffer {
  WebXrSharedBuffer();
  ~WebXrSharedBuffer();

  gfx::Size size = {0, 0};

  // Shared GpuMemoryBuffer
  std::unique_ptr<gpu::GpuMemoryBufferImplAndroidHardwareBuffer> gmb;

  // Resources in the remote GPU process command buffer context
  std::unique_ptr<gpu::MailboxHolder> mailbox_holder;
  GLuint remote_texture_id = 0;
  GLuint remote_image_id = 0;

  // Resources in the local GL context
  GLuint local_texture_id = 0;
  // This refptr keeps the image alive while processing a frame. That's
  // required because it owns underlying resources, and must still be
  // alive when the mailbox texture backed by this image is used.
  scoped_refptr<gl::GLImageEGL> local_glimage;
};

struct WebXrSwapChain {
  WebXrSwapChain();
  ~WebXrSwapChain();

  base::queue<std::unique_ptr<WebXrSharedBuffer>> buffers;
  int next_memory_buffer_id = 0;
};

// This class manages all GLThread owned objects and GL update for ARCore.
// It is not threadsafe and most of these methods must only be called on the
// GL thread (except where marked otherwise with a comment).
class ARCoreGl {
 public:
  ARCoreGl(ARCoreDevice* arcore_device,
           std::unique_ptr<vr::MailboxToSurfaceBridge> mailbox_bridge,
           scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner);
  ~ARCoreGl();

  // Initialize() must be called on a valid GL thread.
  void Initialize();
  bool IsInitialized() { return is_initialized_; }

  // RequestFrame can be called from any thread as it schedules
  // ProduceCameraFrame.
  void RequestFrame(const gfx::Size& frame_size,
                    display::Display::Rotation frame_rotation,
                    mojom::VRMagicWindowProvider::GetFrameDataCallback);

  base::WeakPtr<ARCoreGl> GetWeakPtr();

 private:
  void InitializeGl();
  void SetupHardwareBuffers();
  void ResizeSharedBuffer(WebXrSharedBuffer* buffer, const gfx::Size& size);
  void MakeUvTransformMatrixFromSampleData(float (&mat_out)[16],
                                           const float (&uv_in)[6]);
  void ProduceCameraFrame(const gfx::Size& frame_size,
                          display::Display::Rotation display_rotation,
                          mojom::VRMagicWindowProvider::GetFrameDataCallback);
  bool IsOnGlThread() const;
  // TODO(https://crbug.com/838013): rename WebVRRenderer.
  std::unique_ptr<vr::WebVrRenderer> web_vr_renderer_;
  // samplerExternalOES texture data for WebVR content image.
  GLuint camera_texture_id_arcore_ = 0;
  GLuint camera_fbo_ = 0;
  GLuint transfer_fbo_ = 0;
  bool transfer_fbo_completeness_checked_ = false;
  scoped_refptr<gl::GLSurface> surface_;
  scoped_refptr<gl::GLContext> context_;
  ARCoreDevice* arcore_device_;
  scoped_refptr<base::SingleThreadTaskRunner> gl_thread_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner_;
  std::unique_ptr<vr::MailboxToSurfaceBridge> mailbox_bridge_;

  WebXrSwapChain swap_chain_;

  vr::FPSMeter webxr_fps_meter_;

  std::unique_ptr<ARCoreInterface> arcore_interface_;

  bool is_initialized_ = false;

  // Must be last.
  base::WeakPtrFactory<ARCoreGl> weak_ptr_factory_;
  DISALLOW_COPY_AND_ASSIGN(ARCoreGl);
};

}  // namespace device

#endif  // CHROME_BROWSER_ANDROID_VR_ARCORE_DEVICE_ARCORE_GL_H_
