// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAPTURE_VIDEO_CHROMEOS_STREAM_BUFFER_MANAGER_H_
#define MEDIA_CAPTURE_VIDEO_CHROMEOS_STREAM_BUFFER_MANAGER_H_

#include "base/memory/weak_ptr.h"
#include "media/capture/video/chromeos/camera_device_delegate.h"
#include "media/capture/video/chromeos/mojo/arc_camera3.mojom.h"
#include "media/capture/video_capture_types.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace base {

class SharedMemory;

}  // namespace base

namespace media {

class CameraDeviceContext;

// StreamBufferManager is responsible for managing the buffers of the
// stream.  StreamBufferManager allocates buffers according to the given
// stream configuration, and circulates the buffers along with capture
// requests and results between Chrome and the camera HAL process.
class CAPTURE_EXPORT StreamBufferManager final
    : public arc::mojom::Camera3CallbackOps {
 public:
  StreamBufferManager(
      arc::mojom::Camera3CallbackOpsRequest callback_ops_request,
      std::unique_ptr<StreamCaptureInterface> capture_interface,
      CameraDeviceContext* device_context,
      scoped_refptr<base::SingleThreadTaskRunner> ipc_task_runner);

  ~StreamBufferManager() final;

  // Sets up the stream context and allocate buffers according to the
  // configuration specified in |stream|.
  void SetUpStreamAndBuffers(VideoCaptureFormat capture_format,
                             uint32_t partial_result_count,
                             arc::mojom::Camera3StreamPtr stream);

  // StartCapture is the entry point to starting the video capture.  The way
  // the video capture loop works is:
  //
  //  (1) If there is a free buffer, RegisterBuffer registers the buffer with
  //      the camera HAL.
  //  (2) Once the free buffer is registered, ProcessCaptureRequest is called
  //      to issue a capture request which will eventually fill the registered
  //      buffer.  Goto (1) to register the remaining free buffers.
  //  (3) The camera HAL returns the shutter time of a capture request through
  //      Notify, and the filled buffer through ProcessCaptureResult.
  //  (4) Once all the result metadata are collected,
  //      SubmitCaptureResultIfComplete is called to deliver the filled buffer
  //      to Chrome.  After the buffer is consumed by Chrome it is enqueued back
  //      to the free buffer queue.  Goto (1) to start another capture loop.
  void StartCapture(arc::mojom::CameraMetadataPtr settings);

  // Stops the capture loop.  After StopCapture is called |callback_ops_| is
  // unbound, so no new capture request or result will be processed.
  void StopCapture();

 private:
  friend class StreamBufferManagerTest;

  // Registers a free buffer, if any, to the camera HAL.
  void RegisterBuffer();

  // Calls ProcessCaptureRequest if the buffer specified by |buffer_id| is
  // successfully registered.
  void OnRegisteredBuffer(size_t buffer_id, int32_t result);

  // The capture request contains the buffer handle specified by |buffer_id|.
  void ProcessCaptureRequest(size_t buffer_id);
  // Calls RegisterBuffer to attempt to register any remaining free buffers.
  void OnProcessedCaptureRequest(int32_t result);

  // Camera3CallbackOps implementations.

  // ProcessCaptureResult receives the result metadata as well as the filled
  // buffer from camera HAL.  The result metadata may be divided and delivered
  // in several stages.  Before all the result metadata is received the
  // partial results are kept in |partial_results_|.
  void ProcessCaptureResult(arc::mojom::Camera3CaptureResultPtr result) final;

  // Notify receives the shutter time of capture requests and various errors
  // from camera HAL.  The shutter time is used as the timestamp in the video
  // frame delivered to Chrome.
  void Notify(arc::mojom::Camera3NotifyMsgPtr message) final;
  void HandleNotifyError(uint32_t frame_number,
                         uint64_t error_stream_id,
                         arc::mojom::Camera3ErrorMsgCode error_code);

  // Submits the captured buffer of frame |frame_number_| to Chrome if all the
  // required metadata and the captured buffer are received.  After the buffer
  // is submitted the function then enqueues the buffer to free buffer queue for
  // the next capture request.
  void SubmitCaptureResultIfComplete(uint32_t frame_number);
  void SubmitCaptureResult(uint32_t frame_number);

  mojo::Binding<arc::mojom::Camera3CallbackOps> callback_ops_;

  std::unique_ptr<StreamCaptureInterface> capture_interface_;

  CameraDeviceContext* device_context_;

  // Where all the Mojo IPC calls takes place.
  const scoped_refptr<base::SingleThreadTaskRunner> ipc_task_runner_;

  // A flag indicating whether the capture loops is running.
  bool capturing_;

  // The frame number.  Increased by one for each capture request sent; reset
  // to zero in AllocateAndStart.
  uint32_t frame_number_;

  struct StreamContext {
    StreamContext();
    ~StreamContext();
    // The actual pixel format used in the capture request.
    VideoCaptureFormat capture_format;
    // The camera HAL stream.
    arc::mojom::Camera3StreamPtr stream;
    // The request settings used in the capture request of this stream.
    arc::mojom::CameraMetadataPtr request_settings;
    // The allocated buffers of this stream.
    std::vector<std::unique_ptr<base::SharedMemory>> buffers;
    // The free buffers of this stream.  The queue stores indices into the
    // |buffers| vector.
    std::queue<size_t> free_buffers;
  };

  // The stream context of the preview stream.
  std::unique_ptr<StreamContext> stream_context_;

  // CaptureResult is used to hold the partial capture results for each frame.
  struct CaptureResult {
    CaptureResult();
    ~CaptureResult();
    // |reference_time| and |timestamp| are derived from the shutter time of
    // this frame.  They are be passed to |client_->OnIncomingCapturedData|
    // along with the |buffers| when the captured frame is submitted.
    base::TimeTicks reference_time;
    base::TimeDelta timestamp;
    // The result metadata.  Contains various information about the captured
    // frame.
    arc::mojom::CameraMetadataPtr metadata;
    // The buffer handle that hold the captured data of this frame.
    arc::mojom::Camera3StreamBufferPtr buffer;
    // The set of the partial metadata received.  For each capture result, the
    // total number of partial metadata should equal to
    // |partial_result_count_|.
    std::set<uint32_t> partial_metadata_received;
  };

  // The number of partial stages.  |partial_result_count_| is learned by
  // querying |static_metadata_|.  In case the result count is absent in
  // |static_metadata_|, it defaults to one which means all the result
  // metadata and captured buffer of a frame are returned together in one
  // shot.
  uint32_t partial_result_count_;

  // The shutter time of the first frame.  We derive the |timestamp| of a
  // frame using the difference between the frame's shutter time and
  // |first_frame_shutter_time_|.
  base::TimeTicks first_frame_shutter_time_;

  // Stores the partial capture results of the current in-flight frames.
  std::map<uint32_t, CaptureResult> partial_results_;

  base::WeakPtrFactory<StreamBufferManager> weak_ptr_factory_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(StreamBufferManager);
};

}  // namespace media

#endif  // MEDIA_CAPTURE_VIDEO_CHROMEOS_STREAM_BUFFER_MANAGER_H_
