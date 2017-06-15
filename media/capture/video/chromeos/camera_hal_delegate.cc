// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/chromeos/camera_hal_delegate.h"

#include <fcntl.h>
#include <sys/uio.h>

#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/strings/string_piece.h"
#include "media/capture/video/chromeos/camera_metadata_utils.h"
#include "media/capture/video/chromeos/pixel_format_utils.h"
#include "media/capture/video/chromeos/video_capture_device_arc_chromeos.h"
#include "mojo/edk/embedder/embedder.h"
#include "mojo/edk/embedder/incoming_broker_client_invitation.h"
#include "mojo/edk/embedder/named_platform_handle.h"
#include "mojo/edk/embedder/named_platform_handle_utils.h"
#include "mojo/edk/embedder/platform_channel_pair.h"
#include "mojo/edk/embedder/platform_channel_utils_posix.h"
#include "mojo/edk/embedder/platform_handle_vector.h"

namespace media {

namespace {

const base::StringPiece kArcCamera3SocketPath("/var/run/camera/camera3.sock");

const base::TimeDelta kEventWaitTimeoutMs =
    base::TimeDelta::FromMilliseconds(3000);

}  // namespace

CameraHalDelegate::CameraHalDelegate(
    scoped_refptr<base::SingleThreadTaskRunner> ipc_task_runner)
    : builtin_camera_info_updated_(
          base::WaitableEvent::ResetPolicy::MANUAL,
          base::WaitableEvent::InitialState::NOT_SIGNALED),
      num_builtin_cameras_(0),
      ipc_task_runner_(std::move(ipc_task_runner)),
      camera_module_callbacks_(this) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

CameraHalDelegate::~CameraHalDelegate() {}

bool CameraHalDelegate::StartCameraModuleIpc() {
  // Non-blocking socket handle.
  mojo::edk::ScopedPlatformHandle socket_handle = mojo::edk::CreateClientHandle(
      mojo::edk::NamedPlatformHandle(kArcCamera3SocketPath));

  // Set socket to blocking
  int flags = HANDLE_EINTR(fcntl(socket_handle.get().handle, F_GETFL));
  if (flags == -1) {
    PLOG(ERROR) << "fcntl(F_GETFL) failed: ";
    return false;
  }
  if (HANDLE_EINTR(fcntl(socket_handle.get().handle, F_SETFL,
                         flags & ~O_NONBLOCK)) == -1) {
    PLOG(ERROR) << "fcntl(F_SETFL) failed: ";
    return false;
  }

  const size_t kTokenSize = 32;
  char token[kTokenSize] = {};
  std::deque<mojo::edk::PlatformHandle> platform_handles;
  ssize_t ret = mojo::edk::PlatformChannelRecvmsg(
      socket_handle.get(), token, sizeof(token), &platform_handles, true);
  if (ret == -1) {
    PLOG(ERROR) << "PlatformChannelRecvmsg failed: ";
    return false;
  }
  if (platform_handles.size() != 1) {
    LOG(ERROR) << "Unexpected number of handles received, expected 1: "
               << platform_handles.size();
    return false;
  }
  mojo::edk::ScopedPlatformHandle parent_pipe(platform_handles.back());
  platform_handles.pop_back();
  if (!parent_pipe.is_valid()) {
    LOG(ERROR) << "Invalid parent pipe";
    return false;
  }
  std::unique_ptr<mojo::edk::IncomingBrokerClientInvitation> invitation =
      mojo::edk::IncomingBrokerClientInvitation::Accept(
          mojo::edk::ConnectionParams(mojo::edk::TransportProtocol::kLegacy,
                                      std::move(parent_pipe)));
  mojo::ScopedMessagePipeHandle child_pipe =
      invitation->ExtractMessagePipe(token);
  if (!child_pipe.is_valid()) {
    LOG(ERROR) << "Invalid child pipe";
    return false;
  }

  camera_module_ =
      mojo::MakeProxy(mojo::InterfacePtrInfo<arc::mojom::CameraModule>(
                          std::move(child_pipe), 0u),
                      ipc_task_runner_);

  VLOG(1) << "Camera module IPC connection established";

  return true;
}

void CameraHalDelegate::Reset() {
  ipc_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&CameraHalDelegate::ResetMojoInterfaceOnIpcThread, this));
}

std::unique_ptr<VideoCaptureDevice> CameraHalDelegate::CreateDevice(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner_for_screen_observer,
    const VideoCaptureDeviceDescriptor& device_descriptor) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::unique_ptr<VideoCaptureDevice> capture_device;
  if (!UpdateBuiltInCameraInfo()) {
    return capture_device;
  }
  if (camera_info_.find(device_descriptor.device_id) == camera_info_.end()) {
    LOG(ERROR) << "Invalid camera device: " << device_descriptor.device_id;
    return capture_device;
  }
  capture_device.reset(new VideoCaptureDeviceArcChromeOS(
      std::move(task_runner_for_screen_observer), device_descriptor, this));
  return capture_device;
}

