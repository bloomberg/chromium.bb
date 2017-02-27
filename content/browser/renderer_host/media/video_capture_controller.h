// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_MEDIA_VIDEO_CAPTURE_CONTROLLER_H_
#define CONTENT_BROWSER_RENDERER_HOST_MEDIA_VIDEO_CAPTURE_CONTROLLER_H_

#include <list>
#include <memory>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/process/process.h"
#include "content/browser/renderer_host/media/video_capture_controller_event_handler.h"
#include "content/common/content_export.h"
#include "content/common/media/video_capture.h"
#include "media/capture/video/video_frame_receiver.h"
#include "media/capture/video_capture_types.h"

namespace content {

// Implementation of media::VideoFrameReceiver that distributes received frames
// to potentially multiple connected clients.
class CONTENT_EXPORT VideoCaptureController : public media::VideoFrameReceiver {
 public:
  VideoCaptureController();
  ~VideoCaptureController() override;

  base::WeakPtr<VideoCaptureController> GetWeakPtrForIOThread();

  // Factory code creating instances of VideoCaptureController may optionally
  // set a VideoFrameConsumerFeedbackObserver. Setting the observer is done in
  // this method separate from the constructor to allow clients to create and
  // use instances before they can provide the observer. (This is the case with
  // VideoCaptureManager).
  void SetConsumerFeedbackObserver(
      std::unique_ptr<media::VideoFrameConsumerFeedbackObserver> observer);

  // Start video capturing and try to use the resolution specified in |params|.
  // Buffers will be shared to the client as necessary. The client will continue
  // to receive frames from the device until RemoveClient() is called.
  void AddClient(VideoCaptureControllerID id,
                 VideoCaptureControllerEventHandler* event_handler,
                 media::VideoCaptureSessionId session_id,
                 const media::VideoCaptureParams& params);

  // Stop video capture. This will take back all buffers held by by
  // |event_handler|, and |event_handler| shouldn't use those buffers any more.
  // Returns the session_id of the stopped client, or
  // kInvalidMediaCaptureSessionId if the indicated client was not registered.
  int RemoveClient(VideoCaptureControllerID id,
                   VideoCaptureControllerEventHandler* event_handler);

  // Pause the video capture for specified client.
  void PauseClient(VideoCaptureControllerID id,
                   VideoCaptureControllerEventHandler* event_handler);
  // Resume the video capture for specified client.
  // Returns true if the client will be resumed.
  bool ResumeClient(VideoCaptureControllerID id,
                    VideoCaptureControllerEventHandler* event_handler);

  int GetClientCount() const;

  // Return true if there is client that isn't paused.
  bool HasActiveClient() const;

  // Return true if there is client paused.
  bool HasPausedClient() const;

  // API called directly by VideoCaptureManager in case the device is
  // prematurely closed.
  void StopSession(int session_id);

  // Return a buffer with id |buffer_id| previously given in
  // VideoCaptureControllerEventHandler::OnBufferReady.
  // If the consumer provided resource utilization
  // feedback, this will be passed here (-1.0 indicates no feedback).
  void ReturnBuffer(VideoCaptureControllerID id,
                    VideoCaptureControllerEventHandler* event_handler,
                    int buffer_id,
                    double consumer_resource_utilization);

  const media::VideoCaptureFormat& GetVideoCaptureFormat() const;

  bool has_received_frames() const { return has_received_frames_; }

  // Implementation of media::VideoFrameReceiver interface:
  void OnNewBufferHandle(
      int buffer_id,
      std::unique_ptr<media::VideoCaptureDevice::Client::Buffer::HandleProvider>
          handle_provider) override;
  void OnFrameReadyInBuffer(
      int buffer_id,
      int frame_feedback_id,
      std::unique_ptr<
          media::VideoCaptureDevice::Client::Buffer::ScopedAccessPermission>
          buffer_read_permission,
      media::mojom::VideoFrameInfoPtr frame_info) override;
  void OnBufferRetired(int buffer_id) override;
  void OnError() override;
  void OnLog(const std::string& message) override;
  void OnStarted() override;

 private:
  struct ControllerClient;
  typedef std::list<std::unique_ptr<ControllerClient>> ControllerClients;

  class BufferContext {
   public:
    BufferContext(
        int buffer_context_id,
        int buffer_id,
        media::VideoFrameConsumerFeedbackObserver* consumer_feedback_observer,
        mojo::ScopedSharedBufferHandle handle);
    ~BufferContext();
    BufferContext(BufferContext&& other);
    BufferContext& operator=(BufferContext&& other);
    int buffer_context_id() const { return buffer_context_id_; }
    int buffer_id() const { return buffer_id_; }
    bool is_retired() const { return is_retired_; }
    void set_is_retired() { is_retired_ = true; }
    void set_frame_feedback_id(int id) { frame_feedback_id_ = id; }
    void set_consumer_feedback_observer(
        media::VideoFrameConsumerFeedbackObserver* consumer_feedback_observer) {
      consumer_feedback_observer_ = consumer_feedback_observer;
    }
    void set_read_permission(
        std::unique_ptr<
            media::VideoCaptureDevice::Client::Buffer::ScopedAccessPermission>
            buffer_read_permission) {
      buffer_read_permission_ = std::move(buffer_read_permission);
    }
    void RecordConsumerUtilization(double utilization);
    void IncreaseConsumerCount();
    void DecreaseConsumerCount();
    bool HasConsumers() const { return consumer_hold_count_ > 0; }
    mojo::ScopedSharedBufferHandle CloneHandle();

   private:
    int buffer_context_id_;
    int buffer_id_;
    bool is_retired_;
    int frame_feedback_id_;
    media::VideoFrameConsumerFeedbackObserver* consumer_feedback_observer_;
    mojo::ScopedSharedBufferHandle buffer_handle_;
    double max_consumer_utilization_;
    int consumer_hold_count_;
    std::unique_ptr<
        media::VideoCaptureDevice::Client::Buffer::ScopedAccessPermission>
        buffer_read_permission_;
  };

  // Find a client of |id| and |handler| in |clients|.
  ControllerClient* FindClient(VideoCaptureControllerID id,
                               VideoCaptureControllerEventHandler* handler,
                               const ControllerClients& clients);

  // Find a client of |session_id| in |clients|.
  ControllerClient* FindClient(int session_id,
                               const ControllerClients& clients);

  std::vector<BufferContext>::iterator FindBufferContextFromBufferContextId(
      int buffer_context_id);
  std::vector<BufferContext>::iterator FindUnretiredBufferContextFromBufferId(
      int buffer_id);

  void OnClientFinishedConsumingBuffer(ControllerClient* client,
                                       int buffer_id,
                                       double consumer_resource_utilization);
  void ReleaseBufferContext(
      const std::vector<BufferContext>::iterator& buffer_state_iter);

  std::unique_ptr<media::VideoFrameConsumerFeedbackObserver>
      consumer_feedback_observer_;

  std::vector<BufferContext> buffer_contexts_;

  // All clients served by this controller.
  ControllerClients controller_clients_;

  // Takes on only the states 'STARTED' and 'ERROR'. 'ERROR' is an absorbing
  // state which stops the flow of data to clients.
  VideoCaptureState state_;

  int next_buffer_context_id_ = 0;

  // True if the controller has received a video frame from the device.
  bool has_received_frames_;

  media::VideoCaptureFormat video_capture_format_;

  base::WeakPtrFactory<VideoCaptureController> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(VideoCaptureController);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_MEDIA_VIDEO_CAPTURE_CONTROLLER_H_
