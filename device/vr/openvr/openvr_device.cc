// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#define _USE_MATH_DEFINES  // for M_PI
#include "device/vr/openvr/openvr_device.h"
#include <math.h>
#include "third_party/openvr/src/headers/openvr.h"

namespace {

constexpr float kRadToDeg = static_cast<float>(180 / M_PI);
constexpr float kDefaultIPD = 0.06f;  // Default average IPD

device::mojom::VRFieldOfViewPtr openVRFovToWebVRFov(vr::IVRSystem* vr_system,
                                                    vr::Hmd_Eye eye) {
  device::mojom::VRFieldOfViewPtr out = device::mojom::VRFieldOfView::New();
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

}  // namespace

namespace device {

OpenVRDevice::OpenVRDevice(vr::IVRSystem* vr)
    : vr_system_(vr), weak_ptr_factory_(this), is_polling_events_(false) {}
OpenVRDevice::~OpenVRDevice() {}

void OpenVRDevice::CreateVRDisplayInfo(
    const base::Callback<void(mojom::VRDisplayInfoPtr)>& on_created) {
  if (!vr_system_) {
    on_created.Run(nullptr);
    return;
  }

  mojom::VRDisplayInfoPtr device = mojom::VRDisplayInfo::New();
  device->index = id();
  device->displayName =
      GetOpenVRString(vr_system_, vr::Prop_ManufacturerName_String) + " " +
      GetOpenVRString(vr_system_, vr::Prop_ModelNumber_String);
  device->capabilities = mojom::VRDisplayCapabilities::New();
  device->capabilities->hasPosition = true;
  device->capabilities->hasExternalDisplay = true;
  device->capabilities->canPresent = false;

  device->leftEye = mojom::VREyeParameters::New();
  device->rightEye = mojom::VREyeParameters::New();
  mojom::VREyeParametersPtr& left_eye = device->leftEye;
  mojom::VREyeParametersPtr& right_eye = device->rightEye;

  left_eye->fieldOfView = openVRFovToWebVRFov(vr_system_, vr::Eye_Left);
  right_eye->fieldOfView = openVRFovToWebVRFov(vr_system_, vr::Eye_Left);

  vr::TrackedPropertyError error = vr::TrackedProp_Success;
  float ipd = vr_system_->GetFloatTrackedDeviceProperty(
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
  vr_system_->GetRecommendedRenderTargetSize(&width, &height);
  left_eye->renderWidth = width;
  left_eye->renderHeight = height;
  right_eye->renderWidth = left_eye->renderWidth;
  right_eye->renderHeight = left_eye->renderHeight;

  device->stageParameters = mojom::VRStageParameters::New();
  vr::HmdMatrix34_t mat =
      vr_system_->GetSeatedZeroPoseToStandingAbsoluteTrackingPose();
  device->stageParameters->standingTransform =
      HmdMatrix34ToWebVRTransformMatrix(mat);

  vr::IVRChaperone* chaperone = vr::VRChaperone();
  if (chaperone) {
    chaperone->GetPlayAreaSize(&device->stageParameters->sizeX,
                               &device->stageParameters->sizeZ);
  } else {
    device->stageParameters->sizeX = 0.0f;
    device->stageParameters->sizeZ = 0.0f;
  }

  // If it is the first initialization, OpenVRRenderLoop instance needs to be
  // created and the polling event callback needs to be registered.
  if (!render_loop_) {
    render_loop_ = std::make_unique<OpenVRRenderLoop>(vr_system_);

    render_loop_->RegisterPollingEventCallback(base::Bind(
        &OpenVRDevice::OnPollingEvents, weak_ptr_factory_.GetWeakPtr()));
  }

  on_created.Run(std::move(device));
}

void OpenVRDevice::RequestPresent(mojom::VRSubmitFrameClientPtr submit_client,
                                  const base::Callback<void(bool)>& callback) {
  callback.Run(false);
  // We don't support presentation currently.
}

void OpenVRDevice::SetSecureOrigin(bool secure_origin) {
  // We don't support presentation currently, so don't do anything.
}

void OpenVRDevice::ExitPresent() {
  // We don't support presentation currently, so don't do anything.
}

void OpenVRDevice::SubmitFrame(int16_t frame_index,
                               const gpu::MailboxHolder& mailbox) {
  // We don't support presentation currently, so don't do anything.
}

void OpenVRDevice::UpdateLayerBounds(int16_t frame_index,
                                     mojom::VRLayerBoundsPtr left_bounds,
                                     mojom::VRLayerBoundsPtr right_bounds,
                                     int16_t source_width,
                                     int16_t source_height) {
  // We don't support presentation currently, so don't do anything.
}

void OpenVRDevice::GetVRVSyncProvider(mojom::VRVSyncProviderRequest request) {
  render_loop_->Bind(std::move(request));
}

OpenVRDevice::OpenVRRenderLoop::OpenVRRenderLoop(vr::IVRSystem* vr_system)
    : vr_system_(vr_system),
      main_thread_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      binding_(this),
      base::SimpleThread("OpenVRRenderLoop") {
  DCHECK(main_thread_task_runner_);
}

void OpenVRDevice::OpenVRRenderLoop::Bind(
    mojom::VRVSyncProviderRequest request) {
  binding_.Close();
  binding_.Bind(std::move(request));
}

void OpenVRDevice::OpenVRRenderLoop::Run() {
  // TODO (BillOrr): We will wait for VSyncs on this thread using WaitGetPoses
  // when we support presentation.
}

device::mojom::VRPosePtr OpenVRDevice::OpenVRRenderLoop::getPose() {
  device::mojom::VRPosePtr pose = device::mojom::VRPose::New();
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

  if (is_changed) {
    OnChanged();
  }
}

// Register a callback function to deal with system events.
void OpenVRDevice::OpenVRRenderLoop::RegisterPollingEventCallback(
    const base::Callback<void()>& onPollingEvents) {
  if (onPollingEvents.is_null())
    return;

  on_polling_events_ = onPollingEvents;
}

void OpenVRDevice::OpenVRRenderLoop::UnregisterPollingEventCallback() {
  on_polling_events_.Reset();
}

void OpenVRDevice::OpenVRRenderLoop::GetVSync(
    const mojom::VRVSyncProvider::GetVSyncCallback& callback) {
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

  device::mojom::VRPosePtr pose = getPose();
  Sleep(11);  // TODO (billorr): Use real vsync timing instead of a sleep (this
              // sleep just throttles vsyncs so we don't fill message queues).
  callback.Run(std::move(pose), time, frame,
               device::mojom::VRVSyncProvider::Status::SUCCESS);
}

}  // namespace device