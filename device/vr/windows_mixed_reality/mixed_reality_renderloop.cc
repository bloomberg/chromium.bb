// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/windows_mixed_reality/mixed_reality_renderloop.h"

#include <HolographicSpaceInterop.h>
#include <SpatialInteractionManagerInterop.h>
#include <Windows.Graphics.DirectX.Direct3D11.interop.h>
#include <windows.graphics.holographic.h>
#include <windows.perception.h>
#include <windows.perception.spatial.h>
#include <windows.ui.input.spatial.h>

#include <algorithm>
#include <utility>
#include <vector>

#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/trace_event/common/trace_event_common.h"
#include "base/win/com_init_util.h"
#include "base/win/core_winrt_util.h"
#include "base/win/scoped_hstring.h"
#include "device/vr/windows/d3d11_texture_helper.h"
#include "ui/gfx/geometry/angle_conversions.h"
#include "ui/gfx/geometry/vector3d_f.h"
#include "ui/gfx/transform.h"
#include "ui/gfx/transform_util.h"

namespace device {

// TODO(crbug.com/941546): Remove namespaces to comply with coding standard.
using namespace ABI::Windows::Foundation;
using namespace ABI::Windows::Graphics::Holographic;
using namespace ABI::Windows::Perception;
using namespace ABI::Windows::Perception::Spatial;
using namespace Microsoft::WRL;
using namespace Microsoft::WRL::Wrappers;

namespace WFC = ABI::Windows::Foundation::Collections;
namespace WFN = ABI::Windows::Foundation::Numerics;
namespace WInput = ABI::Windows::UI::Input::Spatial;

class MixedRealityWindow : public gfx::WindowImpl {
  BOOL ProcessWindowMessage(HWND window,
                            UINT message,
                            WPARAM w_param,
                            LPARAM l_param,
                            LRESULT& result,
                            DWORD msg_map_id) override;
};

BOOL MixedRealityWindow::ProcessWindowMessage(HWND window,
                                              UINT message,
                                              WPARAM w_param,
                                              LPARAM l_param,
                                              LRESULT& result,
                                              DWORD msg_map_id) {
  return false;  // We don't currently handle any messages ourselves.
}

namespace {
mojom::VRFieldOfViewPtr ParseProjection(
    const ABI::Windows::Foundation::Numerics::Matrix4x4& projection) {
  gfx::Transform proj(
      projection.M11, projection.M21, projection.M31, projection.M41,
      projection.M12, projection.M22, projection.M32, projection.M42,
      projection.M13, projection.M23, projection.M33, projection.M43,
      projection.M14, projection.M24, projection.M34, projection.M44);

  gfx::Transform projInv;
  bool invertable = proj.GetInverse(&projInv);
  DCHECK(invertable);

  // We will convert several points from projection space into view space to
  // calculate the view frustum angles.  We are assuming some common form for
  // the projection matrix.
  gfx::Point3F left_top_far(-1, 1, 1);
  gfx::Point3F left_top_near(-1, 1, 0);
  gfx::Point3F right_bottom_far(1, -1, 1);
  gfx::Point3F right_bottom_near(1, -1, 0);

  projInv.TransformPoint(&left_top_far);
  projInv.TransformPoint(&left_top_near);
  projInv.TransformPoint(&right_bottom_far);
  projInv.TransformPoint(&right_bottom_near);

  float left_on_far_plane = left_top_far.x();
  float top_on_far_plane = left_top_far.y();
  float right_on_far_plane = right_bottom_far.x();
  float bottom_on_far_plane = right_bottom_far.y();
  float far_plane = left_top_far.z();

  mojom::VRFieldOfViewPtr field_of_view = mojom::VRFieldOfView::New();
  field_of_view->upDegrees =
      gfx::RadToDeg(atanf(-top_on_far_plane / far_plane));
  field_of_view->downDegrees =
      gfx::RadToDeg(atanf(bottom_on_far_plane / far_plane));
  field_of_view->leftDegrees =
      gfx::RadToDeg(atanf(left_on_far_plane / far_plane));
  field_of_view->rightDegrees =
      gfx::RadToDeg(atanf(-right_on_far_plane / far_plane));

  // TODO(billorr): Expand the mojo interface to support just sending the
  // projection matrix directly, instead of decomposing it.
  return field_of_view;
}

gfx::Transform CreateTransform(WFN::Vector3 position,
                               WFN::Quaternion rotation) {
  gfx::DecomposedTransform decomposed_transform;
  decomposed_transform.translate[0] = position.X;
  decomposed_transform.translate[1] = position.Y;
  decomposed_transform.translate[2] = position.Z;

  decomposed_transform.quaternion =
      gfx::Quaternion(rotation.X, rotation.Y, rotation.Z, rotation.W);
  return gfx::ComposeTransform(decomposed_transform);
}

bool TryGetGripFromLocation(
    ComPtr<WInput::ISpatialInteractionSourceLocation> location_in_origin,
    gfx::Transform* origin_from_grip) {
  DCHECK(origin_from_grip);
  *origin_from_grip = gfx::Transform();

  if (!location_in_origin)
    return false;

  ComPtr<IReference<WFN::Vector3>> pos_ref;

  if (FAILED(location_in_origin->get_Position(&pos_ref)) || !pos_ref)
    return false;

  WFN::Vector3 pos;
  if (FAILED(pos_ref->get_Value(&pos)))
    return false;

  ComPtr<WInput::ISpatialInteractionSourceLocation2> location_in_origin2;
  if (FAILED(location_in_origin.As(&location_in_origin2)))
    return false;

  ComPtr<IReference<WFN::Quaternion>> quat_ref;
  if (FAILED(location_in_origin2->get_Orientation(&quat_ref)) || !quat_ref)
    return false;

  WFN::Quaternion quat;
  if (FAILED(quat_ref->get_Value(&quat)))
    return false;

  *origin_from_grip = CreateTransform(pos, quat);
  return true;
}

bool TryGetPointerOffsetFromLocation(
    ComPtr<WInput::ISpatialInteractionSourceLocation> location_in_origin,
    gfx::Transform origin_from_grip,
    gfx::Transform* grip_from_pointer) {
  DCHECK(grip_from_pointer);
  *grip_from_pointer = gfx::Transform();

  if (!location_in_origin)
    return false;

  // We can get the pointer position, but we'll need to transform it to an
  // offset from the grip position.  If we can't get an inverse of that,
  // then go ahead and bail early.
  gfx::Transform grip_from_origin;
  if (!origin_from_grip.GetInverse(&grip_from_origin))
    return false;

  ComPtr<WInput::ISpatialInteractionSourceLocation3> location_in_origin3;
  if (FAILED(location_in_origin.As(&location_in_origin3)))
    return false;

  ComPtr<WInput::ISpatialPointerInteractionSourcePose> pointer_pose;
  if (FAILED(location_in_origin3->get_SourcePointerPose(&pointer_pose)) ||
      !pointer_pose)
    return false;

  WFN::Vector3 pos;
  if (FAILED(pointer_pose->get_Position(&pos)))
    return false;

  ComPtr<WInput::ISpatialPointerInteractionSourcePose2> pose2;
  if (FAILED(pointer_pose.As(&pose2)))
    return false;

  WFN::Quaternion rot;
  if (FAILED(pose2->get_Orientation(&rot)))
    return false;

  gfx::Transform origin_from_pointer = CreateTransform(pos, rot);
  *grip_from_pointer = (grip_from_origin * origin_from_pointer);
  return true;
}
}  // namespace

MixedRealityRenderLoop::MixedRealityRenderLoop(
    base::RepeatingCallback<void(mojom::VRDisplayInfoPtr)>
        on_display_info_changed)
    : XRCompositorCommon(),
      on_display_info_changed_(std::move(on_display_info_changed)) {}

MixedRealityRenderLoop::~MixedRealityRenderLoop() {
  Stop();
}

bool MixedRealityRenderLoop::PreComposite() {
  if (rendering_params_) {
    Microsoft::WRL::ComPtr<
        ABI::Windows::Graphics::DirectX::Direct3D11::IDirect3DSurface>
        surface;
    if (FAILED(rendering_params_->get_Direct3D11BackBuffer(&surface)))
      return false;
    Microsoft::WRL::ComPtr<
        Windows::Graphics::DirectX::Direct3D11::IDirect3DDxgiInterfaceAccess>
        dxgi_interface_access;
    if (FAILED(surface.As(&dxgi_interface_access)))
      return false;
    Microsoft::WRL::ComPtr<ID3D11Resource> native_resource;
    if (FAILED(dxgi_interface_access->GetInterface(
            IID_PPV_ARGS(&native_resource))))
      return false;

    Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
    if (FAILED(native_resource.As(&texture)))
      return false;
    texture_helper_.SetBackbuffer(texture);
    D3D11_TEXTURE2D_DESC desc;
    texture->GetDesc(&desc);

    ABI::Windows::Foundation::Rect viewport;
    if (FAILED(pose_->get_Viewport(&viewport)))
      return false;

    gfx::RectF override_viewport =
        gfx::RectF(viewport.X / desc.Width, viewport.Y / desc.Height,
                   viewport.Width / desc.Width, viewport.Height / desc.Height);

    texture_helper_.OverrideViewports(override_viewport, override_viewport);
    texture_helper_.SetDefaultSize(gfx::Size(desc.Width, desc.Height));
  }
  return true;
}

bool MixedRealityRenderLoop::SubmitCompositedFrame() {
  ABI::Windows::Graphics::Holographic::HolographicFramePresentResult result;
  if (FAILED(holographic_frame_->PresentUsingCurrentPrediction(&result)))
    return false;
  texture_helper_.DiscardView();
  return true;
}

namespace {

FARPROC LoadD3D11Function(const char* function_name) {
  static HMODULE const handle = ::LoadLibrary(L"d3d11.dll");
  return handle ? ::GetProcAddress(handle, function_name) : nullptr;
}

decltype(&::CreateDirect3D11DeviceFromDXGIDevice)
GetCreateDirect3D11DeviceFromDXGIDeviceFunction() {
  static decltype(&::CreateDirect3D11DeviceFromDXGIDevice) const function =
      reinterpret_cast<decltype(&::CreateDirect3D11DeviceFromDXGIDevice)>(
          LoadD3D11Function("CreateDirect3D11DeviceFromDXGIDevice"));
  return function;
}

HRESULT WrapperCreateDirect3D11DeviceFromDXGIDevice(IDXGIDevice* in,
                                                    IInspectable** out) {
  *out = nullptr;
  auto func = GetCreateDirect3D11DeviceFromDXGIDeviceFunction();
  if (!func)
    return E_FAIL;
  return func(in, out);
}

}  // namespace

bool MixedRealityRenderLoop::StartRuntime() {
  initializer_ = std::make_unique<base::win::ScopedWinrtInitializer>();

  InitializeSpace();
  if (!holographic_space_)
    return false;

  ABI::Windows::Graphics::Holographic::HolographicAdapterId id;
  HRESULT hr = holographic_space_->get_PrimaryAdapterId(&id);
  if (FAILED(hr))
    return false;

  LUID adapter_luid;
  adapter_luid.HighPart = id.HighPart;
  adapter_luid.LowPart = id.LowPart;
  texture_helper_.SetUseBGRA(true);
  if (!texture_helper_.SetAdapterLUID(adapter_luid) ||
      !texture_helper_.EnsureInitialized()) {
    return false;
  }

  // Associate our holographic space with our directx device.
  ComPtr<IDXGIDevice> dxgi_device;
  hr = texture_helper_.GetDevice().As(&dxgi_device);
  if (FAILED(hr))
    return false;

  ComPtr<IInspectable> spInsp;
  hr = WrapperCreateDirect3D11DeviceFromDXGIDevice(dxgi_device.Get(), &spInsp);
  if (FAILED(hr))
    return false;

  ComPtr<ABI::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice> device;
  hr = spInsp.As(&device);
  if (FAILED(hr))
    return false;

  hr = holographic_space_->SetDirect3D11Device(device.Get());
  return SUCCEEDED(hr);
}

void MixedRealityRenderLoop::StopRuntime() {
  if (window_)
    ShowWindow(window_->hwnd(), SW_HIDE);
  holographic_space_ = nullptr;
  origin_ = nullptr;

  holographic_frame_ = nullptr;
  timestamp_ = nullptr;
  poses_ = nullptr;
  pose_ = nullptr;
  rendering_params_ = nullptr;
  camera_ = nullptr;

  if (window_)
    DestroyWindow(window_->hwnd());
  window_ = nullptr;

  if (initializer_)
    initializer_ = nullptr;
}

void MixedRealityRenderLoop::InitializeOrigin() {
  // Try to use a SpatialStageFrameOfReference.
  ComPtr<ISpatialStageFrameOfReferenceStatics> spatial_stage_statics;
  base::win::ScopedHString spatial_stage_string =
      base::win::ScopedHString::Create(
          RuntimeClass_Windows_Perception_Spatial_SpatialStageFrameOfReference);
  HRESULT hr = base::win::RoGetActivationFactory(
      spatial_stage_string.get(), IID_PPV_ARGS(&spatial_stage_statics));
  if (SUCCEEDED(hr)) {
    ComPtr<ISpatialStageFrameOfReference> spatial_stage;
    hr = spatial_stage_statics->get_Current(&spatial_stage);
    if (SUCCEEDED(hr) && spatial_stage) {
      hr = spatial_stage->get_CoordinateSystem(&origin_);
      if (SUCCEEDED(hr))
        return;
    }
  }

  // Failed to get a Stage frame of reference - try to get a stationary frame.
  ComPtr<ISpatialLocatorStatics> spatial_locator_statics;
  base::win::ScopedHString spatial_locator_string =
      base::win::ScopedHString::Create(
          RuntimeClass_Windows_Perception_Spatial_SpatialLocator);
  hr = base::win::RoGetActivationFactory(
      spatial_locator_string.get(), IID_PPV_ARGS(&spatial_locator_statics));
  if (FAILED(hr))
    return;

  ComPtr<ISpatialLocator> locator;
  hr = spatial_locator_statics->GetDefault(&locator);
  if (FAILED(hr))
    return;

  ComPtr<ISpatialStationaryFrameOfReference> stationary_frame;
  hr = locator->CreateStationaryFrameOfReferenceAtCurrentLocation(
      &stationary_frame);
  if (FAILED(hr))
    return;

  hr = stationary_frame->get_CoordinateSystem(&origin_);
  if (FAILED(hr))
    return;

  // TODO(billorr): Consider adding support for using an attached frame for
  // orientation-only experiences.
}

void MixedRealityRenderLoop::OnSessionStart() {
  // Each session should start with a new origin.
  origin_ = nullptr;
  InitializeOrigin();

  StartPresenting();
}

void MixedRealityRenderLoop::InitializeSpace() {
  // Create a Window, which is required to get an IHolographicSpace.
  // TODO(billorr): Finalize what is rendered to this window, its title, and
  // where the window is shown.
  window_ = std::make_unique<MixedRealityWindow>();
  window_->Init(NULL, gfx::Rect());

  // Create a holographic space from that Window.
  ComPtr<IHolographicSpaceInterop> holographic_space_interop;
  base::win::ScopedHString holographic_space_string =
      base::win::ScopedHString::Create(
          RuntimeClass_Windows_Graphics_Holographic_HolographicSpace);
  HRESULT hr = base::win::RoGetActivationFactory(
      holographic_space_string.get(), IID_PPV_ARGS(&holographic_space_interop));

  if (SUCCEEDED(hr)) {
    hr = holographic_space_interop->CreateForWindow(
        window_->hwnd(), IID_PPV_ARGS(&holographic_space_));
  }
}

void MixedRealityRenderLoop::StartPresenting() {
  ShowWindow(window_->hwnd(), SW_SHOW);
}

mojom::XRGamepadDataPtr MixedRealityRenderLoop::GetNextGamepadData() {
  // Not yet implemented.
  return nullptr;
}

struct EyeToWorldDecomposed {
  gfx::Quaternion world_to_eye_rotation;
  gfx::Point3F eye_in_world_space;
};

EyeToWorldDecomposed DecomposeViewMatrix(
    const ABI::Windows::Foundation::Numerics::Matrix4x4& view) {
  gfx::Transform world_to_view(view.M11, view.M21, view.M31, view.M41, view.M12,
                               view.M22, view.M32, view.M42, view.M13, view.M23,
                               view.M33, view.M43, view.M14, view.M24, view.M34,
                               view.M44);

  gfx::Transform view_to_world;
  bool invertable = world_to_view.GetInverse(&view_to_world);
  DCHECK(invertable);

  gfx::Point3F eye_in_world_space(view_to_world.matrix().get(0, 3),
                                  view_to_world.matrix().get(1, 3),
                                  view_to_world.matrix().get(2, 3));

  gfx::DecomposedTransform world_to_view_decomposed;
  bool decomposable =
      gfx::DecomposeTransform(&world_to_view_decomposed, world_to_view);
  DCHECK(decomposable);

  gfx::Quaternion world_to_eye_rotation = world_to_view_decomposed.quaternion;
  return {world_to_eye_rotation.inverse(), eye_in_world_space};
}

mojom::VRPosePtr GetMonoViewData(const HolographicStereoTransform& view) {
  auto eye = DecomposeViewMatrix(view.Left);

  auto pose = mojom::VRPose::New();

  // World to device orientation.
  pose->orientation =
      std::vector<float>{static_cast<float>(eye.world_to_eye_rotation.x()),
                         static_cast<float>(eye.world_to_eye_rotation.y()),
                         static_cast<float>(eye.world_to_eye_rotation.z()),
                         static_cast<float>(eye.world_to_eye_rotation.w())};

  // Position in world space.
  pose->position =
      std::vector<float>{eye.eye_in_world_space.x(), eye.eye_in_world_space.y(),
                         eye.eye_in_world_space.z()};

  return pose;
}

struct PoseAndEyeTransform {
  mojom::VRPosePtr pose;
  std::vector<float> left_offset;
  std::vector<float> right_offset;
};

PoseAndEyeTransform GetStereoViewData(const HolographicStereoTransform& view) {
  auto left_eye = DecomposeViewMatrix(view.Left);
  auto right_eye = DecomposeViewMatrix(view.Right);
  auto center = gfx::Point3F(
      (left_eye.eye_in_world_space.x() + right_eye.eye_in_world_space.x()) / 2,
      (left_eye.eye_in_world_space.y() + right_eye.eye_in_world_space.y()) / 2,
      (left_eye.eye_in_world_space.z() + right_eye.eye_in_world_space.z()) / 2);

  // We calculate the overal headset pose to be the slerp of per-eye poses as
  // calculated by the view transform's decompositions.  Although this works, we
  // should consider using per-eye rotation as well as translation for eye
  // parameters. See https://crbug.com/928433 for a similar issue.
  gfx::Quaternion world_to_view_rotation = left_eye.world_to_eye_rotation;
  world_to_view_rotation.Slerp(right_eye.world_to_eye_rotation, 0.5f);

  // Calculate new eye offsets.
  gfx::Vector3dF left_offset = left_eye.eye_in_world_space - center;
  gfx::Vector3dF right_offset = right_eye.eye_in_world_space - center;

  gfx::Transform transform(world_to_view_rotation);  // World to view.
  transform.Transpose();                             // Now it is view to world.

  transform.TransformVector(&left_offset);  // Offset is now in view space
  transform.TransformVector(&right_offset);

  PoseAndEyeTransform ret;
  ret.right_offset =
      std::vector<float>{right_offset.x(), right_offset.y(), right_offset.z()};
  ret.left_offset =
      std::vector<float>{left_offset.x(), left_offset.y(), left_offset.z()};

  // TODO(https://crbug.com/928433): We don't currently support per-eye rotation
  // in the mojo interface, but we should.

  ret.pose = mojom::VRPose::New();

  // World to device orientation.
  ret.pose->orientation =
      std::vector<float>{static_cast<float>(world_to_view_rotation.x()),
                         static_cast<float>(world_to_view_rotation.y()),
                         static_cast<float>(world_to_view_rotation.z()),
                         static_cast<float>(world_to_view_rotation.w())};

  // Position in world space.
  ret.pose->position = std::vector<float>{center.x(), center.y(), center.z()};

  return ret;
}

mojom::XRFrameDataPtr CreateDefaultFrameData(
    ComPtr<IPerceptionTimestamp> timestamp,
    int16_t frame_id) {
  mojom::XRFrameDataPtr ret = mojom::XRFrameData::New();

  ABI::Windows::Foundation::DateTime date_time;
  timestamp->get_TargetTime(&date_time);

  ABI::Windows::Foundation::TimeSpan relative_time;
  if (SUCCEEDED(timestamp->get_PredictionAmount(&relative_time))) {
    // relative_time.Duration is a count of 100ns units, so multiply by 100
    // to get a count of nanoseconds.
    double milliseconds =
        base::TimeDelta::FromNanosecondsD(100.0 * relative_time.Duration)
            .InMillisecondsF();
    TRACE_EVENT_INSTANT1("gpu", "WebXR pose prediction",
                         TRACE_EVENT_SCOPE_THREAD, "milliseconds",
                         milliseconds);
  }

  ret->time_delta =
      base::TimeDelta::FromMicroseconds(date_time.UniversalTime / 10);
  ret->frame_id = frame_id;
  return ret;
}

void MixedRealityRenderLoop::UpdateWMRDataForNextFrame() {
  holographic_frame_ = nullptr;
  poses_ = nullptr;
  pose_ = nullptr;
  rendering_params_ = nullptr;
  camera_ = nullptr;
  timestamp_ = nullptr;

  // Start populating this frame's data.
  HRESULT hr = holographic_space_->CreateNextFrame(&holographic_frame_);
  if (FAILED(hr))
    return;

  ComPtr<IHolographicFramePrediction> prediction;
  hr = holographic_frame_->get_CurrentPrediction(&prediction);
  if (FAILED(hr))
    return;

  hr = prediction->get_Timestamp(&timestamp_);
  if (FAILED(hr))
    return;

  hr = prediction->get_CameraPoses(&poses_);
  if (FAILED(hr))
    return;

  unsigned int num;
  hr = poses_->get_Size(&num);
  if (FAILED(hr) || num != 1) {
    return;
  }

  // There is only 1 pose.
  hr = poses_->GetAt(0, &pose_);
  if (FAILED(hr))
    return;

  hr = holographic_frame_->GetRenderingParameters(pose_.Get(),
                                                  &rendering_params_);
  if (FAILED(hr))
    return;

  // Make sure we have an origin.
  if (!origin_) {
    InitializeOrigin();
  }

  if (FAILED(pose_->get_HolographicCamera(&camera_)))
    return;
}

bool MixedRealityRenderLoop::UpdateDisplayInfo() {
  if (!pose_)
    return false;
  if (!camera_)
    return false;

  ABI::Windows::Graphics::Holographic::HolographicStereoTransform projection;
  if (FAILED(pose_->get_ProjectionTransform(&projection)))
    return false;

  ABI::Windows::Foundation::Size size;
  if (FAILED(camera_->get_RenderTargetSize(&size)))
    return false;
  boolean stereo;
  if (FAILED(camera_->get_IsStereo(&stereo)))
    return false;

  bool changed = false;

  if (!current_display_info_) {
    current_display_info_ = mojom::VRDisplayInfo::New();
    current_display_info_->id =
        device::mojom::XRDeviceId::WINDOWS_MIXED_REALITY_ID;
    current_display_info_->displayName =
        "Windows Mixed Reality";  // TODO(billorr): share this string.
    current_display_info_->capabilities = mojom::VRDisplayCapabilities::New(
        true /* hasPosition */, true /* hasExternalDisplay */,
        true /* canPresent */, false /* canProvideEnvironmentIntegration */);

    // TODO(billorr): consider scaling framebuffers after rendering support is
    // added.
    current_display_info_->webvr_default_framebuffer_scale = 1.0f;
    current_display_info_->webxr_default_framebuffer_scale = 1.0f;

    changed = true;
  }

  if (!stereo && current_display_info_->rightEye) {
    changed = true;
    current_display_info_->rightEye = nullptr;
  }

  if (!current_display_info_->leftEye) {
    current_display_info_->leftEye = mojom::VREyeParameters::New();
    changed = true;
  }

  if (current_display_info_->leftEye->renderWidth != size.Width ||
      current_display_info_->leftEye->renderHeight != size.Height) {
    changed = true;
    current_display_info_->leftEye->renderWidth = size.Width;
    current_display_info_->leftEye->renderHeight = size.Height;
  }

  auto left_fov = ParseProjection(projection.Left);
  if (!current_display_info_->leftEye->fieldOfView ||
      !left_fov->Equals(*current_display_info_->leftEye->fieldOfView)) {
    current_display_info_->leftEye->fieldOfView = std::move(left_fov);
    changed = true;
  }

  if (stereo) {
    if (!current_display_info_->rightEye) {
      current_display_info_->rightEye = mojom::VREyeParameters::New();
      changed = true;
    }

    if (current_display_info_->rightEye->renderWidth != size.Width ||
        current_display_info_->rightEye->renderHeight != size.Height) {
      changed = true;
      current_display_info_->rightEye->renderWidth = size.Width;
      current_display_info_->rightEye->renderHeight = size.Height;
    }

    auto right_fov = ParseProjection(projection.Right);
    if (!current_display_info_->rightEye->fieldOfView ||
        !right_fov->Equals(*current_display_info_->rightEye->fieldOfView)) {
      current_display_info_->rightEye->fieldOfView = std::move(right_fov);
      changed = true;
    }
  }

  // TODO(billorr): Need to set stageParameters.
  return changed;
}

mojom::XRFrameDataPtr MixedRealityRenderLoop::GetNextFrameData() {
  UpdateWMRDataForNextFrame();
  if (!timestamp_)
    return nullptr;

  // Once we have a prediction, we can generate a frame data.
  mojom::XRFrameDataPtr ret =
      CreateDefaultFrameData(timestamp_, next_frame_id_);

  if (!origin_ || !pose_) {
    // If we don't have an origin or pose for this frame, we can still give out
    // a timestamp and frame to render head-locked content.
    return ret;
  }

  Microsoft::WRL::ComPtr<ABI::Windows::Foundation::IReference<
      ABI::Windows::Graphics::Holographic::HolographicStereoTransform>>
      view_ref;
  HRESULT hr = pose_->TryGetViewTransform(origin_.Get(), &view_ref);
  if (FAILED(hr) || !view_ref) {
    // If we can't locate origin_, throw it away and try to get a new origin
    // next frame.
    // TODO(billorr): Try to keep the origin working over multiple frames, doing
    // some transform work.
    origin_ = nullptr;
    return ret;
  }

  ABI::Windows::Graphics::Holographic::HolographicStereoTransform view;
  if (FAILED(view_ref->get_Value(&view)))
    return ret;

  bool send_new_display_info = UpdateDisplayInfo();
  if (!current_display_info_)
    return ret;

  if (current_display_info_->rightEye) {
    // If we have a right eye, we are stereo.
    PoseAndEyeTransform pose_and_eye_transform = GetStereoViewData(view);
    ret->pose = std::move(pose_and_eye_transform.pose);

    if (current_display_info_->leftEye->offset !=
            pose_and_eye_transform.left_offset ||
        current_display_info_->rightEye->offset !=
            pose_and_eye_transform.right_offset) {
      current_display_info_->leftEye->offset =
          std::move(pose_and_eye_transform.left_offset);
      current_display_info_->rightEye->offset =
          std::move(pose_and_eye_transform.right_offset);
      send_new_display_info = true;
    }
  } else {
    ret->pose = GetMonoViewData(view);
    std::vector<float> offset = {0, 0, 0};
    if (current_display_info_->leftEye->offset != offset) {
      current_display_info_->leftEye->offset = offset;
      send_new_display_info = true;
    }
  }

  if (send_new_display_info) {
    // Update the eye info for this frame.
    ret->left_eye = current_display_info_->leftEye.Clone();
    ret->right_eye = current_display_info_->rightEye.Clone();

    // Notify the device about the display info change.
    main_thread_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(on_display_info_changed_,
                                  current_display_info_.Clone()));
  }

