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
#include "device/vr/public/mojom/isolated_xr_service.mojom.h"
#include "device/vr/public/mojom/vr_service.mojom.h"
#include "device/vr/util/fps_meter.h"
#include "mojo/public/cpp/bindings/associated_binding.h"
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
class ArCoreInstallUtils;
}  // namespace vr

namespace device {

class ArCore;
class ArCoreFactory;
struct ArCoreHitTestRequest;
class ArImageTransport;

using ArCoreGlCreateSessionCallback = base::OnceCallback<void(
    mojom::XRFrameDataProviderPtrInfo frame_data_provider_info,
    mojom::VRDisplayInfoPtr display_info,
    mojom::XRSessionControllerPtrInfo session_controller_info,
    mojom::XRRuntime::RequestSessionCallback deferred_callback)>;

// All of this class's methods must be called on the same valid GL thread with
// the exception of GetGlThreadTaskRunner() and GetWeakPtr().
class ArCoreGl : public mojom::XRFrameDataProvider,
                 public mojom::XREnvironmentIntegrationProvider,
                 public mojom::XRSessionController {
 public:
  explicit ArCoreGl(std::unique_ptr<ArImageTransport> ar_image_transport);
  ~ArCoreGl() override;

  void Initialize(vr::ArCoreInstallUtils* install_utils,
                  ArCoreFactory* arcore_factory,
                  base::OnceCallback<void(bool)> callback);

  void CreateSession(mojom::VRDisplayInfoPtr display_info,
                     mojom::XRRuntime::RequestSessionCallback deferred_callback,
                     ArCoreGlCreateSessionCallback callback);

  const scoped_refptr<base::SingleThreadTaskRunner>& GetGlThreadTaskRunner() {
    return gl_thread_task_runner_;
  }

  // mojom::XRFrameDataProvider
  void GetFrameData(GetFrameDataCallback callback) override;

  void GetEnvironmentIntegrationProvider(
      mojom::XREnvironmentIntegrationProviderAssociatedRequest
          environment_provider) override;

  // mojom::XREnvironmentIntegrationProvider
  void UpdateSessionGeometry(
      const gfx::Size& frame_size,
      display::Display::Rotation display_rotation) override;

  void RequestHitTest(
      mojom::XRRayPtr,
      mojom::XREnvironmentIntegrationProvider::RequestHitTestCallback) override;

  // mojom::XRSessionController
  void SetFrameDataRestricted(bool restricted) override;

  base::WeakPtr<ArCoreGl> GetWeakPtr();

 private:
  void Pause();
  void Resume();

  void ProcessFrame(mojom::XRFrameDataPtr frame_data,
                    mojom::XRFrameDataProvider::GetFrameDataCallback callback);

  bool InitializeGl();
  bool IsOnGlThread() const;

  scoped_refptr<gl::GLSurface> surface_;
  scoped_refptr<gl::GLContext> context_;
  scoped_refptr<base::SingleThreadTaskRunner> gl_thread_task_runner_;

  // Created on GL thread and should only be accessed on that thread.
  std::unique_ptr<ArCore> arcore_;
  std::unique_ptr<ArImageTransport> ar_image_transport_;

  // Default dummy values to ensure consistent behaviour.
  gfx::Size transfer_size_ = gfx::Size(0, 0);
  display::Display::Rotation display_rotation_ = display::Display::ROTATE_0;
  bool should_update_display_geometry_ = true;

  gfx::Transform uv_transform_;
  gfx::Transform projection_;
  // The first run of ProduceFrame should set uv_transform_ and projection_
  // using the default settings in ArCore.
  bool should_recalculate_uvs_ = true;

  bool is_initialized_ = false;

  bool restrict_frame_data_ = false;

  FPSMeter fps_meter_;

  std::vector<std::unique_ptr<ArCoreHitTestRequest>> hit_test_requests_;

  mojo::Binding<mojom::XRFrameDataProvider> frame_data_binding_;
  mojo::Binding<mojom::XRSessionController> session_controller_binding_;
  mojo::AssociatedBinding<mojom::XREnvironmentIntegrationProvider>
      environment_binding_;

  // Must be last.
  base::WeakPtrFactory<ArCoreGl> weak_ptr_factory_;
  DISALLOW_COPY_AND_ASSIGN(ArCoreGl);
};

}  // namespace device

#endif  // CHROME_BROWSER_ANDROID_VR_ARCORE_DEVICE_ARCORE_GL_H_