void CameraHalDelegate::GetSupportedFormats(
    const VideoCaptureDeviceDescriptor& device_descriptor,
    VideoCaptureFormats* supported_formats) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!UpdateBuiltInCameraInfo()) {
    return;
  }
  std::string camera_id = device_descriptor.device_id;
  if (camera_info_.find(camera_id) == camera_info_.end() ||
      camera_info_[camera_id].is_null()) {
    LOG(ERROR) << "Invalid camera_id: " << camera_id;
    return;
  }
  const arc::mojom::CameraInfoPtr& camera_info = camera_info_[camera_id];

  const arc::mojom::CameraMetadataEntryPtr* min_frame_durations =
      GetMetadataEntry(camera_info->static_camera_characteristics,
                       arc::mojom::CameraMetadataTag::
                           ANDROID_SCALER_AVAILABLE_MIN_FRAME_DURATIONS);
  if (!min_frame_durations) {
    LOG(ERROR)
        << "Failed to get available min frame durations from camera info";
    return;
  }
  // The min frame durations are stored as tuples of four int64s:
  // (hal_pixel_format, width, height, ns) x n
  const size_t kStreamFormatOffset = 0;
  const size_t kStreamWidthOffset = 1;
  const size_t kStreamHeightOffset = 2;
  const size_t kStreamDurationOffset = 3;
  const size_t kStreamDurationSize = 4;
  int64_t* iter =
      reinterpret_cast<int64_t*>((*min_frame_durations)->data.data());
  for (size_t i = 0; i < (*min_frame_durations)->count;
       i += kStreamDurationSize) {
    int32_t format = base::checked_cast<int32_t>(iter[kStreamFormatOffset]);
    int32_t width = base::checked_cast<int32_t>(iter[kStreamWidthOffset]);
    int32_t height = base::checked_cast<int32_t>(iter[kStreamHeightOffset]);
    int64_t duration = iter[kStreamDurationOffset];
    iter += kStreamDurationSize;

    if (duration <= 0) {
      LOG(ERROR) << "Ignoring invalid frame duration: " << duration;
      continue;
    }
    float max_fps = 1.0 * 1000000000LL / duration;

    DVLOG(1) << "[" << std::hex << format << " " << std::dec << width << " "
             << height << " " << duration << "]";
    VideoPixelFormat cr_format =
        PixFormatHalToChromium(static_cast<arc::mojom::HalPixelFormat>(format));
    if (cr_format == PIXEL_FORMAT_UNKNOWN) {
      continue;
    }
    VLOG(1) << "Supported format: " << width << "x" << height
            << " fps=" << max_fps << " format=" << cr_format;
    supported_formats->emplace_back(gfx::Size(width, height), max_fps,
                                    cr_format);
  }
}

void CameraHalDelegate::GetDeviceDescriptors(
    VideoCaptureDeviceDescriptors* device_descriptors) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!UpdateBuiltInCameraInfo()) {
    return;
  }
  for (size_t id = 0; id < num_builtin_cameras_; ++id) {
    VideoCaptureDeviceDescriptor desc;
    std::string camera_id = std::to_string(id);
    const arc::mojom::CameraInfoPtr& camera_info = camera_info_[camera_id];
    if (!camera_info) {
      continue;
    }
    desc.device_id = camera_id;
    desc.capture_api = VideoCaptureApi::ANDROID_API2_LIMITED;
    desc.transport_type = VideoCaptureTransportType::OTHER_TRANSPORT;
    switch (camera_info->facing) {
      case arc::mojom::CameraFacing::CAMERA_FACING_BACK:
        desc.facing = VideoFacingMode::MEDIA_VIDEO_FACING_ENVIRONMENT;
        desc.display_name = std::string("Back Camera");
        break;
      case arc::mojom::CameraFacing::CAMERA_FACING_FRONT:
        desc.facing = VideoFacingMode::MEDIA_VIDEO_FACING_USER;
        desc.display_name = std::string("Front Camera");
        break;
      case arc::mojom::CameraFacing::CAMERA_FACING_EXTERNAL:
        desc.facing = VideoFacingMode::MEDIA_VIDEO_FACING_NONE;
        desc.display_name = std::string("External Camera");
        break;
        // Mojo validates the input parameters for us so we don't need to worry
        // about malformed values.
    }
    device_descriptors->push_back(desc);
  }
  // TODO(jcliang): Remove this after JS API supports query camera facing
  // (http://crbug.com/543997).
  std::sort(device_descriptors->begin(), device_descriptors->end());
}

void CameraHalDelegate::GetCameraInfo(int32_t camera_id,
                                      const GetCameraInfoCallback& callback) {
  // This method may be called on any thread.  Currently this method is used by
  // CameraDeviceDelegate to query camera info.
  ipc_task_runner_->PostTask(
      FROM_HERE, base::Bind(&CameraHalDelegate::GetCameraInfoOnIpcThread, this,
                            camera_id, callback));
}

void CameraHalDelegate::OpenDevice(
    int32_t camera_id,
    arc::mojom::Camera3DeviceOpsRequest device_ops_request,
    const OpenDeviceCallback& callback) {
  // This method may be called on any thread.  Currently this method is used by
  // CameraDeviceDelegate to open a camera device.
  ipc_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&CameraHalDelegate::OpenDeviceOnIpcThread, this, camera_id,
                 base::Passed(&device_ops_request), callback));
}

