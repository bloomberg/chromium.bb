// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_V4L2_V4L2_IMAGE_PROCESSOR_H_
#define MEDIA_GPU_V4L2_V4L2_IMAGE_PROCESSOR_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <vector>

#include <linux/videodev2.h>

#include "base/callback_forward.h"
#include "base/containers/queue.h"
#include "base/files/scoped_file.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/threading/thread.h"
#include "base/threading/thread_checker.h"
#include "media/base/video_frame.h"
#include "media/base/video_frame_layout.h"
#include "media/gpu/image_processor.h"
#include "media/gpu/media_gpu_export.h"
#include "media/gpu/v4l2/v4l2_device.h"
#include "ui/gfx/geometry/size.h"

namespace media {

// Handles image processing accelerators that expose a V4L2 memory-to-memory
// interface. The threading model of this class is the same as for other V4L2
// hardware accelerators (see V4L2VideoDecodeAccelerator) for more details.
class MEDIA_GPU_EXPORT V4L2ImageProcessor : public ImageProcessor {
 public:
  // ImageProcessor implementation.
  ~V4L2ImageProcessor() override;
  bool Reset() override;

  // Returns true if image processing is supported on this platform.
  static bool IsSupported();

  // Returns a vector of supported input formats in fourcc.
  static std::vector<uint32_t> GetSupportedInputFormats();

  // Returns a vector of supported output formats in fourcc.
  static std::vector<uint32_t> GetSupportedOutputFormats();

  // Gets output allocated size and number of planes required by the device
  // for conversion from |input_pixelformat| to |output_pixelformat|, for
  // visible size |size|. Returns true on success. Adjusted coded size will be
  // stored in |size| and the number of planes will be stored in |num_planes|.
  static bool TryOutputFormat(uint32_t input_pixelformat,
                              uint32_t output_pixelformat,
                              gfx::Size* size,
                              size_t* num_planes);

  // Factory method to create V4L2ImageProcessor to convert from
  // input_config to output_config. The number of input buffers and output
  // buffers will be |num_buffers|. Provided |error_cb| will be posted to the
  // same thread Create() is called if an error occurs after initialization.
  // Returns nullptr if V4L2ImageProcessor fails to create.
  // Note: output_mode will be removed once all its clients use import mode.
  // TODO(crbug.com/917798): remove |device| parameter once
  //     V4L2VideoDecodeAccelerator no longer creates and uses
  //     |image_processor_device_| before V4L2ImageProcessor is created.
  static std::unique_ptr<V4L2ImageProcessor> Create(
      scoped_refptr<V4L2Device> device,
      const ImageProcessor::PortConfig& input_config,
      const ImageProcessor::PortConfig& output_config,
      const ImageProcessor::OutputMode output_mode,
      size_t num_buffers,
      ErrorCB error_cb);

 private:
  // Record for input buffers.
  struct InputRecord {
    InputRecord();
    InputRecord(const V4L2ImageProcessor::InputRecord&);
    ~InputRecord();
    scoped_refptr<VideoFrame> frame;
    bool at_device;
  };

  // Record for output buffers.
  struct OutputRecord {
    OutputRecord();
    OutputRecord(OutputRecord&&);
    ~OutputRecord();
    bool at_device;
    // The exported FDs of the frame will be stored here if
    // |output_memory_type_| is V4L2_MEMORY_MMAP
    std::vector<base::ScopedFD> dmabuf_fds;
  };

  // Job record. Jobs are processed in a FIFO order. This is separate from
  // InputRecord, because an InputRecord may be returned before we dequeue
  // the corresponding output buffer. The processed frame will be stored in
  // |output_buffer_index| output buffer. If |output_memory_type_| is
  // V4L2_MEMORY_DMABUF, the processed frame will be stored in
  // |output_dmabuf_fds|.
  struct JobRecord {
    JobRecord();
    ~JobRecord();
    scoped_refptr<VideoFrame> input_frame;
    int output_buffer_index;
    scoped_refptr<VideoFrame> output_frame;
    std::vector<base::ScopedFD> output_dmabuf_fds;
    FrameReadyCB ready_cb;
  };

