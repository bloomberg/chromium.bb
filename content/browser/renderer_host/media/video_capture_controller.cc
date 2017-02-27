// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/video_capture_controller.h"

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <set>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/sparse_histogram.h"
#include "build/build_config.h"
#include "components/display_compositor/gl_helper.h"
#include "content/browser/renderer_host/media/media_stream_manager.h"
#include "content/browser/renderer_host/media/video_capture_manager.h"
#include "content/common/video_capture.mojom.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_switches.h"
#include "media/base/video_frame.h"
#include "media/capture/video/video_capture_buffer_pool.h"
#include "media/capture/video/video_capture_buffer_tracker_factory_impl.h"
#include "media/capture/video/video_capture_device_client.h"
#include "mojo/public/cpp/system/platform_handle.h"

#if !defined(OS_ANDROID)
#include "content/browser/compositor/image_transport_factory.h"
#endif

using media::VideoCaptureFormat;
using media::VideoFrame;
using media::VideoFrameMetadata;

namespace content {

namespace {

static const int kInfiniteRatio = 99999;

#define UMA_HISTOGRAM_ASPECT_RATIO(name, width, height) \
  UMA_HISTOGRAM_SPARSE_SLOWLY(                          \
      name, (height) ? ((width)*100) / (height) : kInfiniteRatio);

}  // anonymous namespace

struct VideoCaptureController::ControllerClient {
  ControllerClient(VideoCaptureControllerID id,
                   VideoCaptureControllerEventHandler* handler,
                   media::VideoCaptureSessionId session_id,
                   const media::VideoCaptureParams& params)
      : controller_id(id),
        event_handler(handler),
        session_id(session_id),
        parameters(params),
        session_closed(false),
        paused(false) {}

  ~ControllerClient() {}

  // ID used for identifying this object.
  const VideoCaptureControllerID controller_id;
  VideoCaptureControllerEventHandler* const event_handler;

  const media::VideoCaptureSessionId session_id;
  const media::VideoCaptureParams parameters;

  std::vector<int> known_buffer_context_ids;
  // |buffer_context_id|s of buffers currently being consumed by this client.
  std::vector<int> buffers_in_use;

  // State of capture session, controlled by VideoCaptureManager directly. This
  // transitions to true as soon as StopSession() occurs, at which point the
  // client is sent an OnEnded() event. However, because the client retains a
  // VideoCaptureController* pointer, its ControllerClient entry lives on until
  // it unregisters itself via RemoveClient(), which may happen asynchronously.
  //
  // TODO(nick): If we changed the semantics of VideoCaptureHost so that
  // OnEnded() events were processed synchronously (with the RemoveClient() done
  // implicitly), we could avoid tracking this state here in the Controller, and
  // simplify the code in both places.
  bool session_closed;

  // Indicates whether the client is paused, if true, VideoCaptureController
  // stops updating its buffer.
  bool paused;
};

VideoCaptureController::BufferContext::BufferContext(
    int buffer_context_id,
    int buffer_id,
    media::VideoFrameConsumerFeedbackObserver* consumer_feedback_observer,
    mojo::ScopedSharedBufferHandle handle)
    : buffer_context_id_(buffer_context_id),
      buffer_id_(buffer_id),
      is_retired_(false),
      frame_feedback_id_(0),
      consumer_feedback_observer_(consumer_feedback_observer),
      buffer_handle_(std::move(handle)),
      max_consumer_utilization_(
          media::VideoFrameConsumerFeedbackObserver::kNoUtilizationRecorded),
      consumer_hold_count_(0) {}

VideoCaptureController::BufferContext::~BufferContext() = default;

VideoCaptureController::BufferContext::BufferContext(
    VideoCaptureController::BufferContext&& other) = default;

VideoCaptureController::BufferContext& VideoCaptureController::BufferContext::
operator=(BufferContext&& other) = default;

void VideoCaptureController::BufferContext::RecordConsumerUtilization(
    double utilization) {
  if (std::isfinite(utilization) && utilization >= 0.0) {
    max_consumer_utilization_ =
        std::max(max_consumer_utilization_, utilization);
  }
}

void VideoCaptureController::BufferContext::IncreaseConsumerCount() {
  consumer_hold_count_++;
}

void VideoCaptureController::BufferContext::DecreaseConsumerCount() {
  consumer_hold_count_--;
  if (consumer_hold_count_ == 0) {
    if (consumer_feedback_observer_ != nullptr &&
        max_consumer_utilization_ !=
            media::VideoFrameConsumerFeedbackObserver::kNoUtilizationRecorded) {
      consumer_feedback_observer_->OnUtilizationReport(
          frame_feedback_id_, max_consumer_utilization_);
    }
    buffer_read_permission_.reset();
    max_consumer_utilization_ =
        media::VideoFrameConsumerFeedbackObserver::kNoUtilizationRecorded;
  }
}

mojo::ScopedSharedBufferHandle
VideoCaptureController::BufferContext::CloneHandle() {
  return buffer_handle_->Clone();
}

VideoCaptureController::VideoCaptureController()
    : consumer_feedback_observer_(nullptr),
      state_(VIDEO_CAPTURE_STATE_STARTED),
      has_received_frames_(false),
      weak_ptr_factory_(this) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
}