void CameraHalDelegate::StartForTesting(arc::mojom::CameraModulePtrInfo info) {
  camera_module_.Bind(std::move(info), ipc_task_runner_);
}

void CameraHalDelegate::ResetMojoInterfaceOnIpcThread() {
  DCHECK(ipc_task_runner_->BelongsToCurrentThread());
  camera_module_.reset();
  if (camera_module_callbacks_.is_bound()) {
    camera_module_callbacks_.Close();
  }
}

bool CameraHalDelegate::UpdateBuiltInCameraInfo() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (builtin_camera_info_updated_.IsSignaled()) {
    return true;
  }
  // The built-in camera are static per specification of the Android camera HAL
  // v3 specification.  We only update the built-in camera info once.
  ipc_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&CameraHalDelegate::UpdateBuiltInCameraInfoOnIpcThread, this));
  if (!builtin_camera_info_updated_.TimedWait(kEventWaitTimeoutMs)) {
    LOG(ERROR) << "Timed out getting camera info";
    return false;
  }
  return true;
}

void CameraHalDelegate::UpdateBuiltInCameraInfoOnIpcThread() {
  DCHECK(ipc_task_runner_->BelongsToCurrentThread());
  camera_module_->GetNumberOfCameras(
      base::Bind(&CameraHalDelegate::OnGotNumberOfCamerasOnIpcThread, this));
}

void CameraHalDelegate::OnGotNumberOfCamerasOnIpcThread(int32_t num_cameras) {
  DCHECK(ipc_task_runner_->BelongsToCurrentThread());
  if (num_cameras <= 0) {
    builtin_camera_info_updated_.Signal();
    LOG(ERROR) << "Failed to get number of cameras: " << num_cameras;
    return;
  }
  VLOG(1) << "Number of built-in cameras: " << num_cameras;
  num_builtin_cameras_ = num_cameras;
  // Per camera HAL v3 specification SetCallbacks() should be called after the
  // first time GetNumberOfCameras() is called, and before other CameraModule
  // functions are called.
  arc::mojom::CameraModuleCallbacksPtr camera_module_callbacks_ptr;
  arc::mojom::CameraModuleCallbacksRequest camera_module_callbacks_request =
      mojo::MakeRequest(&camera_module_callbacks_ptr);
  camera_module_callbacks_.Bind(std::move(camera_module_callbacks_request));
  camera_module_->SetCallbacks(
      std::move(camera_module_callbacks_ptr),
      base::Bind(&CameraHalDelegate::OnSetCallbacksOnIpcThread, this));
}

void CameraHalDelegate::OnSetCallbacksOnIpcThread(int32_t result) {
  DCHECK(ipc_task_runner_->BelongsToCurrentThread());
  if (result) {
    num_builtin_cameras_ = 0;
    builtin_camera_info_updated_.Signal();
    LOG(ERROR) << "Failed to set camera module callbacks: " << strerror(result);
    return;
  }
  for (size_t camera_id = 0; camera_id < num_builtin_cameras_; ++camera_id) {
    GetCameraInfoOnIpcThread(
        camera_id, base::Bind(&CameraHalDelegate::OnGotCameraInfoOnIpcThread,
                              this, camera_id));
  }
}

void CameraHalDelegate::GetCameraInfoOnIpcThread(
    int32_t camera_id,
    const GetCameraInfoCallback& callback) {
  DCHECK(ipc_task_runner_->BelongsToCurrentThread());
  camera_module_->GetCameraInfo(camera_id, callback);
}

void CameraHalDelegate::OnGotCameraInfoOnIpcThread(
    int32_t camera_id,
    int32_t result,
    arc::mojom::CameraInfoPtr camera_info) {
  DCHECK(ipc_task_runner_->BelongsToCurrentThread());
  DVLOG(1) << "Got camera info of camera " << camera_id;
  if (result) {
    LOG(ERROR) << "Failed to get camera info. Camera id: " << camera_id;
  }
  // In case of error |camera_info| is empty.
  camera_info_[std::to_string(camera_id)] = std::move(camera_info);
  if (camera_info_.size() == num_builtin_cameras_) {
    builtin_camera_info_updated_.Signal();
  }
}

void CameraHalDelegate::OpenDeviceOnIpcThread(
    int32_t camera_id,
    arc::mojom::Camera3DeviceOpsRequest device_ops_request,
    const OpenDeviceCallback& callback) {
  DCHECK(ipc_task_runner_->BelongsToCurrentThread());
  camera_module_->OpenDevice(camera_id, std::move(device_ops_request),
                             callback);
}

// CameraModuleCallbacks implementations.
void CameraHalDelegate::CameraDeviceStatusChange(
    int32_t camera_id,
    arc::mojom::CameraDeviceStatus new_status) {
  DCHECK(ipc_task_runner_->BelongsToCurrentThread());
  // TODO(jcliang): Handle status change for external cameras.
  NOTIMPLEMENTED() << "CameraDeviceStatusChange is not implemented";
}

}  // namespace media
