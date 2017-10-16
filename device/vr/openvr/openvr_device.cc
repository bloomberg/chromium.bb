// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#define _USE_MATH_DEFINES  // for M_PI

#include <math.h>

#include "base/memory/ptr_util.h"
#include "device/vr/openvr/openvr_device.h"
#include "third_party/openvr/src/headers/openvr.h"

namespace device {

namespace {

constexpr float kRadToDeg = static_cast<float>(180 / M_PI);
constexpr float kDefaultIPD = 0.06f;  // Default average IPD

mojom::VRFieldOfViewPtr OpenVRFovToWebVRFov(vr::IVRSystem* vr_system,
                                            vr::Hmd_Eye eye) {
  auto out = mojom::VRFieldOfView::New();
  float up_tan, down_tan, left_tan, right_tan;
  vr_system->GetProjectionRaw(eye, &left_tan, &right_tan, &up_tan, &down_tan);
  out->upDegrees = -(atanf(up_tan) * kRadToDeg);
  out->downDegrees = atanf(down_tan) * kRadToDeg;
  out->leftDegrees = -(atanf(left_tan) * kRadToDeg);
  out->rightDegrees = atanf(right_tan) * kRadToDeg;
  return out;
}

std::vector<float> HmdVector3ToWebVR(const vr::HmdVector3_t& vec) {
  std::vector<float> out;
  out.resize(3);
  out[0] = vec.v[0];
  out[1] = vec.v[1];
  out[2] = vec.v[2];
  return out;
}

std::string GetOpenVRString(vr::IVRSystem* vr_system,
                            vr::TrackedDeviceProperty prop) {
  std::string out;

  vr::TrackedPropertyError error = vr::TrackedProp_Success;
  char openvr_string[vr::k_unMaxPropertyStringSize];
  vr_system->GetStringTrackedDeviceProperty(
      vr::k_unTrackedDeviceIndex_Hmd, prop, openvr_string,
      vr::k_unMaxPropertyStringSize, &error);

  if (error == vr::TrackedProp_Success)
    out = openvr_string;

  return out;
}

std::vector<float> HmdMatrix34ToWebVRTransformMatrix(
    const vr::HmdMatrix34_t& mat) {
  std::vector<float> transform;
  transform.resize(16);
  transform[0] = mat.m[0][0];
  transform[1] = mat.m[1][0];
  transform[2] = mat.m[2][0];
  transform[3] = 0.0f;
  transform[4] = mat.m[0][1];
  transform[5] = mat.m[1][1];
  transform[6] = mat.m[2][1];
  transform[7] = 0.0f;
  transform[8] = mat.m[0][2];
  transform[9] = mat.m[1][2];
  transform[10] = mat.m[2][2];
  transform[11] = 0.0f;
  transform[12] = mat.m[0][3];
  transform[13] = mat.m[1][3];
  transform[14] = mat.m[2][3];
  transform[15] = 1.0f;
  return transform;
}

mojom::VRDisplayInfoPtr CreateVRDisplayInfo(vr::IVRSystem* vr_system,
                                            unsigned int id) {
  mojom::VRDisplayInfoPtr display_info = mojom::VRDisplayInfo::New();
  display_info->index = id;
  display_info->displayName =
      GetOpenVRString(vr_system, vr::Prop_ManufacturerName_String) + " " +
      GetOpenVRString(vr_system, vr::Prop_ModelNumber_String);
  display_info->capabilities = mojom::VRDisplayCapabilities::New();
  display_info->capabilities->hasPosition = true;
  display_info->capabilities->hasExternalDisplay = true;
  display_info->capabilities->canPresent = false;

  display_info->leftEye = mojom::VREyeParameters::New();
  display_info->rightEye = mojom::VREyeParameters::New();
  mojom::VREyeParametersPtr& left_eye = display_info->leftEye;
  mojom::VREyeParametersPtr& right_eye = display_info->rightEye;

  left_eye->fieldOfView = OpenVRFovToWebVRFov(vr_system, vr::Eye_Left);
  right_eye->fieldOfView = OpenVRFovToWebVRFov(vr_system, vr::Eye_Left);

  vr::TrackedPropertyError error = vr::TrackedProp_Success;
  float ipd = vr_system->GetFloatTrackedDeviceProperty(
      vr::k_unTrackedDeviceIndex_Hmd, vr::Prop_UserIpdMeters_Float, &error);

  if (error != vr::TrackedProp_Success)
    ipd = kDefaultIPD;

  left_eye->offset.resize(3);
  left_eye->offset[0] = -ipd * 0.5;
  left_eye->offset[1] = 0.0f;
  left_eye->offset[2] = 0.0f;
  right_eye->offset.resize(3);
  right_eye->offset[0] = ipd * 0.5;
  right_eye->offset[1] = 0.0;
  right_eye->offset[2] = 0.0;

  uint32_t width, height;
  vr_system->GetRecommendedRenderTargetSize(&width, &height);
  left_eye->renderWidth = width;
  left_eye->renderHeight = height;
  right_eye->renderWidth = left_eye->renderWidth;
  right_eye->renderHeight = left_eye->renderHeight;

  display_info->stageParameters = mojom::VRStageParameters::New();
  vr::HmdMatrix34_t mat =
      vr_system->GetSeatedZeroPoseToStandingAbsoluteTrackingPose();
  display_info->stageParameters->standingTransform =
      HmdMatrix34ToWebVRTransformMatrix(mat);

  vr::IVRChaperone* chaperone = vr::VRChaperone();
  if (chaperone) {
    chaperone->GetPlayAreaSize(&display_info->stageParameters->sizeX,
                               &display_info->stageParameters->sizeZ);
  } else {
    display_info->stageParameters->sizeX = 0.0f;
    display_info->stageParameters->sizeZ = 0.0f;
  }

  return display_info;
}

class OpenVRRenderLoop : public base::SimpleThread,
                         mojom::VRPresentationProvider {
 public:
  OpenVRRenderLoop(vr::IVRSystem* vr);

  void RegisterPollingEventCallback(
      const base::Callback<void()>& on_polling_events);

  void UnregisterPollingEventCallback();

  void Bind(mojom::VRPresentationProvider request);

  mojom::VRPosePtr GetPose();

 private:
  void Run() override;

  void GetVSync(
      mojom::VRPresentationProvider::GetVSyncCallback callback) override;
  void SubmitFrame(int16_t frame_index,
                   const gpu::MailboxHolder& mailbox) override;
  void UpdateLayerBounds(int16_t frame_id,
                         const gfx::RectF& left_bounds,
                         const gfx::RectF& right_bounds,
                         const gfx::Size& source_size) override;

  scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner_;
  base::Callback<void()> on_polling_events_;
  vr::IVRSystem* vr_system_;
  mojo::Binding<mojom::VRPresentationProvider> binding_;
};

}  // namespace

OpenVRDevice::OpenVRDevice(vr::IVRSystem* vr)
    : vr_system_(vr), weak_ptr_factory_(this), is_polling_events_(false) {
  DCHECK(vr_system_);
  SetVRDisplayInfo(CreateVRDisplayInfo(vr_system_, GetId()));
  render_loop_ = std::make_unique<OpenVRRenderLoop>(vr_system_);
  render_loop_->RegisterPollingEventCallback(base::Bind(
      &OpenVRDevice::OnPollingEvents, weak_ptr_factory_.GetWeakPtr()));
}
OpenVRDevice::~OpenVRDevice() {}

mojom::VRDisplayInfoPtr OpenVRDevice::GetVRDisplayInfo() {
  return display_info_.Clone();
}

OpenVRRenderLoop::OpenVRRenderLoop(vr::IVRSystem* vr_system)
    : vr_system_(vr_system),
      main_thread_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      binding_(this),
      base::SimpleThread("OpenVRRenderLoop") {
  DCHECK(main_thread_task_runner_);
}

void OpenVRRenderLoop::Bind(mojom::VRPresentationProvider request) {
  binding_.Bind(std::move(request));
}

void OpenVRRenderLoop::Run() {
  // TODO (BillOrr): We will wait for VSyncs on this thread using WaitGetPoses
  // when we support presentation.
}

mojom::VRPosePtr OpenVRRenderLoop::GetPose() {
  mojom::VRPosePtr pose = mojom::VRPose::New();
  pose->orientation.emplace(4);

  pose->orientation.value()[0] = 0;
  pose->orientation.value()[1] = 0;
  pose->orientation.value()[2] = 0;
  pose->orientation.value()[3] = 1;

  pose->position.emplace(3);
  pose->position.value()[0] = 0;
  pose->position.value()[1] = 0;
  pose->position.value()[2] = 0;

  vr::TrackedDevicePose_t poses[vr::k_unMaxTrackedDeviceCount];

  vr_system_->GetDeviceToAbsoluteTrackingPose(
      vr::TrackingUniverseSeated, 0.0f, poses, vr::k_unMaxTrackedDeviceCount);
  const auto& hmdPose = poses[vr::k_unTrackedDeviceIndex_Hmd];
  if (hmdPose.bPoseIsValid && hmdPose.bDeviceIsConnected) {
    const auto& transform = hmdPose.mDeviceToAbsoluteTracking;
    const auto& m = transform.m;
    float w = sqrt(1 + m[0][0] + m[1][1] + m[2][2]);
    pose->orientation.value()[0] = (m[2][1] - m[1][2]) / (4 * w);
    pose->orientation.value()[1] = (m[0][2] - m[2][0]) / (4 * w);
    pose->orientation.value()[2] = (m[1][0] - m[0][1]) / (4 * w);
    pose->orientation.value()[3] = w;

    pose->position.value()[0] = m[0][3];
    pose->position.value()[1] = m[1][3];
    pose->position.value()[2] = m[2][3];

    pose->linearVelocity = HmdVector3ToWebVR(hmdPose.vVelocity);
    pose->angularVelocity = HmdVector3ToWebVR(hmdPose.vAngularVelocity);
  }

  return std::move(pose);
}

// Only deal with events that will cause displayInfo changes for now.
void OpenVRDevice::OnPollingEvents() {
  if (!vr_system_ || is_polling_events_)
    return;

  // Polling events through vr_system_ has started.
  is_polling_events_ = true;

  vr::VREvent_t event;
  bool is_changed = false;
  while (vr_system_->PollNextEvent(&event, sizeof(event))) {
    if (event.trackedDeviceIndex != vr::k_unTrackedDeviceIndex_Hmd &&
        event.trackedDeviceIndex != vr::k_unTrackedDeviceIndexInvalid) {
      continue;
    }

    switch (event.eventType) {
      case vr::VREvent_TrackedDeviceUpdated:
      case vr::VREvent_IpdChanged:
      case vr::VREvent_ChaperoneDataHasChanged:
      case vr::VREvent_ChaperoneSettingsHaveChanged:
      case vr::VREvent_ChaperoneUniverseHasChanged:
        is_changed = true;
        break;

      default:
        break;
    }
  }

  // Polling events through vr_system_ has finished.
  is_polling_events_ = false;

  if (is_changed)
    SetVRDisplayInfo(CreateVRDisplayInfo(vr_system_, GetId()));
}

// Register a callback function to deal with system events.
void OpenVRRenderLoop::RegisterPollingEventCallback(
    const base::Callback<void()>& on_polling_events) {
  if (on_polling_events.is_null())
    return;

  on_polling_events_ = on_polling_events;
}

void OpenVRRenderLoop::UnregisterPollingEventCallback() {
  on_polling_events_.Reset();
}

void OpenVRRenderLoop::GetVSync(
    mojom::VRPresentationProvider::GetVSyncCallback callback) {
  static int16_t next_frame = 0;
  int16_t frame = next_frame++;

  // VSync could be used as a signal to poll events.
  if (!on_polling_events_.is_null()) {
    main_thread_task_runner_->PostTask(FROM_HERE, on_polling_events_);
  }

  // TODO(BillOrr): Give real values when VSync loop is hooked up.  This is the
  // presentation time for the frame. Just returning a default value for now
  // since we don't have VSync hooked up.
  base::TimeDelta time = base::TimeDelta::FromSecondsD(2.0);

  mojom::VRPosePtr pose = GetPose();
  Sleep(11);  // TODO (billorr): Use real vsync timing instead of a sleep (this
              // sleep just throttles vsyncs so we don't fill message queues).
  std::move(callback).Run(std::move(pose), time, frame,
                          mojom::VRPresentationProvider::VSyncStatus::SUCCESS);
}

void OpenVRRenderLoop::SubmitFrame(int16_t frame_index,
                                   const gpu::MailboxHolder& mailbox) {
  // We don't support presentation currently, so don't do anything.
}

void OpenVRRenderLoop::UpdateLayerBounds(int16_t frame_id,
                                         const gfx::RectF& left_bounds,
                                         const gfx::RectF& right_bounds,
                                         const gfx::Size& source_size) {
  // We don't support presentation currently, so don't do anything.
}

}  // namespace device
