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
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_switches.h"
#include "media/base/video_frame.h"
#include "media/capture/video/video_capture_buffer_pool.h"
#include "media/capture/video/video_capture_buffer_tracker_factory_impl.h"
#include "media/capture/video/video_capture_device_client.h"

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
    UMA_HISTOGRAM_SPARSE_SLOWLY( \
        name, \
        (height) ? ((width) * 100) / (height) : kInfiniteRatio);

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

  // Buffers that are currently known to this client.
  std::vector<int> known_buffers;

  // Buffers currently held by this client.
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

VideoCaptureController::BufferState::BufferState(
    int buffer_id,
    int frame_feedback_id,
    media::VideoFrameConsumerFeedbackObserver* consumer_feedback_observer,
    media::FrameBufferPool* frame_buffer_pool,
    scoped_refptr<media::VideoFrame> frame)
    : buffer_id_(buffer_id),
      frame_feedback_id_(frame_feedback_id),
      consumer_feedback_observer_(consumer_feedback_observer),
      frame_buffer_pool_(frame_buffer_pool),
      frame_(std::move(frame)),
      max_consumer_utilization_(
          media::VideoFrameConsumerFeedbackObserver::kNoUtilizationRecorded),
      consumer_hold_count_(0) {}

VideoCaptureController::BufferState::~BufferState() = default;

VideoCaptureController::BufferState::BufferState(
    const VideoCaptureController::BufferState& other) = default;

void VideoCaptureController::BufferState::RecordConsumerUtilization(
    double utilization) {
  if (std::isfinite(utilization) && utilization >= 0.0) {
    max_consumer_utilization_ =
        std::max(max_consumer_utilization_, utilization);
  }
}

void VideoCaptureController::BufferState::IncreaseConsumerCount() {
  if (consumer_hold_count_ == 0)
    if (frame_buffer_pool_ != nullptr)
      frame_buffer_pool_->SetBufferHold(buffer_id_);
  consumer_hold_count_++;
}

void VideoCaptureController::BufferState::DecreaseConsumerCount() {
  consumer_hold_count_--;
  if (consumer_hold_count_ == 0) {
    if (consumer_feedback_observer_ != nullptr &&
        max_consumer_utilization_ !=
            media::VideoFrameConsumerFeedbackObserver::kNoUtilizationRecorded) {
      consumer_feedback_observer_->OnUtilizationReport(
          frame_feedback_id_, max_consumer_utilization_);
    }
    if (frame_buffer_pool_ != nullptr)
      frame_buffer_pool_->ReleaseBufferHold(buffer_id_);
    max_consumer_utilization_ =
        media::VideoFrameConsumerFeedbackObserver::kNoUtilizationRecorded;
  }
}

bool VideoCaptureController::BufferState::HasZeroConsumerHoldCount() {
  return consumer_hold_count_ == 0;
}

void VideoCaptureController::BufferState::SetConsumerFeedbackObserver(
    media::VideoFrameConsumerFeedbackObserver* consumer_feedback_observer) {
  consumer_feedback_observer_ = consumer_feedback_observer;
}

void VideoCaptureController::BufferState::SetFrameBufferPool(
    media::FrameBufferPool* frame_buffer_pool) {
  frame_buffer_pool_ = frame_buffer_pool;
}

VideoCaptureController::VideoCaptureController()
    : frame_buffer_pool_(nullptr),
      consumer_feedback_observer_(nullptr),
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

void VideoCaptureController::SetFrameBufferPool(
    std::unique_ptr<media::FrameBufferPool> frame_buffer_pool) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  frame_buffer_pool_ = std::move(frame_buffer_pool);
  // Update existing BufferState entries.
  for (auto& entry : buffer_id_to_state_map_)
    entry.second.SetFrameBufferPool(frame_buffer_pool_.get());
}

void VideoCaptureController::SetConsumerFeedbackObserver(
    std::unique_ptr<media::VideoFrameConsumerFeedbackObserver>
        consumer_feedback_observer) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  consumer_feedback_observer_ = std::move(consumer_feedback_observer);
  // Update existing BufferState entries.
  for (auto& entry : buffer_id_to_state_map_)
    entry.second.SetConsumerFeedbackObserver(consumer_feedback_observer_.get());
}

void VideoCaptureController::AddClient(
    VideoCaptureControllerID id,
    VideoCaptureControllerEventHandler* event_handler,
    media::VideoCaptureSessionId session_id,
    const media::VideoCaptureParams& params) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DVLOG(1) << "VideoCaptureController::AddClient() -- id=" << id
           << ", session_id=" << session_id
           << ", params.requested_format="
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

  // Take back all buffers held by the |client|.
  for (const auto& buffer_id : client->buffers_in_use)
    buffer_id_to_state_map_.at(buffer_id).DecreaseConsumerCount();
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

  BufferState& buffer_state = buffer_id_to_state_map_.at(buffer_id);
  buffer_state.RecordConsumerUtilization(consumer_resource_utilization);
  buffer_state.DecreaseConsumerCount();
  client->buffers_in_use.erase(buffers_in_use_entry_iter);
}

const media::VideoCaptureFormat&
VideoCaptureController::GetVideoCaptureFormat() const {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  return video_capture_format_;
}