VideoCaptureController::~VideoCaptureController() = default;

base::WeakPtr<VideoCaptureController>
VideoCaptureController::GetWeakPtrForIOThread() {
  return weak_ptr_factory_.GetWeakPtr();
}

void VideoCaptureController::SetConsumerFeedbackObserver(
    std::unique_ptr<media::VideoFrameConsumerFeedbackObserver>
        consumer_feedback_observer) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  consumer_feedback_observer_ = std::move(consumer_feedback_observer);
  // Update existing BufferContext entries.
  for (auto& entry : buffer_contexts_)
    entry.set_consumer_feedback_observer(consumer_feedback_observer_.get());
}

void VideoCaptureController::AddClient(
    VideoCaptureControllerID id,
    VideoCaptureControllerEventHandler* event_handler,
    media::VideoCaptureSessionId session_id,
    const media::VideoCaptureParams& params) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DVLOG(1) << "VideoCaptureController::AddClient() -- id=" << id
           << ", session_id=" << session_id << ", params.requested_format="
           << media::VideoCaptureFormat::ToString(params.requested_format);

  // Check that requested VideoCaptureParams are valid and supported.  If not,
  // report an error immediately and punt.
  if (!params.IsValid() ||
      !(params.requested_format.pixel_format == media::PIXEL_FORMAT_I420 ||
        params.requested_format.pixel_format == media::PIXEL_FORMAT_Y16) ||
      params.requested_format.pixel_storage != media::PIXEL_STORAGE_CPU) {
    // Crash in debug builds since the renderer should not have asked for
    // invalid or unsupported parameters.
    LOG(DFATAL) << "Invalid or unsupported video capture parameters requested: "
                << media::VideoCaptureFormat::ToString(params.requested_format);
    event_handler->OnError(id);
    return;
  }

  // If this is the first client added to the controller, cache the parameters.
  if (controller_clients_.empty())
    video_capture_format_ = params.requested_format;

  // Signal error in case device is already in error state.
  if (state_ == VIDEO_CAPTURE_STATE_ERROR) {
    event_handler->OnError(id);
    return;
  }

  // Do nothing if this client has called AddClient before.
  if (FindClient(id, event_handler, controller_clients_))
    return;

  std::unique_ptr<ControllerClient> client =
      base::MakeUnique<ControllerClient>(id, event_handler, session_id, params);
  // If we already have gotten frame_info from the device, repeat it to the new
  // client.
  if (state_ == VIDEO_CAPTURE_STATE_STARTED) {
    controller_clients_.push_back(std::move(client));
    return;
  }
}

int VideoCaptureController::RemoveClient(
    VideoCaptureControllerID id,
    VideoCaptureControllerEventHandler* event_handler) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DVLOG(1) << "VideoCaptureController::RemoveClient, id " << id;

  ControllerClient* client = FindClient(id, event_handler, controller_clients_);
  if (!client)
    return kInvalidMediaCaptureSessionId;

  for (const auto& buffer_id : client->buffers_in_use) {
    OnClientFinishedConsumingBuffer(
        client, buffer_id,
        media::VideoFrameConsumerFeedbackObserver::kNoUtilizationRecorded);
  }
  client->buffers_in_use.clear();

  int session_id = client->session_id;
  controller_clients_.remove_if(
      [client](const std::unique_ptr<ControllerClient>& ptr) {
        return ptr.get() == client;
      });

  return session_id;
}

void VideoCaptureController::PauseClient(
    VideoCaptureControllerID id,
    VideoCaptureControllerEventHandler* event_handler) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DVLOG(1) << "VideoCaptureController::PauseClient, id " << id;

  ControllerClient* client = FindClient(id, event_handler, controller_clients_);
  if (!client)
    return;

  DLOG_IF(WARNING, client->paused) << "Redundant client configuration";

  client->paused = true;
}

bool VideoCaptureController::ResumeClient(
    VideoCaptureControllerID id,
    VideoCaptureControllerEventHandler* event_handler) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DVLOG(1) << "VideoCaptureController::ResumeClient, id " << id;

  ControllerClient* client = FindClient(id, event_handler, controller_clients_);
  if (!client)
    return false;

  if (!client->paused) {
    DVLOG(1) << "Calling resume on unpaused client";
    return false;
  }

  client->paused = false;
  return true;
}