  ret->pose->input_state = GetInputState();

  return ret;
}

void MixedRealityRenderLoop::GetEnvironmentIntegrationProvider(
    mojom::XREnvironmentIntegrationProviderAssociatedRequest
        environment_provider) {
  // Environment integration is not supported. This call should not
  // be made on this device.
  mojo::ReportBadMessage("Environment integration is not supported.");
  return;
}

bool MixedRealityRenderLoop::EnsureSpatialInteractionManager() {
  if (spatial_interaction_manager_)
    return true;

  if (!window_)
    return false;

  ComPtr<ISpatialInteractionManagerInterop> spatial_interaction_interop;
  base::win::ScopedHString spatial_interaction_interop_string =
      base::win::ScopedHString::Create(
          RuntimeClass_Windows_UI_Input_Spatial_SpatialInteractionManager);
  HRESULT hr = base::win::RoGetActivationFactory(
      spatial_interaction_interop_string.get(),
      IID_PPV_ARGS(&spatial_interaction_interop));
  if (FAILED(hr))
    return false;

  hr = spatial_interaction_interop->GetForWindow(
      window_->hwnd(), IID_PPV_ARGS(&spatial_interaction_manager_));
  return SUCCEEDED(hr);
}

std::vector<mojom::XRInputSourceStatePtr>
MixedRealityRenderLoop::GetInputState() {
  std::vector<mojom::XRInputSourceStatePtr> input_states;

  if (!timestamp_ || !origin_ || !EnsureSpatialInteractionManager())
    return input_states;

  ComPtr<WFC::IVectorView<WInput::SpatialInteractionSourceState*>>
      source_states;
  if (FAILED(spatial_interaction_manager_->GetDetectedSourcesAtTimestamp(
          timestamp_.Get(), &source_states)))
    return input_states;

  unsigned int size;
  if (FAILED(source_states->get_Size(&size)))
    return input_states;

  for (unsigned int i = 0; i < size; i++) {
    ComPtr<WInput::ISpatialInteractionSourceState> state;
    if (FAILED(source_states->GetAt(i, &state)))
      continue;

    // Get the source and query for all of the state that we send first
    ComPtr<WInput::ISpatialInteractionSource> source;
    if (FAILED(state->get_Source(&source)))
      continue;

    WInput::SpatialInteractionSourceKind source_kind;
    if (FAILED(source->get_Kind(&source_kind)) ||
        source_kind != WInput::SpatialInteractionSourceKind_Controller)
      continue;

    uint32_t id;
    if (FAILED(source->get_Id(&id)))
      continue;

    ComPtr<WInput::ISpatialInteractionSourceState2> state2;
    if (FAILED(state.As(&state2)))
      continue;

    boolean pressed = false;
    if (FAILED(state2->get_IsSelectPressed(&pressed)))
      continue;

    ComPtr<WInput::ISpatialInteractionSourceProperties> props;
    if (FAILED(state->get_Properties(&props)))
      continue;

    ComPtr<WInput::ISpatialInteractionSourceLocation> location_in_origin;
    if (FAILED(props->TryGetLocation(origin_.Get(), &location_in_origin)) ||
        !location_in_origin)
      continue;

    gfx::Transform origin_from_grip;
    if (!TryGetGripFromLocation(location_in_origin, &origin_from_grip))
      continue;

    ComPtr<WInput::ISpatialInteractionSource2> source2;
    boolean pointingSupported = false;
    if (FAILED(source.As(&source2)) ||
        FAILED(source2->get_IsPointingSupported(&pointingSupported)) ||
        !pointingSupported)
      continue;

    gfx::Transform grip_from_pointer;
    if (!TryGetPointerOffsetFromLocation(location_in_origin, origin_from_grip,
                                         &grip_from_pointer))
      continue;

    // Now that we've queried for all of the state we're going to send,
    // build the object to send it.
    device::mojom::XRInputSourceStatePtr source_state =
        device::mojom::XRInputSourceState::New();

    // Hands may not have the same id especially if they are lost but since we
    // are only tracking controllers, this id should be consistent.
    source_state->source_id = id;

    source_state->primary_input_pressed = pressed;
    source_state->primary_input_clicked =
        !pressed && controller_pressed_state_[id];
    controller_pressed_state_[id] = pressed;

    source_state->grip = origin_from_grip;

    device::mojom::XRInputSourceDescriptionPtr description =
        device::mojom::XRInputSourceDescription::New();

    // It's a fully 6DoF handheld pointing device
    description->emulated_position = false;
    description->target_ray_mode = device::mojom::XRTargetRayMode::POINTING;

    description->pointer_offset = grip_from_pointer;

    WInput::SpatialInteractionSourceHandedness handedness;
    ComPtr<WInput::ISpatialInteractionSource3> source3;
    if (SUCCEEDED(source.As(&source3) &&
                  SUCCEEDED(source3->get_Handedness(&handedness)))) {
      switch (handedness) {
        case WInput::SpatialInteractionSourceHandedness_Left:
          description->handedness = device::mojom::XRHandedness::LEFT;
          break;
        case WInput::SpatialInteractionSourceHandedness_Right:
          description->handedness = device::mojom::XRHandedness::RIGHT;
          break;
        default:
          description->handedness = device::mojom::XRHandedness::NONE;
          break;
      }
    } else {
      description->handedness = device::mojom::XRHandedness::NONE;
    }

    source_state->description = std::move(description);
    input_states.push_back(std::move(source_state));
  }

  return input_states;
}

}  // namespace device
