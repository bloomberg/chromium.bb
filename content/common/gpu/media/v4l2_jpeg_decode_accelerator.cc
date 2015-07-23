// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <linux/videodev2.h>
#include <sys/mman.h>

#include "base/bind.h"
#include "base/thread_task_runner_handle.h"
#include "content/common/gpu/media/v4l2_jpeg_decode_accelerator.h"

#define IOCTL_OR_ERROR_RETURN_VALUE(type, arg, value, type_name)      \
  do {                                                                \
    if (device_->Ioctl(type, arg) != 0) {                             \
      PLOG(ERROR) << __func__ << "(): ioctl() failed: " << type_name; \
      PostNotifyError(kInvalidBitstreamBufferId, PLATFORM_FAILURE);   \
      return value;                                                   \
    }                                                                 \
  } while (0)

#define IOCTL_OR_ERROR_RETURN(type, arg) \
  IOCTL_OR_ERROR_RETURN_VALUE(type, arg, ((void)0), #type)

#define IOCTL_OR_ERROR_RETURN_FALSE(type, arg) \
  IOCTL_OR_ERROR_RETURN_VALUE(type, arg, false, #type)

#define IOCTL_OR_LOG_ERROR(type, arg)                               \
  do {                                                              \
    if (device_->Ioctl(type, arg) != 0) {                           \
      PLOG(ERROR) << __func__ << "(): ioctl() failed: " << #type;   \
      PostNotifyError(kInvalidBitstreamBufferId, PLATFORM_FAILURE); \
    }                                                               \
  } while (0)

namespace content {

V4L2JpegDecodeAccelerator::BufferRecord::BufferRecord()
    : address(nullptr), length(0), at_device(false) {
}

V4L2JpegDecodeAccelerator::BufferRecord::~BufferRecord() {
}

V4L2JpegDecodeAccelerator::JobRecord::JobRecord(
    media::BitstreamBuffer bitstream_buffer,
    scoped_refptr<media::VideoFrame> video_frame)
    : bitstream_buffer(bitstream_buffer), out_frame(video_frame) {
}

V4L2JpegDecodeAccelerator::JobRecord::~JobRecord() {
}

V4L2JpegDecodeAccelerator::V4L2JpegDecodeAccelerator(
    const scoped_refptr<V4L2Device>& device,
    const scoped_refptr<base::SingleThreadTaskRunner>& io_task_runner)
    : recreate_input_buffers_pending_(false),
      recreate_output_buffers_pending_(false),
      child_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      io_task_runner_(io_task_runner),
      client_(nullptr),
      device_(device),
      decoder_thread_("V4L2JpegDecodeThread"),
      device_poll_thread_("V4L2JpegDecodeDevicePollThread"),
      input_streamon_(false),
      output_streamon_(false),
      weak_factory_(this) {
  weak_ptr_ = weak_factory_.GetWeakPtr();
}

V4L2JpegDecodeAccelerator::~V4L2JpegDecodeAccelerator() {
  DCHECK(child_task_runner_->BelongsToCurrentThread());

  if (decoder_thread_.IsRunning()) {
    decoder_task_runner_->PostTask(
        FROM_HERE, base::Bind(&V4L2JpegDecodeAccelerator::DestroyTask,
                              base::Unretained(this)));
    decoder_thread_.Stop();
  }
  weak_factory_.InvalidateWeakPtrs();
  DCHECK(!device_poll_thread_.IsRunning());
}

void V4L2JpegDecodeAccelerator::DestroyTask() {
  DCHECK(decoder_task_runner_->BelongsToCurrentThread());
  while (!input_jobs_.empty())
    input_jobs_.pop();
  while (!running_jobs_.empty())
    running_jobs_.pop();

  // Stop streaming and the device_poll_thread_.
  StopDevicePoll();

  ResetQueues();
  DestroyInputBuffers();
  DestroyOutputBuffers();
}

void V4L2JpegDecodeAccelerator::VideoFrameReady(int32_t bitstream_buffer_id) {
  DCHECK(child_task_runner_->BelongsToCurrentThread());
  client_->VideoFrameReady(bitstream_buffer_id);
}

void V4L2JpegDecodeAccelerator::NotifyError(int32_t bitstream_buffer_id,
                                            Error error) {
  DCHECK(child_task_runner_->BelongsToCurrentThread());
  LOG(ERROR) << "Notifying of error " << error << " for buffer id "
             << bitstream_buffer_id;
  client_->NotifyError(bitstream_buffer_id, error);
}

void V4L2JpegDecodeAccelerator::PostNotifyError(
    int32_t bitstream_buffer_id,
    Error error) {
  child_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&V4L2JpegDecodeAccelerator::NotifyError, weak_ptr_,
                 bitstream_buffer_id, error));
}

bool V4L2JpegDecodeAccelerator::Initialize(Client* client) {
  DCHECK(child_task_runner_->BelongsToCurrentThread());

  // Capabilities check.
  struct v4l2_capability caps;
  const __u32 kCapsRequired = V4L2_CAP_STREAMING | V4L2_CAP_VIDEO_M2M;
  memset(&caps, 0, sizeof(caps));
  if (device_->Ioctl(VIDIOC_QUERYCAP, &caps) != 0) {
    PLOG(ERROR) << __func__ << "(): ioctl() failed: VIDIOC_QUERYCAP";
    return false;
  }
  if ((caps.capabilities & kCapsRequired) != kCapsRequired) {
    LOG(ERROR) << "Initialize(): VIDIOC_QUERYCAP, caps check failed: 0x"
               << std::hex << caps.capabilities;
    return false;
  }

  if (!decoder_thread_.Start()) {
    LOG(ERROR) << "Initialize(): decoder thread failed to start";
    return false;
  }
  client_ = client;
  decoder_task_runner_ = decoder_thread_.task_runner();

  decoder_task_runner_->PostTask(
      FROM_HERE, base::Bind(&V4L2JpegDecodeAccelerator::StartDevicePoll,
                            base::Unretained(this)));

  DVLOG(1) << "V4L2JpegDecodeAccelerator initialized.";
  return true;
}

void V4L2JpegDecodeAccelerator::Decode(
    const media::BitstreamBuffer& bitstream_buffer,
    const scoped_refptr<media::VideoFrame>& video_frame) {
  DVLOG(1) << "Decode(): input_id=" << bitstream_buffer.id()
           << ", size=" << bitstream_buffer.size();
  DCHECK(io_task_runner_->BelongsToCurrentThread());

  if (video_frame->format() != media::PIXEL_FORMAT_I420) {
    PostNotifyError(bitstream_buffer.id(), UNSUPPORTED_JPEG);
    return;
  }

  scoped_ptr<JobRecord> job_record(
      new JobRecord(bitstream_buffer, video_frame));

  decoder_task_runner_->PostTask(
      FROM_HERE, base::Bind(&V4L2JpegDecodeAccelerator::DecodeTask,
                            base::Unretained(this), base::Passed(&job_record)));
}

bool V4L2JpegDecodeAccelerator::IsSupported() {
  v4l2_fmtdesc fmtdesc;
  memset(&fmtdesc, 0, sizeof(fmtdesc));
  fmtdesc.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;

  for (; device_->Ioctl(VIDIOC_ENUM_FMT, &fmtdesc) == 0; ++fmtdesc.index) {
    if (fmtdesc.pixelformat == V4L2_PIX_FMT_JPEG)
      return true;
  }
  return false;
}

void V4L2JpegDecodeAccelerator::DecodeTask(scoped_ptr<JobRecord> job_record) {
  DCHECK(decoder_task_runner_->BelongsToCurrentThread());
  job_record->shm.reset(
      new base::SharedMemory(job_record->bitstream_buffer.handle(), true));
  if (!job_record->shm->Map(job_record->bitstream_buffer.size())) {
    PLOG(ERROR) << "DecodeTask(): could not map bitstream_buffer";
    PostNotifyError(job_record->bitstream_buffer.id(), UNREADABLE_INPUT);
    return;
  }
  input_jobs_.push(make_linked_ptr(job_record.release()));

  ServiceDeviceTask();
}

size_t V4L2JpegDecodeAccelerator::InputBufferQueuedCount() {
  return input_buffer_map_.size() - free_input_buffers_.size();
}

size_t V4L2JpegDecodeAccelerator::OutputBufferQueuedCount() {
  return output_buffer_map_.size() - free_output_buffers_.size();
}

bool V4L2JpegDecodeAccelerator::ShouldRecreateInputBuffers() {
  DCHECK(decoder_task_runner_->BelongsToCurrentThread());
  if (input_jobs_.empty())
    return false;

  linked_ptr<JobRecord> job_record = input_jobs_.front();
  // Check input buffer size is enough
  return (input_buffer_map_.empty() ||
      job_record->bitstream_buffer.size() > input_buffer_map_.front().length);
}

bool V4L2JpegDecodeAccelerator::ShouldRecreateOutputBuffers() {
  DCHECK(decoder_task_runner_->BelongsToCurrentThread());
  if (input_jobs_.empty())
    return false;

  linked_ptr<JobRecord> job_record = input_jobs_.front();
  // Check image resolution is the same as previous.
  if (job_record->out_frame->coded_size() != image_coded_size_) {
    size_t frame_size = media::VideoFrame::AllocationSize(
        job_record->out_frame->format(), job_record->out_frame->coded_size());
    if (output_buffer_map_.empty() ||
        frame_size > output_buffer_map_.front().length) {
      return true;
    }
  }
  return false;
}

bool V4L2JpegDecodeAccelerator::CreateBuffersIfNecessary() {
  DVLOG(3) << __func__;
  DCHECK(decoder_task_runner_->BelongsToCurrentThread());

  recreate_input_buffers_pending_ = ShouldRecreateInputBuffers();
  recreate_output_buffers_pending_ = ShouldRecreateOutputBuffers();
  if (!recreate_input_buffers_pending_ && !recreate_output_buffers_pending_)
    return true;

  // If running queue is not empty, we should wait until pending frames finish.
  if (!running_jobs_.empty())
    return true;

  if (input_streamon_ || output_streamon_) {
    ResetQueues();
    if (recreate_input_buffers_pending_)
      DestroyInputBuffers();

    if (recreate_output_buffers_pending_)
      DestroyOutputBuffers();
  }

  if (recreate_input_buffers_pending_ && !CreateInputBuffers()) {
    LOG(ERROR) << "Create input buffers failed.";
    return false;
  }
  if (recreate_output_buffers_pending_ && !CreateOutputBuffers()) {
    LOG(ERROR) << "Create output buffers failed.";
    return false;
  }

  return true;
}

bool V4L2JpegDecodeAccelerator::CreateInputBuffers() {
  DVLOG(3) << __func__;
  DCHECK(decoder_task_runner_->BelongsToCurrentThread());
  DCHECK(!input_streamon_);
  DCHECK(!input_jobs_.empty());
  linked_ptr<JobRecord> job_record = input_jobs_.front();
  // Reserve twice size to avoid recreating input buffer frequently.
  size_t reserve_size = job_record->bitstream_buffer.size() * 2;

  struct v4l2_format format;
  memset(&format, 0, sizeof(format));
  format.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
  format.fmt.pix.width = job_record->out_frame->coded_size().width();
  format.fmt.pix.height = job_record->out_frame->coded_size().height();
  format.fmt.pix.pixelformat = V4L2_PIX_FMT_JPEG;
  format.fmt.pix.sizeimage = reserve_size;
  format.fmt.pix.field = V4L2_FIELD_ANY;
  IOCTL_OR_ERROR_RETURN_FALSE(VIDIOC_S_FMT, &format);

  struct v4l2_requestbuffers reqbufs;
  memset(&reqbufs, 0, sizeof(reqbufs));
  reqbufs.count = kBufferCount;
  reqbufs.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
  reqbufs.memory = V4L2_MEMORY_MMAP;
  IOCTL_OR_ERROR_RETURN_FALSE(VIDIOC_REQBUFS, &reqbufs);

  DCHECK(input_buffer_map_.empty());
  input_buffer_map_.resize(reqbufs.count);

  for (size_t i = 0; i < input_buffer_map_.size(); ++i) {
    free_input_buffers_.push_back(i);

    struct v4l2_buffer buffer;
    memset(&buffer, 0, sizeof(buffer));
    buffer.index = i;
    buffer.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    buffer.memory = V4L2_MEMORY_MMAP;
    IOCTL_OR_ERROR_RETURN_FALSE(VIDIOC_QUERYBUF, &buffer);
    void* address = device_->Mmap(NULL, buffer.length, PROT_READ | PROT_WRITE,
                                  MAP_SHARED, buffer.m.offset);
    if (address == MAP_FAILED) {
      PLOG(ERROR) << "CreateInputBuffers(): mmap() failed";
      PostNotifyError(kInvalidBitstreamBufferId, PLATFORM_FAILURE);
      return false;
    }
    input_buffer_map_[i].address = address;
    input_buffer_map_[i].length = buffer.length;
  }
  recreate_input_buffers_pending_ = false;

  return true;
}

bool V4L2JpegDecodeAccelerator::CreateOutputBuffers() {
  DVLOG(3) << __func__;
  DCHECK(decoder_task_runner_->BelongsToCurrentThread());
  DCHECK(!output_streamon_);
  DCHECK(!input_jobs_.empty());
  linked_ptr<JobRecord> job_record = input_jobs_.front();

  size_t frame_size = media::VideoFrame::AllocationSize(
      media::PIXEL_FORMAT_I420, job_record->out_frame->coded_size());
  struct v4l2_format format;
  memset(&format, 0, sizeof(format));
  format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  format.fmt.pix.width = job_record->out_frame->coded_size().width();
  format.fmt.pix.height = job_record->out_frame->coded_size().height();
  format.fmt.pix.sizeimage = frame_size;
  format.fmt.pix.pixelformat = V4L2_PIX_FMT_YUV420;
  format.fmt.pix.field = V4L2_FIELD_ANY;
  IOCTL_OR_ERROR_RETURN_FALSE(VIDIOC_S_FMT, &format);

  struct v4l2_requestbuffers reqbufs;
  memset(&reqbufs, 0, sizeof(reqbufs));
  reqbufs.count = kBufferCount;
  reqbufs.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  reqbufs.memory = V4L2_MEMORY_MMAP;
  IOCTL_OR_ERROR_RETURN_FALSE(VIDIOC_REQBUFS, &reqbufs);

  DCHECK(output_buffer_map_.empty());
  output_buffer_map_.resize(reqbufs.count);

  for (size_t i = 0; i < output_buffer_map_.size(); ++i) {
    free_output_buffers_.push_back(i);

    struct v4l2_buffer buffer;
    memset(&buffer, 0, sizeof(buffer));
    buffer.index = i;
    buffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buffer.memory = V4L2_MEMORY_MMAP;
    IOCTL_OR_ERROR_RETURN_FALSE(VIDIOC_QUERYBUF, &buffer);
    void* address = device_->Mmap(NULL, buffer.length, PROT_READ | PROT_WRITE,
                                  MAP_SHARED, buffer.m.offset);
    if (address == MAP_FAILED) {
      PLOG(ERROR) << "CreateOutputBuffers(): mmap() failed";
      PostNotifyError(kInvalidBitstreamBufferId, PLATFORM_FAILURE);
      return false;
    }
    output_buffer_map_[i].address = address;
    output_buffer_map_[i].length = buffer.length;
  }
  image_coded_size_ = job_record->out_frame->coded_size();
  recreate_output_buffers_pending_ = false;

  return true;
}

void V4L2JpegDecodeAccelerator::DestroyInputBuffers() {
  DCHECK(decoder_task_runner_->BelongsToCurrentThread());
  DCHECK(!input_streamon_);

  for (size_t buf = 0; buf < input_buffer_map_.size(); ++buf) {
    BufferRecord& input_record = input_buffer_map_[buf];
    device_->Munmap(input_record.address, input_record.length);
  }

  struct v4l2_requestbuffers reqbufs;
  memset(&reqbufs, 0, sizeof(reqbufs));
  reqbufs.count = 0;
  reqbufs.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
  reqbufs.memory = V4L2_MEMORY_MMAP;
  IOCTL_OR_LOG_ERROR(VIDIOC_REQBUFS, &reqbufs);

  input_buffer_map_.clear();
  free_input_buffers_.clear();
}

void V4L2JpegDecodeAccelerator::DestroyOutputBuffers() {
  DCHECK(decoder_task_runner_->BelongsToCurrentThread());
  DCHECK(!output_streamon_);

  for (size_t buf = 0; buf < output_buffer_map_.size(); ++buf) {
    BufferRecord& output_record = output_buffer_map_[buf];
    device_->Munmap(output_record.address, output_record.length);
  }

  struct v4l2_requestbuffers reqbufs;
  memset(&reqbufs, 0, sizeof(reqbufs));
  reqbufs.count = 0;
  reqbufs.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  reqbufs.memory = V4L2_MEMORY_MMAP;
  IOCTL_OR_LOG_ERROR(VIDIOC_REQBUFS, &reqbufs);

  output_buffer_map_.clear();
  free_output_buffers_.clear();
}

void V4L2JpegDecodeAccelerator::DevicePollTask() {
  DCHECK(device_poll_task_runner_->BelongsToCurrentThread());

  bool event_pending;
  if (!device_->Poll(true, &event_pending)) {
    PLOG(ERROR) << "DevicePollTask(): Poll device error.";
    PostNotifyError(kInvalidBitstreamBufferId, PLATFORM_FAILURE);
    return;
  }

  // All processing should happen on ServiceDeviceTask(), since we shouldn't
  // touch decoder state from this thread.
  decoder_task_runner_->PostTask(
      FROM_HERE, base::Bind(&V4L2JpegDecodeAccelerator::ServiceDeviceTask,
                            base::Unretained(this)));
}

void V4L2JpegDecodeAccelerator::ServiceDeviceTask() {
  DCHECK(decoder_task_runner_->BelongsToCurrentThread());
  // If DestroyTask() shuts |device_poll_thread_| down, we should early-out.
  if (!device_poll_thread_.IsRunning())
    return;

  if (!running_jobs_.empty())
    Dequeue();
  if (!CreateBuffersIfNecessary())
    return;

  EnqueueInput();
  EnqueueOutput();

  if (!running_jobs_.empty()) {
    device_poll_task_runner_->PostTask(
        FROM_HERE, base::Bind(&V4L2JpegDecodeAccelerator::DevicePollTask,
                              base::Unretained(this)));
  }

  DVLOG(2) << __func__ << ": buffer counts: INPUT["
           << input_jobs_.size() << "] => DEVICE["
           << free_input_buffers_.size() << "/"
           << input_buffer_map_.size() << "->"
           << free_output_buffers_.size() << "/"
           << output_buffer_map_.size() << "]";
}

void V4L2JpegDecodeAccelerator::EnqueueInput() {
  DCHECK(decoder_task_runner_->BelongsToCurrentThread());
  while (!input_jobs_.empty() && !free_input_buffers_.empty()) {
    // Do not enqueue input record when input/output buffers are required to
    // re-create until all pending frames are handled by device.
    if (recreate_input_buffers_pending_ || recreate_output_buffers_pending_)
      break;
    if (!EnqueueInputRecord())
      return;
    recreate_input_buffers_pending_ = ShouldRecreateInputBuffers();
    recreate_output_buffers_pending_ = ShouldRecreateOutputBuffers();
  }
  // Check here because we cannot STREAMON before QBUF in earlier kernel.
  if (!input_streamon_ && InputBufferQueuedCount()) {
    __u32 type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    IOCTL_OR_ERROR_RETURN(VIDIOC_STREAMON, &type);
    input_streamon_ = true;
  }
}

void V4L2JpegDecodeAccelerator::EnqueueOutput() {
  DCHECK(decoder_task_runner_->BelongsToCurrentThread());
  // Output record can be enqueued because the output coded sizes of the frames
  // currently in the pipeline are all the same.
  while (running_jobs_.size() > OutputBufferQueuedCount() &&
      !free_output_buffers_.empty()) {
    if (!EnqueueOutputRecord())
      return;
  }
  // Check here because we cannot STREAMON before QBUF in earlier kernel.
  if (!output_streamon_ && OutputBufferQueuedCount()) {
    __u32 type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    IOCTL_OR_ERROR_RETURN(VIDIOC_STREAMON, &type);
    output_streamon_ = true;
  }
}

void V4L2JpegDecodeAccelerator::Dequeue() {
  DCHECK(decoder_task_runner_->BelongsToCurrentThread());

  // Dequeue completed input (VIDEO_OUTPUT) buffers,
  // and recycle to the free list.
  struct v4l2_buffer dqbuf;
  while (InputBufferQueuedCount() > 0) {
    DCHECK(input_streamon_);
    memset(&dqbuf, 0, sizeof(dqbuf));
    dqbuf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    dqbuf.memory = V4L2_MEMORY_MMAP;
    if (device_->Ioctl(VIDIOC_DQBUF, &dqbuf) != 0) {
      if (errno == EAGAIN) {
        // EAGAIN if we're just out of buffers to dequeue.
        break;
      }
      PLOG(ERROR) << "ioctl() failed: input buffer VIDIOC_DQBUF failed.";
      PostNotifyError(kInvalidBitstreamBufferId, PLATFORM_FAILURE);
      return;
    }
    BufferRecord& input_record = input_buffer_map_[dqbuf.index];
    DCHECK(input_record.at_device);
    input_record.at_device = false;
    free_input_buffers_.push_back(dqbuf.index);

    if (dqbuf.flags & V4L2_BUF_FLAG_ERROR) {
      DVLOG(1) << "Dequeue input buffer error.";
      PostNotifyError(kInvalidBitstreamBufferId, UNSUPPORTED_JPEG);
      running_jobs_.pop();
    }
  }

  // Dequeue completed output (VIDEO_CAPTURE) buffers, recycle to the free list.
  // Return the finished buffer to the client via the job ready callback.
  // If dequeued input buffer has an error, the error frame has removed from
  // |running_jobs_|. We check the size of |running_jobs_| instead of
  // OutputBufferQueueCount() to avoid calling DQBUF unnecessarily.
  while (!running_jobs_.empty()) {
    DCHECK(OutputBufferQueuedCount() > 0);
    DCHECK(output_streamon_);
    memset(&dqbuf, 0, sizeof(dqbuf));
    dqbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    dqbuf.memory = V4L2_MEMORY_MMAP;
    if (device_->Ioctl(VIDIOC_DQBUF, &dqbuf) != 0) {
      if (errno == EAGAIN) {
        // EAGAIN if we're just out of buffers to dequeue.
        break;
      }
      PLOG(ERROR) << "ioctl() failed: output buffer VIDIOC_DQBUF failed.";
      PostNotifyError(kInvalidBitstreamBufferId, PLATFORM_FAILURE);
      return;
    }
    BufferRecord& output_record = output_buffer_map_[dqbuf.index];
    DCHECK(output_record.at_device);
    output_record.at_device = false;
    free_output_buffers_.push_back(dqbuf.index);

    // Jobs are always processed in FIFO order.
    linked_ptr<JobRecord> job_record = running_jobs_.front();
    running_jobs_.pop();

    if (dqbuf.flags & V4L2_BUF_FLAG_ERROR) {
      DVLOG(1) << "Dequeue output buffer error.";
      PostNotifyError(kInvalidBitstreamBufferId, UNSUPPORTED_JPEG);
    } else {
      memcpy(job_record->out_frame->data(media::VideoFrame::kYPlane),
             output_record.address, output_record.length);

      DVLOG(3) << "Decoding finished, returning bitstream buffer, id="
               << job_record->bitstream_buffer.id();

      child_task_runner_->PostTask(
          FROM_HERE,
          base::Bind(&V4L2JpegDecodeAccelerator::VideoFrameReady, weak_ptr_,
                     job_record->bitstream_buffer.id()));
    }
  }
}

bool V4L2JpegDecodeAccelerator::EnqueueInputRecord() {
  DCHECK(!input_jobs_.empty());
  DCHECK(!free_input_buffers_.empty());

  // Enqueue an input (VIDEO_OUTPUT) buffer for an input video frame.
  linked_ptr<JobRecord> job_record = input_jobs_.front();
  input_jobs_.pop();
  const int index = free_input_buffers_.back();
  BufferRecord& input_record = input_buffer_map_[index];
  DCHECK(!input_record.at_device);

  struct v4l2_buffer qbuf;
  memset(&qbuf, 0, sizeof(qbuf));
  memcpy(input_record.address, job_record->shm->memory(),
         job_record->bitstream_buffer.size());
  qbuf.index = index;
  qbuf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
  qbuf.memory = V4L2_MEMORY_MMAP;
  IOCTL_OR_ERROR_RETURN_FALSE(VIDIOC_QBUF, &qbuf);
  input_record.at_device = true;
  running_jobs_.push(job_record);
  free_input_buffers_.pop_back();

  DVLOG(3) << __func__ << ": enqueued frame id="
           << job_record->bitstream_buffer.id() << " to device.";
  return true;
}

bool V4L2JpegDecodeAccelerator::EnqueueOutputRecord() {
  DCHECK(!free_output_buffers_.empty());

  // Enqueue an output (VIDEO_CAPTURE) buffer.
  const int index = free_output_buffers_.back();
  BufferRecord& output_record = output_buffer_map_[index];
  DCHECK(!output_record.at_device);
  struct v4l2_buffer qbuf;
  memset(&qbuf, 0, sizeof(qbuf));
  qbuf.index = index;
  qbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  qbuf.memory = V4L2_MEMORY_MMAP;
  IOCTL_OR_ERROR_RETURN_FALSE(VIDIOC_QBUF, &qbuf);
  output_record.at_device = true;
  free_output_buffers_.pop_back();
  return true;
}

void V4L2JpegDecodeAccelerator::ResetQueues() {
  if (input_streamon_) {
    __u32 type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
    IOCTL_OR_ERROR_RETURN(VIDIOC_STREAMOFF, &type);
    input_streamon_ = false;
  }

  if (output_streamon_) {
    __u32 type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    IOCTL_OR_ERROR_RETURN(VIDIOC_STREAMOFF, &type);
    output_streamon_ = false;
  }

  free_input_buffers_.clear();
  for (size_t i = 0; i < input_buffer_map_.size(); ++i) {
    BufferRecord& input_record = input_buffer_map_[i];
    input_record.at_device = false;
    free_input_buffers_.push_back(i);
  }

  free_output_buffers_.clear();
  for (size_t i = 0; i < output_buffer_map_.size(); ++i) {
    BufferRecord& output_record = output_buffer_map_[i];
    output_record.at_device = false;
    free_output_buffers_.push_back(i);
  }
}

void V4L2JpegDecodeAccelerator::StartDevicePoll() {
  DVLOG(3) << __func__ << ": starting device poll";
  DCHECK(decoder_task_runner_->BelongsToCurrentThread());
  DCHECK(!device_poll_thread_.IsRunning());

  if (!device_poll_thread_.Start()) {
    LOG(ERROR) << "StartDevicePoll(): Device thread failed to start";
    PostNotifyError(kInvalidBitstreamBufferId, PLATFORM_FAILURE);
    return;
  }
  device_poll_task_runner_ = device_poll_thread_.task_runner();
}

bool V4L2JpegDecodeAccelerator::StopDevicePoll() {
  DVLOG(3) << __func__ << ": stopping device poll";
  // Signal the DevicePollTask() to stop, and stop the device poll thread.
  if (!device_->SetDevicePollInterrupt()) {
    LOG(ERROR) << "StopDevicePoll(): SetDevicePollInterrupt failed.";
    PostNotifyError(kInvalidBitstreamBufferId, PLATFORM_FAILURE);
    return false;
  }

  device_poll_thread_.Stop();

  // Clear the interrupt now, to be sure.
  if (!device_->ClearDevicePollInterrupt())
    return false;

  return true;
}

}  // namespace content