int VideoCaptureController::GetClientCount() const {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  return controller_clients_.size();
}

bool VideoCaptureController::HasActiveClient() const {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  for (const auto& client : controller_clients_) {
    if (!client->paused)
      return true;
  }
  return false;
}

bool VideoCaptureController::HasPausedClient() const {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  for (const auto& client : controller_clients_) {
    if (client->paused)
      return true;
  }
  return false;
}

void VideoCaptureController::StopSession(int session_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DVLOG(1) << "VideoCaptureController::StopSession, id " << session_id;

  ControllerClient* client = FindClient(session_id, controller_clients_);

  if (client) {
    client->session_closed = true;
    client->event_handler->OnEnded(client->controller_id);
  }
}

void VideoCaptureController::ReturnBuffer(
    VideoCaptureControllerID id,
    VideoCaptureControllerEventHandler* event_handler,
    int buffer_id,
    double consumer_resource_utilization) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  ControllerClient* client = FindClient(id, event_handler, controller_clients_);

  // If this buffer is not held by this client, or this client doesn't exist
  // in controller, do nothing.
  if (!client) {
    NOTREACHED();
    return;
  }
  auto buffers_in_use_entry_iter =
      std::find(std::begin(client->buffers_in_use),
                std::end(client->buffers_in_use), buffer_id);
  if (buffers_in_use_entry_iter == std::end(client->buffers_in_use)) {
    NOTREACHED();
    return;
  }
  client->buffers_in_use.erase(buffers_in_use_entry_iter);

  OnClientFinishedConsumingBuffer(client, buffer_id,
                                  consumer_resource_utilization);
}

const media::VideoCaptureFormat& VideoCaptureController::GetVideoCaptureFormat()
    const {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  return video_capture_format_;
}

void VideoCaptureController::OnNewBufferHandle(
    int buffer_id,
    std::unique_ptr<media::VideoCaptureDevice::Client::Buffer::HandleProvider>
        handle_provider) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(FindUnretiredBufferContextFromBufferId(buffer_id) ==
         buffer_contexts_.end());
  buffer_contexts_.emplace_back(
      next_buffer_context_id_++, buffer_id, consumer_feedback_observer_.get(),
      handle_provider->GetHandleForInterProcessTransit());
}

void VideoCaptureController::OnFrameReadyInBuffer(
    int buffer_id,
    int frame_feedback_id,
    std::unique_ptr<
        media::VideoCaptureDevice::Client::Buffer::ScopedAccessPermission>
        buffer_read_permission,
    media::mojom::VideoFrameInfoPtr frame_info) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK_NE(buffer_id, media::VideoCaptureBufferPool::kInvalidId);

  auto buffer_context_iter = FindUnretiredBufferContextFromBufferId(buffer_id);
  DCHECK(buffer_context_iter != buffer_contexts_.end());
  buffer_context_iter->set_frame_feedback_id(frame_feedback_id);
  DCHECK(!buffer_context_iter->HasConsumers());

  if (state_ == VIDEO_CAPTURE_STATE_STARTED) {
    const int buffer_context_id = buffer_context_iter->buffer_context_id();
    for (const auto& client : controller_clients_) {
      if (client->session_closed || client->paused)
        continue;

      // On the first use of a BufferContext for a particular client, call
      // OnBufferCreated().
      if (!base::ContainsValue(client->known_buffer_context_ids,
                               buffer_context_id)) {
        client->known_buffer_context_ids.push_back(buffer_context_id);
        const size_t mapped_size =
            media::VideoCaptureFormat(frame_info->coded_size, 0.0f,
                                      frame_info->pixel_format,
                                      frame_info->storage_type)
                .ImageAllocationSize();
        client->event_handler->OnBufferCreated(
            client->controller_id, buffer_context_iter->CloneHandle(),
            mapped_size, buffer_context_id);
      }

      client->event_handler->OnBufferReady(client->controller_id,
                                           buffer_context_id, frame_info);

      if (!base::ContainsValue(client->buffers_in_use, buffer_context_id))
        client->buffers_in_use.push_back(buffer_context_id);
      else
        NOTREACHED() << "Unexpected duplicate buffer: " << buffer_context_id;

      buffer_context_iter->IncreaseConsumerCount();
    }
    if (buffer_context_iter->HasConsumers()) {
      buffer_context_iter->set_read_permission(
          std::move(buffer_read_permission));
    }
  }

  if (!has_received_frames_) {
    UMA_HISTOGRAM_COUNTS("Media.VideoCapture.Width",
                         frame_info->coded_size.width());
    UMA_HISTOGRAM_COUNTS("Media.VideoCapture.Height",
                         frame_info->coded_size.height());
    UMA_HISTOGRAM_ASPECT_RATIO("Media.VideoCapture.AspectRatio",
                               frame_info->coded_size.width(),
                               frame_info->coded_size.height());
    double frame_rate = 0.0f;
    media::VideoFrameMetadata metadata;
    metadata.MergeInternalValuesFrom(*frame_info->metadata);
    if (!metadata.GetDouble(VideoFrameMetadata::FRAME_RATE, &frame_rate)) {
      frame_rate = video_capture_format_.frame_rate;
    }
    UMA_HISTOGRAM_COUNTS("Media.VideoCapture.FrameRate", frame_rate);
    has_received_frames_ = true;
  }
}

