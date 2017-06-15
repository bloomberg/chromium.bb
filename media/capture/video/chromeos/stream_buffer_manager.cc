// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/chromeos/stream_buffer_manager.h"

#include <sync/sync.h>

#include "base/memory/ptr_util.h"
#include "base/memory/shared_memory.h"
#include "media/capture/video/chromeos/camera_device_context.h"
#include "media/capture/video/chromeos/camera_metadata_utils.h"
#include "media/capture/video/chromeos/pixel_format_utils.h"
#include "mojo/edk/embedder/embedder.h"
#include "mojo/edk/embedder/scoped_platform_handle.h"

namespace media {

StreamBufferManager::StreamBufferManager(
    arc::mojom::Camera3CallbackOpsRequest callback_ops_request,
    std::unique_ptr<StreamCaptureInterface> capture_interface,
    CameraDeviceContext* device_context,
    scoped_refptr<base::SingleThreadTaskRunner> ipc_task_runner)
    : callback_ops_(this, std::move(callback_ops_request)),
      capture_interface_(std::move(capture_interface)),
      device_context_(device_context),
      ipc_task_runner_(std::move(ipc_task_runner)),
      capturing_(false),
      frame_number_(0),
      partial_result_count_(1),
      first_frame_shutter_time_(base::TimeTicks()),
      weak_ptr_factory_(this) {
  DCHECK(ipc_task_runner_->BelongsToCurrentThread());
  DCHECK(callback_ops_.is_bound());
  DCHECK(device_context_);
}

StreamBufferManager::~StreamBufferManager() {}

void StreamBufferManager::SetUpStreamAndBuffers(
    VideoCaptureFormat capture_format,
    uint32_t partial_result_count,
    arc::mojom::Camera3StreamPtr stream) {
  DCHECK(ipc_task_runner_->BelongsToCurrentThread());
  DCHECK(!stream_context_);

  VLOG(2) << "Stream " << stream->id << " configured: usage=" << stream->usage
          << " max_buffers=" << stream->max_buffers;

  const size_t kMaximumAllowedBuffers = 15;
  if (stream->max_buffers > kMaximumAllowedBuffers) {
    device_context_->SetErrorState(
        FROM_HERE, std::string("Camera HAL requested ") +
                       std::to_string(stream->max_buffers) +
                       std::string(" buffers which exceeds the allowed maximum "
                                   "number of buffers"));
    return;
  }

  partial_result_count_ = partial_result_count;
  stream_context_ = base::MakeUnique<StreamContext>();
  stream_context_->capture_format = capture_format;
  stream_context_->stream = std::move(stream);

  // Allocate buffers.
  size_t num_buffers = stream_context_->stream->max_buffers;
  stream_context_->buffers.resize(num_buffers);
  for (size_t j = 0; j < num_buffers; ++j) {
    const VideoCaptureFormat frame_format(
        gfx::Size(stream_context_->stream->width,
                  stream_context_->stream->height),
        0.0, stream_context_->capture_format.pixel_format);
    auto buffer = base::MakeUnique<base::SharedMemory>();
    base::SharedMemoryCreateOptions options;
    options.size = frame_format.ImageAllocationSize();
    options.share_read_only = false;
    bool ret = buffer->Create(options);
    if (!ret) {
      device_context_->SetErrorState(FROM_HERE,
                                     "Failed to create SharedMemory buffer");
      return;
    }
    ret = buffer->Map(buffer->requested_size());
    if (!ret) {
      device_context_->SetErrorState(FROM_HERE,
                                     "Failed to map SharedMemory buffer");
      return;
    }
    stream_context_->buffers[j] = std::move(buffer);
    stream_context_->free_buffers.push(j);
  }
  VLOG(2) << "Allocated " << stream_context_->stream->max_buffers << " buffers";
}

void StreamBufferManager::StartCapture(arc::mojom::CameraMetadataPtr settings) {
  DCHECK(ipc_task_runner_->BelongsToCurrentThread());
  DCHECK(stream_context_);
  DCHECK(stream_context_->request_settings.is_null());

  capturing_ = true;
  stream_context_->request_settings = std::move(settings);
  // We cannot use a loop to register all the free buffers in one shot here
  // because the camera HAL v3 API specifies that the client cannot call
  // ProcessCaptureRequest before the previous one returns.
  RegisterBuffer();
}

void StreamBufferManager::StopCapture() {
  DCHECK(ipc_task_runner_->BelongsToCurrentThread());
  capturing_ = false;
}

void StreamBufferManager::RegisterBuffer() {
  DCHECK(ipc_task_runner_->BelongsToCurrentThread());
  DCHECK(stream_context_);

  if (!capturing_) {
    return;
  }

  if (stream_context_->free_buffers.empty()) {
    return;
  }

  size_t buffer_id = stream_context_->free_buffers.front();
  stream_context_->free_buffers.pop();
  const base::SharedMemory* buffer = stream_context_->buffers[buffer_id].get();

  VideoPixelFormat buffer_format = stream_context_->capture_format.pixel_format;
  uint32_t drm_format = PixFormatChromiumToDrm(buffer_format);
  if (!drm_format) {
    device_context_->SetErrorState(
        FROM_HERE, std::string("Unsupported video pixel format") +
                       VideoPixelFormatToString(buffer_format));
    return;
  }
  arc::mojom::HalPixelFormat hal_pixel_format = stream_context_->stream->format;

  size_t num_planes = VideoFrame::NumPlanes(buffer_format);
  std::vector<StreamCaptureInterface::Plane> planes(num_planes);
  for (size_t i = 0; i < num_planes; ++i) {
    base::SharedMemoryHandle shm_handle = buffer->handle();
    // Wrap the platform handle.
    MojoHandle wrapped_handle;
    MojoResult result = mojo::edk::CreatePlatformHandleWrapper(
        mojo::edk::ScopedPlatformHandle(mojo::edk::PlatformHandle(
            base::SharedMemory::DuplicateHandle(shm_handle).GetHandle())),
        &wrapped_handle);
    if (result != MOJO_RESULT_OK) {
      device_context_->SetErrorState(FROM_HERE,
                                     "Failed to wrap shared memory handle");
      return;
    }
    planes[i].fd.reset(mojo::Handle(wrapped_handle));
    planes[i].stride = VideoFrame::RowBytes(
        i, buffer_format, stream_context_->capture_format.frame_size.width());
    if (!i) {
      planes[i].offset = 0;
    } else {
      planes[i].offset =
          planes[i - 1].offset +
          VideoFrame::PlaneSize(buffer_format, i - 1,
                                stream_context_->capture_format.frame_size)
              .GetArea();
    }
  }
  capture_interface_->RegisterBuffer(
      buffer_id, arc::mojom::Camera3DeviceOps::BufferType::SHM, drm_format,
      hal_pixel_format, stream_context_->stream->width,
      stream_context_->stream->height, std::move(planes),
      base::Bind(&StreamBufferManager::OnRegisteredBuffer,
                 weak_ptr_factory_.GetWeakPtr(), buffer_id));
  VLOG(2) << "Registered buffer " << buffer_id;
}

void StreamBufferManager::OnRegisteredBuffer(size_t buffer_id, int32_t result) {
  DCHECK(ipc_task_runner_->BelongsToCurrentThread());

  if (!capturing_) {
    return;
  }
  if (result) {
    device_context_->SetErrorState(FROM_HERE,
                                   std::string("Failed to register buffer: ") +
                                       std::string(strerror(result)));
    return;
  }
  ProcessCaptureRequest(buffer_id);
}

void StreamBufferManager::ProcessCaptureRequest(size_t buffer_id) {
  DCHECK(ipc_task_runner_->BelongsToCurrentThread());
  DCHECK(stream_context_);

  arc::mojom::Camera3StreamBufferPtr buffer =
      arc::mojom::Camera3StreamBuffer::New();
  buffer->stream_id = static_cast<uint64_t>(
      arc::mojom::Camera3RequestTemplate::CAMERA3_TEMPLATE_PREVIEW);
  buffer->buffer_id = buffer_id;
  buffer->status = arc::mojom::Camera3BufferStatus::CAMERA3_BUFFER_STATUS_OK;

  arc::mojom::Camera3CaptureRequestPtr request =
      arc::mojom::Camera3CaptureRequest::New();
  request->frame_number = frame_number_;
  request->settings = stream_context_->request_settings.Clone();
  request->output_buffers.push_back(std::move(buffer));

  capture_interface_->ProcessCaptureRequest(
      std::move(request),
      base::Bind(&StreamBufferManager::OnProcessedCaptureRequest,
                 weak_ptr_factory_.GetWeakPtr()));
  VLOG(2) << "Requested capture for frame " << frame_number_ << " with buffer "
          << buffer_id;
  frame_number_++;
}

void StreamBufferManager::OnProcessedCaptureRequest(int32_t result) {
  DCHECK(ipc_task_runner_->BelongsToCurrentThread());

  if (!capturing_) {
    return;
  }
  if (result) {
    device_context_->SetErrorState(
        FROM_HERE, std::string("Process capture request failed") +
                       std::string(strerror(result)));
    return;
  }
  RegisterBuffer();
}

void StreamBufferManager::ProcessCaptureResult(
    arc::mojom::Camera3CaptureResultPtr result) {
  DCHECK(ipc_task_runner_->BelongsToCurrentThread());

  if (!capturing_) {
    return;
  }
  uint32_t frame_number = result->frame_number;
  // A new partial result may be created in either ProcessCaptureResult or
  // Notify.
  CaptureResult& partial_result = partial_results_[frame_number];
  if (partial_results_.size() > stream_context_->stream->max_buffers) {
    device_context_->SetErrorState(
        FROM_HERE,
        "Received more capture results than the maximum number of buffers");
    return;
  }
  if (result->output_buffers) {
    if (result->output_buffers->size() != 1) {
      device_context_->SetErrorState(
          FROM_HERE,
          std::string("Incorrect number of output buffers received: ") +
              std::to_string(result->output_buffers->size()));
      return;
    }
    arc::mojom::Camera3StreamBufferPtr& stream_buffer =
        result->output_buffers.value()[0];
    VLOG(2) << "Received capture result for frame " << frame_number
            << " stream_id: " << stream_buffer->stream_id;
    // The camera HAL v3 API specifies that only one capture result can carry
    // the result buffer for any given frame number.
    if (!partial_result.buffer.is_null()) {
      device_context_->SetErrorState(
          FROM_HERE,
          std::string("Received multiple result buffers for frame ") +
              std::to_string(frame_number));
      return;
    } else {
      partial_result.buffer = std::move(stream_buffer);
      // If the buffer is marked as error it is due to either a request or a
      // buffer error.  In either case the content of the buffer must be dropped
      // and the buffer can be reused.  We simply submit the buffer here and
      // don't wait for any partial results.  SubmitCaptureResult() will drop
      // and reuse the buffer.
      if (partial_result.buffer->status ==
          arc::mojom::Camera3BufferStatus::CAMERA3_BUFFER_STATUS_ERROR) {
        SubmitCaptureResult(frame_number);
        return;
      }
    }
  }

  // |result->partial_result| is set to 0 if the capture result contains only
  // the result buffer handles and no result metadata.
  if (result->partial_result) {
    uint32_t result_id = result->partial_result;
    if (result_id > partial_result_count_) {
      device_context_->SetErrorState(
          FROM_HERE, std::string("Invalid partial_result id: ") +
                         std::to_string(result_id));
      return;
    }
    if (partial_result.partial_metadata_received.find(result_id) !=
        partial_result.partial_metadata_received.end()) {
      device_context_->SetErrorState(
          FROM_HERE, std::string("Received duplicated partial metadata: ") +
                         std::to_string(result_id));
      return;
    }
    partial_result.partial_metadata_received.insert(result_id);
    MergeMetadata(&partial_result.metadata, result->result);
  }

  SubmitCaptureResultIfComplete(frame_number);
}

void StreamBufferManager::Notify(arc::mojom::Camera3NotifyMsgPtr message) {
  DCHECK(ipc_task_runner_->BelongsToCurrentThread());

  if (!capturing_) {
    return;
  }
  if (message->type == arc::mojom::Camera3MsgType::CAMERA3_MSG_ERROR) {
    uint32_t frame_number = message->message->get_error()->frame_number;
    uint64_t error_stream_id = message->message->get_error()->error_stream_id;
    arc::mojom::Camera3ErrorMsgCode error_code =
        message->message->get_error()->error_code;
    HandleNotifyError(frame_number, error_stream_id, error_code);
  } else {  // arc::mojom::Camera3MsgType::CAMERA3_MSG_SHUTTER
    uint32_t frame_number = message->message->get_shutter()->frame_number;
    uint64_t shutter_time = message->message->get_shutter()->timestamp;
    // A new partial result may be created in either ProcessCaptureResult or
    // Notify.
    VLOG(2) << "Received shutter time for frame " << frame_number;
    if (!shutter_time) {
      device_context_->SetErrorState(
          FROM_HERE, std::string("Received invalid shutter time: ") +
                         std::to_string(shutter_time));
      return;
    }
    CaptureResult& partial_result = partial_results_[frame_number];
    if (partial_results_.size() > stream_context_->stream->max_buffers) {
      device_context_->SetErrorState(
          FROM_HERE,
          "Received more capture results than the maximum number of buffers");
      return;
    }
    // Shutter timestamp is in ns.
    base::TimeTicks reference_time =
        base::TimeTicks::FromInternalValue(shutter_time / 1000);
    partial_result.reference_time = reference_time;
    if (first_frame_shutter_time_.is_null()) {
      // Record the shutter time of the first frame for calculating the
      // timestamp.
      first_frame_shutter_time_ = reference_time;
    }
    partial_result.timestamp = reference_time - first_frame_shutter_time_;
    SubmitCaptureResultIfComplete(frame_number);
  }
}

void StreamBufferManager::HandleNotifyError(
    uint32_t frame_number,
    uint64_t error_stream_id,
    arc::mojom::Camera3ErrorMsgCode error_code) {
  std::string warning_msg;

  switch (error_code) {
    case arc::mojom::Camera3ErrorMsgCode::CAMERA3_MSG_ERROR_DEVICE:
      // Fatal error and no more frames will be produced by the device.
      device_context_->SetErrorState(FROM_HERE, "Fatal device error");
      return;

    case arc::mojom::Camera3ErrorMsgCode::CAMERA3_MSG_ERROR_REQUEST:
      // An error has occurred in processing the request; the request
      // specified by |frame_number| has been dropped by the camera device.
      // Subsequent requests are unaffected.
      //
      // The HAL will call ProcessCaptureResult with the buffers' state set to
      // STATUS_ERROR.  The content of the buffers will be dropped and the
      // buffers will be reused in SubmitCaptureResult.
      warning_msg =
          std::string("An error occurred while processing request for frame ") +
          std::to_string(frame_number);
      break;

    case arc::mojom::Camera3ErrorMsgCode::CAMERA3_MSG_ERROR_RESULT:
      // An error has occurred in producing the output metadata buffer for a
      // result; the output metadata will not be available for the frame
      // specified by |frame_number|.  Subsequent requests are unaffected.
      warning_msg = std::string(
                        "An error occurred while producing result "
                        "metadata for frame ") +
                    std::to_string(frame_number);
      break;

    case arc::mojom::Camera3ErrorMsgCode::CAMERA3_MSG_ERROR_BUFFER:
      // An error has occurred in placing the output buffer into a stream for
      // a request. |frame_number| specifies the request for which the buffer
      // was dropped, and |error_stream_id| specifies the stream that dropped
      // the buffer.
      //
      // The HAL will call ProcessCaptureResult with the buffer's state set to
      // STATUS_ERROR.  The content of the buffer will be dropped and the
      // buffer will be reused in SubmitCaptureResult.
      warning_msg =
          std::string(
              "An error occurred while filling output buffer of stream ") +
          std::to_string(error_stream_id) + std::string(" in frame ") +
          std::to_string(frame_number);
      break;

    default:
      // To eliminate the warning for not handling CAMERA3_MSG_NUM_ERRORS
      break;
  }

  LOG(WARNING) << warning_msg;
  device_context_->LogToClient(warning_msg);
  // If the buffer is already returned by the HAL, submit it and we're done.
  auto partial_result = partial_results_.find(frame_number);
  if (partial_result != partial_results_.end() &&
      !partial_result->second.buffer.is_null()) {
    SubmitCaptureResult(frame_number);
  }
}

void StreamBufferManager::SubmitCaptureResultIfComplete(uint32_t frame_number) {
  DCHECK(ipc_task_runner_->BelongsToCurrentThread());
  DCHECK(partial_results_.find(frame_number) != partial_results_.end());

  CaptureResult& partial_result = partial_results_[frame_number];
  if (partial_result.partial_metadata_received.size() < partial_result_count_ ||
      partial_result.buffer.is_null() ||
      partial_result.reference_time == base::TimeTicks()) {
    // We can only submit the result buffer when:
    //   1. All the result metadata are received, and
    //   2. The result buffer is received, and
    //   3. The the shutter time is received.
    return;
  }
  SubmitCaptureResult(frame_number);
}

void StreamBufferManager::SubmitCaptureResult(uint32_t frame_number) {
  DCHECK(ipc_task_runner_->BelongsToCurrentThread());
  DCHECK(partial_results_.find(frame_number) != partial_results_.end());

  CaptureResult& partial_result = partial_results_[frame_number];
  if (partial_results_.begin()->first != frame_number) {
    device_context_->SetErrorState(
        FROM_HERE, std::string("Received frame is out-of-order; expect ") +
                       std::to_string(partial_results_.begin()->first) +
                       std::string(" but got ") + std::to_string(frame_number));
    return;
  }

  VLOG(2) << "Submit capture result of frame " << frame_number;
  uint32_t buffer_id = partial_result.buffer->buffer_id;

  // Wait on release fence before delivering the result buffer to client.
  if (partial_result.buffer->release_fence.is_valid()) {
    const int kSyncWaitTimeoutMs = 1000;
    mojo::edk::ScopedPlatformHandle fence;
    MojoResult result = mojo::edk::PassWrappedPlatformHandle(
        partial_result.buffer->release_fence.release().value(), &fence);
    if (result != MOJO_RESULT_OK) {
      device_context_->SetErrorState(FROM_HERE,
                                     "Failed to unwrap release fence fd");
      return;
    }
    if (!sync_wait(fence.get().handle, kSyncWaitTimeoutMs)) {
      device_context_->SetErrorState(FROM_HERE,
                                     "Sync wait on release fence timed out");
      return;
    }
  }

  // Deliver the captured data to client and then re-queue the buffer.
  if (partial_result.buffer->status !=
      arc::mojom::Camera3BufferStatus::CAMERA3_BUFFER_STATUS_ERROR) {
    const base::SharedMemory* shm_buffer =
        stream_context_->buffers[buffer_id].get();
    device_context_->SubmitCapturedData(
        reinterpret_cast<uint8_t*>(shm_buffer->memory()),
        shm_buffer->mapped_size(), stream_context_->capture_format,
        partial_result.reference_time, partial_result.timestamp);
  }
  stream_context_->free_buffers.push(buffer_id);
  partial_results_.erase(frame_number);
  RegisterBuffer();
}

StreamBufferManager::StreamContext::StreamContext() {}

StreamBufferManager::StreamContext::~StreamContext() {}

StreamBufferManager::CaptureResult::CaptureResult()
    : metadata(arc::mojom::CameraMetadata::New()) {}

StreamBufferManager::CaptureResult::~CaptureResult() {}

}  // namespace media