void VideoCaptureController::OnIncomingCapturedVideoFrame(
    std::unique_ptr<media::VideoCaptureDevice::Client::Buffer> buffer,
    scoped_refptr<VideoFrame> frame) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(frame_buffer_pool_);
  const int buffer_id = buffer->id();
  DCHECK_NE(buffer_id, media::VideoCaptureBufferPool::kInvalidId);

  // Insert if not exists.
  const auto it =
      buffer_id_to_state_map_
          .insert(std::make_pair(
              buffer_id, BufferState(buffer_id, buffer->frame_feedback_id(),
                                     consumer_feedback_observer_.get(),
                                     frame_buffer_pool_.get(), frame)))
          .first;
  BufferState& buffer_state = it->second;
  DCHECK(buffer_state.HasZeroConsumerHoldCount());

  if (state_ == VIDEO_CAPTURE_STATE_STARTED) {
    if (!frame->metadata()->HasKey(VideoFrameMetadata::FRAME_RATE)) {
      frame->metadata()->SetDouble(VideoFrameMetadata::FRAME_RATE,
                                   video_capture_format_.frame_rate);
    }
    std::unique_ptr<base::DictionaryValue> metadata =
        frame->metadata()->CopyInternalValues();

    // Only I420 and Y16 pixel formats are currently supported.
    DCHECK(frame->format() == media::PIXEL_FORMAT_I420 ||
           frame->format() == media::PIXEL_FORMAT_Y16)
        << "Unsupported pixel format: "
        << media::VideoPixelFormatToString(frame->format());

    // Sanity-checks to confirm |frame| is actually being backed by |buffer|.
    DCHECK(frame->storage_type() == media::VideoFrame::STORAGE_SHMEM);
    DCHECK(frame->data(media::VideoFrame::kYPlane) >= buffer->data(0) &&
           (frame->data(media::VideoFrame::kYPlane) <
            (reinterpret_cast<const uint8_t*>(buffer->data(0)) +
             buffer->mapped_size())))
        << "VideoFrame does not appear to be backed by Buffer";

    for (const auto& client : controller_clients_) {
      if (client->session_closed || client->paused)
        continue;

      // On the first use of a buffer on a client, share the memory handles.
      auto known_buffers_entry_iter =
          std::find(std::begin(client->known_buffers),
                    std::end(client->known_buffers), buffer_id);
      bool is_new_buffer = false;
      if (known_buffers_entry_iter == std::end(client->known_buffers)) {
        client->known_buffers.push_back(buffer_id);
        is_new_buffer = true;
      }
      if (is_new_buffer)
        DoNewBufferOnIOThread(client.get(), buffer.get(), frame);

      client->event_handler->OnBufferReady(client->controller_id, buffer_id,
                                           frame);

      auto buffers_in_use_entry_iter =
          std::find(std::begin(client->buffers_in_use),
                    std::end(client->buffers_in_use), buffer_id);
      if (buffers_in_use_entry_iter == std::end(client->buffers_in_use))
        client->buffers_in_use.push_back(buffer_id);
      else
        DCHECK(false) << "Unexpected duplicate buffer: " << buffer_id;
      buffer_state.IncreaseConsumerCount();
    }
  }

  if (!has_received_frames_) {
    UMA_HISTOGRAM_COUNTS("Media.VideoCapture.Width",
                         frame->visible_rect().width());
    UMA_HISTOGRAM_COUNTS("Media.VideoCapture.Height",
                         frame->visible_rect().height());
    UMA_HISTOGRAM_ASPECT_RATIO("Media.VideoCapture.AspectRatio",
                               frame->visible_rect().width(),
                               frame->visible_rect().height());
    double frame_rate = 0.0f;
    if (!frame->metadata()->GetDouble(VideoFrameMetadata::FRAME_RATE,
                                      &frame_rate)) {
      frame_rate = video_capture_format_.frame_rate;
    }
    UMA_HISTOGRAM_COUNTS("Media.VideoCapture.FrameRate", frame_rate);
    has_received_frames_ = true;
  }
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

void VideoCaptureController::OnBufferDestroyed(int buffer_id_to_drop) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(frame_buffer_pool_);

  for (const auto& client : controller_clients_) {
    if (client->session_closed)
      continue;

    auto known_buffers_entry_iter =
        std::find(std::begin(client->known_buffers),
                  std::end(client->known_buffers), buffer_id_to_drop);
    if (known_buffers_entry_iter != std::end(client->known_buffers)) {
      client->known_buffers.erase(known_buffers_entry_iter);
      client->event_handler->OnBufferDestroyed(client->controller_id,
                                               buffer_id_to_drop);
    }
  }

  buffer_id_to_state_map_.erase(buffer_id_to_drop);
}

void VideoCaptureController::DoNewBufferOnIOThread(
    ControllerClient* client,
    media::VideoCaptureDevice::Client::Buffer* buffer,
    const scoped_refptr<media::VideoFrame>& frame) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK_EQ(media::VideoFrame::STORAGE_SHMEM, frame->storage_type());

  const int buffer_id = buffer->id();
  mojo::ScopedSharedBufferHandle handle =
      frame_buffer_pool_->GetHandleForTransit(buffer_id);
  client->event_handler->OnBufferCreated(client->controller_id,
                                         std::move(handle),
                                         buffer->mapped_size(), buffer_id);
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

}  // namespace content
