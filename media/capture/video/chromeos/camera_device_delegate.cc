// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/chromeos/camera_device_delegate.h"

#include <string>
#include <utility>
#include <vector>

#include "base/memory/ptr_util.h"
#include "media/base/bind_to_current_loop.h"
#include "media/capture/video/chromeos/camera_device_context.h"
#include "media/capture/video/chromeos/camera_hal_delegate.h"
#include "media/capture/video/chromeos/camera_metadata_utils.h"
#include "media/capture/video/chromeos/pixel_format_utils.h"
#include "media/capture/video/chromeos/stream_buffer_manager.h"
#include "mojo/edk/embedder/embedder.h"
#include "mojo/edk/embedder/scoped_platform_handle.h"

namespace media {

StreamCaptureInterface::Plane::Plane() {}

StreamCaptureInterface::Plane::~Plane() {}

class CameraDeviceDelegate::StreamCaptureInterfaceImpl final
    : public StreamCaptureInterface {
 public:
  StreamCaptureInterfaceImpl(
      base::WeakPtr<CameraDeviceDelegate> camera_device_delegate)
      : camera_device_delegate_(std::move(camera_device_delegate)) {}

  void RegisterBuffer(uint64_t buffer_id,
                      arc::mojom::Camera3DeviceOps::BufferType type,
                      uint32_t drm_format,
                      arc::mojom::HalPixelFormat hal_pixel_format,
                      uint32_t width,
                      uint32_t height,
                      std::vector<StreamCaptureInterface::Plane> planes,
                      base::OnceCallback<void(int32_t)> callback) final {
    if (camera_device_delegate_) {
      camera_device_delegate_->RegisterBuffer(
          buffer_id, type, drm_format, hal_pixel_format, width, height,
          std::move(planes), std::move(callback));
    }
  }

  void ProcessCaptureRequest(arc::mojom::Camera3CaptureRequestPtr request,
                             base::OnceCallback<void(int32_t)> callback) final {
    if (camera_device_delegate_) {
      camera_device_delegate_->ProcessCaptureRequest(std::move(request),
                                                     std::move(callback));
    }
  }

 private:
  const base::WeakPtr<CameraDeviceDelegate> camera_device_delegate_;
};

CameraDeviceDelegate::CameraDeviceDelegate(
    VideoCaptureDeviceDescriptor device_descriptor,
    scoped_refptr<CameraHalDelegate> camera_hal_delegate,
    scoped_refptr<base::SingleThreadTaskRunner> ipc_task_runner)
    : device_descriptor_(device_descriptor),
      camera_id_(std::stoi(device_descriptor.device_id)),
      camera_hal_delegate_(std::move(camera_hal_delegate)),
      ipc_task_runner_(std::move(ipc_task_runner)),
      weak_ptr_factory_(this) {}

CameraDeviceDelegate::~CameraDeviceDelegate() {}

void CameraDeviceDelegate::AllocateAndStart(
    const VideoCaptureParams& params,
    std::unique_ptr<VideoCaptureDevice::Client> client) {
  DCHECK(ipc_task_runner_->BelongsToCurrentThread());

  chrome_capture_params_ = params;
  device_context_.reset(new CameraDeviceContext(std::move(client)));
  device_context_->SetState(CameraDeviceContext::State::kStarting);

  // We need to get the static camera metadata of the camera device first.
  camera_hal_delegate_->GetCameraInfo(
      camera_id_, BindToCurrentLoop(base::Bind(
                      &CameraDeviceDelegate::OnGotCameraInfo, GetWeakPtr())));
}

void CameraDeviceDelegate::StopAndDeAllocate(
    base::Closure device_close_callback) {
  DCHECK(ipc_task_runner_->BelongsToCurrentThread());
  // StopAndDeAllocate may be called at any state except
  // CameraDeviceContext::State::kStopping.
  DCHECK_NE(device_context_->GetState(), CameraDeviceContext::State::kStopping);

  if (device_context_->GetState() == CameraDeviceContext::State::kStopped) {
    // In case of Mojo connection error the device may be stopped before
    // StopAndDeAllocate is called.
    std::move(device_close_callback).Run();
    return;
  }

  device_close_callback_ = std::move(device_close_callback);
  device_context_->SetState(CameraDeviceContext::State::kStopping);
  if (!device_ops_.is_bound()) {
    // The device delegate is in the process of opening the camera device.
    return;
  }
  stream_buffer_manager_->StopCapture();
  device_ops_->Close(base::Bind(&CameraDeviceDelegate::OnClosed, GetWeakPtr()));
}

void CameraDeviceDelegate::TakePhoto(
    VideoCaptureDevice::TakePhotoCallback callback) {
  DCHECK(ipc_task_runner_->BelongsToCurrentThread());
  // TODO(jcliang): Implement TakePhoto.
  NOTIMPLEMENTED() << "TakePhoto is not implemented";
}

void CameraDeviceDelegate::GetPhotoState(
    VideoCaptureDevice::GetPhotoStateCallback callback) {
  DCHECK(ipc_task_runner_->BelongsToCurrentThread());
  // TODO(jcliang): Implement GetPhotoState.
  NOTIMPLEMENTED() << "GetPhotoState is not implemented";
}

void CameraDeviceDelegate::SetPhotoOptions(
    mojom::PhotoSettingsPtr settings,
    VideoCaptureDevice::SetPhotoOptionsCallback callback) {
  DCHECK(ipc_task_runner_->BelongsToCurrentThread());
  // TODO(jcliang): Implement SetPhotoOptions.
  NOTIMPLEMENTED() << "SetPhotoOptions is not implemented";
}

void CameraDeviceDelegate::SetRotation(int rotation) {
  DCHECK(ipc_task_runner_->BelongsToCurrentThread());
  DCHECK(rotation >= 0 && rotation < 360 && rotation % 90 == 0);
  device_context_->SetRotation(rotation);
}

base::WeakPtr<CameraDeviceDelegate> CameraDeviceDelegate::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void CameraDeviceDelegate::OnMojoConnectionError() {
  DCHECK(ipc_task_runner_->BelongsToCurrentThread());

  if (device_context_->GetState() == CameraDeviceContext::State::kStopping) {
    // When in stopping state the camera HAL adapter may terminate the Mojo
    // channel before we do, in which case the OnClosed callback will not be
    // called.
    OnClosed(0);
  } else {
    // The Mojo channel terminated unexpectedly.
    stream_buffer_manager_->StopCapture();
    device_context_->SetState(CameraDeviceContext::State::kStopped);
    device_context_->SetErrorState(FROM_HERE, "Mojo connection error");
    ResetMojoInterface();
    // We cannnot reset |device_context_| here because
    // |device_context_->SetErrorState| above will call StopAndDeAllocate later
    // to handle the class destruction.
  }
}

void CameraDeviceDelegate::OnClosed(int32_t result) {
  DCHECK(ipc_task_runner_->BelongsToCurrentThread());
  DCHECK_EQ(device_context_->GetState(), CameraDeviceContext::State::kStopping);

  device_context_->SetState(CameraDeviceContext::State::kStopped);
  if (result) {
    device_context_->LogToClient(std::string("Failed to close device: ") +
                                 std::string(strerror(result)));
  }
  ResetMojoInterface();
  device_context_.reset();
  std::move(device_close_callback_).Run();
}

void CameraDeviceDelegate::ResetMojoInterface() {
  DCHECK(ipc_task_runner_->BelongsToCurrentThread());

  device_ops_.reset();
  stream_buffer_manager_.reset();
}

void CameraDeviceDelegate::OnGotCameraInfo(
    int32_t result,
    arc::mojom::CameraInfoPtr camera_info) {
  DCHECK(ipc_task_runner_->BelongsToCurrentThread());

  if (device_context_->GetState() != CameraDeviceContext::State::kStarting) {
    DCHECK_EQ(device_context_->GetState(),
              CameraDeviceContext::State::kStopping);
    OnClosed(0);
    return;
  }

  if (result) {
    device_context_->SetErrorState(FROM_HERE, "Failed to get camera info");
    return;
  }
  static_metadata_ = std::move(camera_info->static_camera_characteristics);
  // |device_ops_| is bound after the MakeRequest call.
  arc::mojom::Camera3DeviceOpsRequest device_ops_request =
      mojo::MakeRequest(&device_ops_);
  device_ops_.set_connection_error_handler(
      base::Bind(&CameraDeviceDelegate::OnMojoConnectionError, GetWeakPtr()));
  camera_hal_delegate_->OpenDevice(
      camera_id_, std::move(device_ops_request),
      BindToCurrentLoop(
          base::Bind(&CameraDeviceDelegate::OnOpenedDevice, GetWeakPtr())));
}

void CameraDeviceDelegate::OnOpenedDevice(int32_t result) {
  DCHECK(ipc_task_runner_->BelongsToCurrentThread());

  if (device_context_->GetState() != CameraDeviceContext::State::kStarting) {
    DCHECK_EQ(device_context_->GetState(),
              CameraDeviceContext::State::kStopping);
    OnClosed(0);
    return;
  }

  if (result) {
    device_context_->SetErrorState(FROM_HERE, "Failed to open camera device");
    return;
  }
  Initialize();
}

void CameraDeviceDelegate::Initialize() {
  DCHECK(ipc_task_runner_->BelongsToCurrentThread());
  DCHECK_EQ(device_context_->GetState(), CameraDeviceContext::State::kStarting);

  arc::mojom::Camera3CallbackOpsPtr callback_ops_ptr;
  arc::mojom::Camera3CallbackOpsRequest callback_ops_request =
      mojo::MakeRequest(&callback_ops_ptr);
  stream_buffer_manager_ = base::MakeUnique<StreamBufferManager>(
      std::move(callback_ops_request),
      base::MakeUnique<StreamCaptureInterfaceImpl>(GetWeakPtr()),
      device_context_.get(), ipc_task_runner_);
  device_ops_->Initialize(
      std::move(callback_ops_ptr),
      base::Bind(&CameraDeviceDelegate::OnInitialized, GetWeakPtr()));
}

void CameraDeviceDelegate::OnInitialized(int32_t result) {
  DCHECK(ipc_task_runner_->BelongsToCurrentThread());

  if (device_context_->GetState() != CameraDeviceContext::State::kStarting) {
    DCHECK_EQ(device_context_->GetState(),
              CameraDeviceContext::State::kStopping);
    return;
  }
  if (result) {
    device_context_->SetErrorState(
        FROM_HERE, std::string("Failed to initialize camera device") +
                       std::string(strerror(result)));
    return;
  }
  device_context_->SetState(CameraDeviceContext::State::kInitialized);
  ConfigureStreams();
}

void CameraDeviceDelegate::ConfigureStreams() {
  DCHECK(ipc_task_runner_->BelongsToCurrentThread());
  DCHECK_EQ(device_context_->GetState(),
            CameraDeviceContext::State::kInitialized);

  // Set up context for preview stream.
  arc::mojom::Camera3StreamPtr preview_stream =
      arc::mojom::Camera3Stream::New();
  preview_stream->id = static_cast<uint64_t>(
      arc::mojom::Camera3RequestTemplate::CAMERA3_TEMPLATE_PREVIEW);
  preview_stream->stream_type =
      arc::mojom::Camera3StreamType::CAMERA3_STREAM_OUTPUT;
  preview_stream->width =
      chrome_capture_params_.requested_format.frame_size.width();
  preview_stream->height =
      chrome_capture_params_.requested_format.frame_size.height();
  preview_stream->format =
      arc::mojom::HalPixelFormat::HAL_PIXEL_FORMAT_YCbCr_420_888;
  preview_stream->data_space = 0;
  preview_stream->rotation =
      arc::mojom::Camera3StreamRotation::CAMERA3_STREAM_ROTATION_0;

  arc::mojom::Camera3StreamConfigurationPtr stream_config =
      arc::mojom::Camera3StreamConfiguration::New();
  stream_config->streams.push_back(std::move(preview_stream));
  stream_config->operation_mode = arc::mojom::Camera3StreamConfigurationMode::
      CAMERA3_STREAM_CONFIGURATION_NORMAL_MODE;
  device_ops_->ConfigureStreams(
      std::move(stream_config),
      base::Bind(&CameraDeviceDelegate::OnConfiguredStreams, GetWeakPtr()));
}

void CameraDeviceDelegate::OnConfiguredStreams(
    int32_t result,
    arc::mojom::Camera3StreamConfigurationPtr updated_config) {
  DCHECK(ipc_task_runner_->BelongsToCurrentThread());

  if (device_context_->GetState() != CameraDeviceContext::State::kInitialized) {
    DCHECK_EQ(device_context_->GetState(),
              CameraDeviceContext::State::kStopping);
    return;
  }
  if (result) {
    device_context_->SetErrorState(
        FROM_HERE, std::string("Failed to configure streams: ") +
                       std::string(strerror(result)));
    return;
  }
  if (!updated_config || updated_config->streams.size() != 1) {
    device_context_->SetErrorState(
        FROM_HERE, std::string("Wrong number of streams configured: ") +
                       std::to_string(updated_config->streams.size()));
    return;
  }

  VideoCaptureFormat capture_format = chrome_capture_params_.requested_format;
  // TODO(jcliang): Determine the best format from metadata.
  capture_format.pixel_format = PIXEL_FORMAT_NV12;

  // The partial result count metadata is optional; defaults to 1 in case it
  // is not set in the static metadata.
  uint32_t partial_result_count = 1;
  const arc::mojom::CameraMetadataEntryPtr* partial_count = GetMetadataEntry(
      static_metadata_,
      arc::mojom::CameraMetadataTag::ANDROID_REQUEST_PARTIAL_RESULT_COUNT);
  if (partial_count) {
    partial_result_count =
        *reinterpret_cast<int32_t*>((*partial_count)->data.data());
  }
  stream_buffer_manager_->SetUpStreamAndBuffers(
      capture_format, partial_result_count,
      std::move(updated_config->streams[0]));

  device_context_->SetState(CameraDeviceContext::State::kStreamConfigured);
  ConstructDefaultRequestSettings();
}

void CameraDeviceDelegate::ConstructDefaultRequestSettings() {
  DCHECK(ipc_task_runner_->BelongsToCurrentThread());
  DCHECK_EQ(device_context_->GetState(),
            CameraDeviceContext::State::kStreamConfigured);

  device_ops_->ConstructDefaultRequestSettings(
      arc::mojom::Camera3RequestTemplate::CAMERA3_TEMPLATE_PREVIEW,
      base::Bind(&CameraDeviceDelegate::OnConstructedDefaultRequestSettings,
                 GetWeakPtr()));
}

void CameraDeviceDelegate::OnConstructedDefaultRequestSettings(
    arc::mojom::CameraMetadataPtr settings) {
  DCHECK(ipc_task_runner_->BelongsToCurrentThread());

  if (device_context_->GetState() !=
      CameraDeviceContext::State::kStreamConfigured) {
    DCHECK_EQ(device_context_->GetState(),
              CameraDeviceContext::State::kStopping);
    return;
  }
  if (!settings) {
    device_context_->SetErrorState(FROM_HERE,
                                   "Failed to get default request settings");
    return;
  }
  device_context_->SetState(CameraDeviceContext::State::kCapturing);
  stream_buffer_manager_->StartCapture(std::move(settings));
}

void CameraDeviceDelegate::RegisterBuffer(
    uint64_t buffer_id,
    arc::mojom::Camera3DeviceOps::BufferType type,
    uint32_t drm_format,
    arc::mojom::HalPixelFormat hal_pixel_format,
    uint32_t width,
    uint32_t height,
    std::vector<StreamCaptureInterface::Plane> planes,
    base::OnceCallback<void(int32_t)> callback) {
  DCHECK(ipc_task_runner_->BelongsToCurrentThread());

  if (device_context_->GetState() != CameraDeviceContext::State::kCapturing) {
    DCHECK_EQ(device_context_->GetState(),
              CameraDeviceContext::State::kStopping);
    return;
  }
  size_t num_planes = planes.size();
  std::vector<mojo::ScopedHandle> fds(num_planes);
  std::vector<uint32_t> strides(num_planes);
  std::vector<uint32_t> offsets(num_planes);
  for (size_t i = 0; i < num_planes; ++i) {
    fds[i] = std::move(planes[i].fd);
    strides[i] = planes[i].stride;
    offsets[i] = planes[i].offset;
  }
  device_ops_->RegisterBuffer(
      buffer_id, type, std::move(fds), drm_format, hal_pixel_format, width,
      height, std::move(strides), std::move(offsets), std::move(callback));
}

void CameraDeviceDelegate::ProcessCaptureRequest(
    arc::mojom::Camera3CaptureRequestPtr request,
    base::OnceCallback<void(int32_t)> callback) {
  DCHECK(ipc_task_runner_->BelongsToCurrentThread());

  if (device_context_->GetState() != CameraDeviceContext::State::kCapturing) {
    DCHECK_EQ(device_context_->GetState(),
              CameraDeviceContext::State::kStopping);
    return;
  }
  device_ops_->ProcessCaptureRequest(std::move(request), std::move(callback));
}

}  // namespace media
