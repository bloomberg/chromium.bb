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

#include "base/containers/queue.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread.h"
#include "media/base/video_frame.h"
#include "media/base/video_frame_layout.h"
#include "media/gpu/image_processor.h"
#include "media/gpu/media_gpu_export.h"
#include "media/gpu/v4l2/v4l2_device.h"

namespace media {

// Handles image processing accelerators that expose a V4L2 memory-to-memory
// interface. The threading model of this class is the same as for other V4L2
// hardware accelerators (see V4L2VideoDecodeAccelerator) for more details.
class MEDIA_GPU_EXPORT V4L2ImageProcessor : public ImageProcessor {
 public:
  // ImageProcessor implementation.
  ~V4L2ImageProcessor() override;
  gfx::Size input_allocated_size() const override;
  gfx::Size output_allocated_size() const override;
  VideoFrame::StorageType input_storage_type() const override;
  VideoFrame::StorageType output_storage_type() const override;
  OutputMode output_mode() const override;
  bool Process(scoped_refptr<VideoFrame> frame,
               int output_buffer_index,
               std::vector<base::ScopedFD> output_dmabuf_fds,
               FrameReadyCB cb) override;
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
  // input_format to output_format. Caller shall provide input and output
  // storage type as well as output mode. The number of input buffers and output
  // buffers will be |num_buffers|. Provided |error_cb| will be posted to the
  // child thread if an error occurs after initialization. Returns nullptr if
  // V4L2ImageProcessor fails to create.
  // Note: output_mode will be removed once all its clients use import mode.
  static std::unique_ptr<V4L2ImageProcessor> Create(
      scoped_refptr<V4L2Device> device,
      VideoFrame::StorageType input_storage_type,
      VideoFrame::StorageType output_storage_type,
      OutputMode output_mode,
      const VideoFrameLayout& input_layout,
      const VideoFrameLayout& output_layout,
      gfx::Size input_visible_size,
      gfx::Size output_visible_size,
      size_t num_buffers,
      const base::Closure& error_cb);

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
                     const base::Closure& error_cb);

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
  void NotifyErrorOnChildThread(const base::Closure& error_cb);

  void ProcessTask(std::unique_ptr<JobRecord> job_record);
  void ServiceDeviceTask();

  // Attempt to start/stop device_poll_thread_.
  void StartDevicePoll();
  void StopDevicePoll();

  // Ran on device_poll_thread_ to wait for device events.
  void DevicePollTask(bool poll_device);

  // A processed frame is ready.
  void FrameReady(FrameReadyCB cb, scoped_refptr<VideoFrame> frame);

  // Stop all processing and clean up. After this method returns no more
  // callbacks will be invoked.
  void Destroy();

  // Stores input frame's format, coded_size, buffer and plane layout.
  const VideoFrameLayout input_layout_;
  const gfx::Size input_visible_size_;
  const v4l2_memory input_memory_type_;
  const VideoFrame::StorageType input_storage_type_;

  // Stores input frame's format, coded_size, buffer and plane layout.
  const VideoFrameLayout output_layout_;
  const gfx::Size output_visible_size_;
  const v4l2_memory output_memory_type_;
  const VideoFrame::StorageType output_storage_type_;
  const OutputMode output_mode_;

  // Our original calling task runner for the child thread.
  const scoped_refptr<base::SingleThreadTaskRunner> child_task_runner_;

  // V4L2 device in use.
  scoped_refptr<V4L2Device> device_;

  // Thread to communicate with the device on.
  base::Thread device_thread_;
  // Thread used to poll the V4L2 for events only.
  base::Thread device_poll_thread_;

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
  base::Closure error_cb_;

  // WeakPtr<> pointing to |this| for use in posting tasks from the device
  // worker threads back to the child thread.  Because the worker threads
  // are members of this class, any task running on those threads is guaranteed
  // that this object is still alive.  As a result, tasks posted from the child
  // thread to the device thread should use base::Unretained(this),
  // and tasks posted the other way should use |weak_this_|.
  base::WeakPtr<V4L2ImageProcessor> weak_this_;

  // Weak factory for producing weak pointers on the child thread.
  base::WeakPtrFactory<V4L2ImageProcessor> weak_this_factory_;

  DISALLOW_COPY_AND_ASSIGN(V4L2ImageProcessor);
};

}  // namespace media

#endif  // MEDIA_GPU_V4L2_V4L2_IMAGE_PROCESSOR_H_
