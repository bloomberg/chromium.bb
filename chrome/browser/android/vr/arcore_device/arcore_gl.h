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
class GLSurface;
}  // namespace gl

namespace vr {
class MailboxToSurfaceBridge;
}  // namespace vr

namespace device {

class ARCore;
struct ARCoreHitTestRequest;
class ARImageTransport;

// All of this class's methods must be called on the same valid GL thread with
// the exception of GetGlThreadTaskRunner() and GetWeakPtr().
class ARCoreGl {
 public:
  explicit ARCoreGl(std::unique_ptr<vr::MailboxToSurfaceBridge> mailbox_bridge);
  ~ARCoreGl();

  void Initialize(base::OnceCallback<void(bool)> callback);

  void ProduceFrame(const gfx::Size& frame_size,
                    display::Display::Rotation display_rotation,
                    mojom::VRMagicWindowProvider::GetFrameDataCallback);
  void Pause();
  void Resume();

  const scoped_refptr<base::SingleThreadTaskRunner>& GetGlThreadTaskRunner() {
    return gl_thread_task_runner_;
  }

  void RequestHitTest(mojom::XRRayPtr,
                      mojom::VRMagicWindowProvider::RequestHitTestCallback);

  base::WeakPtr<ARCoreGl> GetWeakPtr();

 private:
  // TODO(https://crbug/835948): remove frame_size.
  void ProcessFrame(
      mojom::XRFrameDataPtr frame_data,
      const gfx::Size& frame_size,
      mojom::VRMagicWindowProvider::GetFrameDataCallback callback);

  bool InitializeGl();
  bool IsOnGlThread() const;

  scoped_refptr<gl::GLSurface> surface_;
  scoped_refptr<gl::GLContext> context_;
  scoped_refptr<base::SingleThreadTaskRunner> gl_thread_task_runner_;

  // Created on GL thread and should only be accessed on that thread.
  std::unique_ptr<ARCore> arcore_;
  std::unique_ptr<ARImageTransport> ar_image_transport_;

  // Default dummy values to ensure consistent behaviour.
  gfx::Size transfer_size_ = gfx::Size(0, 0);
  display::Display::Rotation display_rotation_ = display::Display::ROTATE_0;

  gfx::Transform uv_transform_;
  gfx::Transform projection_;
  // The first run of ProduceFrame should set uv_transform_ and projection_
  // using the default settings in ARCore.
  bool should_recalculate_uvs_ = true;

  bool is_initialized_ = false;

  vr::FPSMeter fps_meter_;

  std::vector<std::unique_ptr<ARCoreHitTestRequest>> hit_test_requests_;

  // Must be last.
  base::WeakPtrFactory<ARCoreGl> weak_ptr_factory_;
  DISALLOW_COPY_AND_ASSIGN(ARCoreGl);
};

}  // namespace device

#endif  // CHROME_BROWSER_ANDROID_VR_ARCORE_DEVICE_ARCORE_GL_H_