void VideoCaptureController::OnBufferRetired(int buffer_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  auto buffer_context_iter = FindUnretiredBufferContextFromBufferId(buffer_id);
  DCHECK(buffer_context_iter != buffer_contexts_.end());

  // If there are any clients still using the buffer, we need to allow them
  // to finish up. We need to hold on to the BufferContext entry until then,
  // because it contains the consumer hold.
  if (!buffer_context_iter->HasConsumers())
    ReleaseBufferContext(buffer_context_iter);
  else
    buffer_context_iter->set_is_retired();
}

void VideoCaptureController::OnError() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  state_ = VIDEO_CAPTURE_STATE_ERROR;

  for (const auto& client : controller_clients_) {
    if (client->session_closed)
      continue;
    client->event_handler->OnError(client->controller_id);
  }
}

void VideoCaptureController::OnLog(const std::string& message) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  MediaStreamManager::SendMessageToNativeLog("Video capture: " + message);
}

void VideoCaptureController::OnStarted() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  for (const auto& client : controller_clients_) {
    if (client->session_closed)
      continue;
    client->event_handler->OnStarted(client->controller_id);
  }
}

VideoCaptureController::ControllerClient* VideoCaptureController::FindClient(
    VideoCaptureControllerID id,
    VideoCaptureControllerEventHandler* handler,
    const ControllerClients& clients) {
  for (const auto& client : clients) {
    if (client->controller_id == id && client->event_handler == handler)
      return client.get();
  }
  return nullptr;
}

VideoCaptureController::ControllerClient* VideoCaptureController::FindClient(
    int session_id,
    const ControllerClients& clients) {
  for (const auto& client : clients) {
    if (client->session_id == session_id)
      return client.get();
  }
  return nullptr;
}

std::vector<VideoCaptureController::BufferContext>::iterator
VideoCaptureController::FindBufferContextFromBufferContextId(
    int buffer_context_id) {
  return std::find_if(buffer_contexts_.begin(), buffer_contexts_.end(),
                      [buffer_context_id](const BufferContext& entry) {
                        return entry.buffer_context_id() == buffer_context_id;
                      });
}

std::vector<VideoCaptureController::BufferContext>::iterator
VideoCaptureController::FindUnretiredBufferContextFromBufferId(int buffer_id) {
  return std::find_if(buffer_contexts_.begin(), buffer_contexts_.end(),
                      [buffer_id](const BufferContext& entry) {
                        return (entry.buffer_id() == buffer_id) &&
                               (entry.is_retired() == false);
                      });
}

void VideoCaptureController::OnClientFinishedConsumingBuffer(
    ControllerClient* client,
    int buffer_context_id,
    double consumer_resource_utilization) {
  auto buffer_context_iter =
      FindBufferContextFromBufferContextId(buffer_context_id);
  DCHECK(buffer_context_iter != buffer_contexts_.end());

  buffer_context_iter->RecordConsumerUtilization(consumer_resource_utilization);
  buffer_context_iter->DecreaseConsumerCount();
  if (!buffer_context_iter->HasConsumers() &&
      buffer_context_iter->is_retired()) {
    ReleaseBufferContext(buffer_context_iter);
  }
}

void VideoCaptureController::ReleaseBufferContext(
    const std::vector<BufferContext>::iterator& buffer_context_iter) {
  for (const auto& client : controller_clients_) {
    if (client->session_closed)
      continue;
    auto entry_iter = std::find(std::begin(client->known_buffer_context_ids),
                                std::end(client->known_buffer_context_ids),
                                buffer_context_iter->buffer_context_id());
    if (entry_iter != std::end(client->known_buffer_context_ids)) {
      client->known_buffer_context_ids.erase(entry_iter);
      client->event_handler->OnBufferDestroyed(
          client->controller_id, buffer_context_iter->buffer_context_id());
    }
  }
  buffer_contexts_.erase(buffer_context_iter);
}

}  // namespace content