  V4L2ImageProcessor(scoped_refptr<V4L2Device> device,
                     VideoFrame::StorageType input_storage_type,
                     VideoFrame::StorageType output_storage_type,
                     v4l2_memory input_memory_type,
                     v4l2_memory output_memory_type,
                     OutputMode output_mode,
                     const VideoFrameLayout& input_layout,
                     const VideoFrameLayout& output_layout,
                     gfx::Size input_visible_size,
                     gfx::Size output_visible_size,
                     size_t num_buffers,
                     ErrorCB error_cb);

  bool Initialize();
  void EnqueueInput();
  void EnqueueOutput(const JobRecord* job_record);
  void Dequeue();
  bool EnqueueInputRecord();
  bool EnqueueOutputRecord(const JobRecord* job_record);
  bool CreateInputBuffers();
  bool CreateOutputBuffers();
  void DestroyInputBuffers();
  void DestroyOutputBuffers();

  void NotifyError();

  // ImageProcessor implementation.
  bool ProcessInternal(scoped_refptr<VideoFrame> frame,
                       int output_buffer_index,
                       std::vector<base::ScopedFD> output_dmabuf_fds,
                       FrameReadyCB cb) override;
  bool ProcessInternal(scoped_refptr<VideoFrame> input_frame,
                       scoped_refptr<VideoFrame> output_frame,
                       FrameReadyCB cb) override;

  void ProcessTask(std::unique_ptr<JobRecord> job_record);
  void ServiceDeviceTask();

  // Attempt to start/stop device_poll_thread_.
  void StartDevicePoll();
  void StopDevicePoll();

  // Ran on device_poll_thread_ to wait for device events.
  void DevicePollTask(bool poll_device);

  // Stop all processing and clean up. After this method returns no more
  // callbacks will be invoked.
  void Destroy();

  // Stores input frame's visible size and v4l2_memory type.
  const gfx::Size input_visible_size_;
  const v4l2_memory input_memory_type_;

  // Stores output frame's visible size and v4l2_memory type.
  const gfx::Size output_visible_size_;
  const v4l2_memory output_memory_type_;

  // V4L2 device in use.
  scoped_refptr<V4L2Device> device_;

  // Thread to communicate with the device on.
  base::Thread device_thread_;
  // Thread used to poll the V4L2 for events only.
  base::Thread device_poll_thread_;

  // CancelableTaskTracker for ProcessTask().
  // Because ProcessTask is posted from |client_task_runner_|'s thread to
  // another sequence, |device_thread_|, it is unsafe to cancel the posted tasks
  // from |client_task_runner_|'s thread using CancelableCallback and WeakPtr
  // binding. CancelableTaskTracker is designed to deal with this scenario.
  base::CancelableTaskTracker process_task_tracker_;

  // All the below members are to be accessed from device_thread_ only
  // (if it's running).
  base::queue<std::unique_ptr<JobRecord>> input_queue_;
  base::queue<std::unique_ptr<JobRecord>> running_jobs_;

  // Input queue state.
  bool input_streamon_;
  // Number of input buffers enqueued to the device.
  int input_buffer_queued_count_;
  // Input buffers ready to use; LIFO since we don't care about ordering.
  std::vector<int> free_input_buffers_;
  // Mapping of int index to an input buffer record.
  std::vector<InputRecord> input_buffer_map_;

  // Output queue state.
  bool output_streamon_;
  // Number of output buffers enqueued to the device.
  int output_buffer_queued_count_;
  // Mapping of int index to an output buffer record.
  std::vector<OutputRecord> output_buffer_map_;
  // The number of input or output buffers.
  const size_t num_buffers_;

  // Error callback to the client.
  ErrorCB error_cb_;

  // Checker for the thread that creates this V4L2ImageProcessor.
  THREAD_CHECKER(client_thread_checker_);

  DISALLOW_COPY_AND_ASSIGN(V4L2ImageProcessor);
};

}  // namespace media

#endif  // MEDIA_GPU_V4L2_V4L2_IMAGE_PROCESSOR_H_
