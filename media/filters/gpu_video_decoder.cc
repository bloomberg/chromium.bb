// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/gpu_video_decoder.h"

#include "base/bind.h"
#include "base/message_loop.h"
#include "base/stl_util.h"
#include "media/base/demuxer_stream.h"
#include "media/base/filter_host.h"
#include "media/base/video_decoder_config.h"
#include "media/ffmpeg/ffmpeg_common.h"

namespace media {

GpuVideoDecoder::Factories::~Factories() {}

// Size of shared-memory segments we allocate.  Since we reuse them we let them
// be on the beefy side.
static const size_t kSharedMemorySegmentBytes = 100 << 10;

GpuVideoDecoder::SHMBuffer::SHMBuffer(base::SharedMemory* m, size_t s)
    : shm(m), size(s) {
}

GpuVideoDecoder::SHMBuffer::~SHMBuffer() {}

GpuVideoDecoder::BufferPair::BufferPair(
    SHMBuffer* s, const scoped_refptr<Buffer>& b) : shm_buffer(s), buffer(b) {
}

GpuVideoDecoder::BufferPair::~BufferPair() {}

GpuVideoDecoder::GpuVideoDecoder(
    MessageLoop* message_loop,
    Factories* factories)
    : message_loop_(message_loop),
      factories_(factories),
      flush_in_progress_(false),
      demuxer_read_in_progress_(false),
      next_picture_buffer_id_(0),
      next_bitstream_buffer_id_(0) {
  DCHECK(message_loop_ && factories_.get());
}

GpuVideoDecoder::~GpuVideoDecoder() {
  DCHECK(!vda_);  // Stop should have been already called.
  STLDeleteElements(&available_shm_segments_);
  for (std::map<int32, BufferPair>::iterator it =
           bitstream_buffers_in_decoder_.begin();
       it != bitstream_buffers_in_decoder_.end(); ++it) {
    it->second.shm_buffer->shm->Close();
  }
  bitstream_buffers_in_decoder_.clear();
}

void GpuVideoDecoder::Stop(const base::Closure& callback) {
  if (MessageLoop::current() != message_loop_) {
    message_loop_->PostTask(FROM_HERE, base::Bind(
        &GpuVideoDecoder::Stop, this, callback));
    return;
  }
  if (!vda_) {
    callback.Run();
    return;
  }
  vda_->Destroy();
  vda_ = NULL;
  callback.Run();
}

void GpuVideoDecoder::Seek(base::TimeDelta time, const FilterStatusCB& cb) {
  if (MessageLoop::current() != message_loop_) {
    message_loop_->PostTask(FROM_HERE, base::Bind(
        &GpuVideoDecoder::Seek, this, time, cb));
    return;
  }
  pts_stream_.Seek(time);
  cb.Run(PIPELINE_OK);
}

void GpuVideoDecoder::Pause(const base::Closure& callback)  {
  if (MessageLoop::current() != message_loop_) {
    message_loop_->PostTask(FROM_HERE, base::Bind(
        &GpuVideoDecoder::Pause, this, callback));
    return;
  }
  callback.Run();
}

void GpuVideoDecoder::Flush(const base::Closure& callback)  {
  if (MessageLoop::current() != message_loop_) {
    message_loop_->PostTask(FROM_HERE, base::Bind(
        &GpuVideoDecoder::Flush, this, callback));
    return;
  }
  // Pipeline should have quiesced (via Pause() to all filters) before calling
  // us, so there should be nothing pending.
  DCHECK(pending_read_cb_.is_null());

  // Throw away any already-decoded frames.
  ready_video_frames_.clear();

  if (!vda_) {
    callback.Run();
    return;
  }
  DCHECK(pending_flush_cb_.is_null());
  pending_flush_cb_ = callback;
  pts_stream_.Flush();
  vda_->Reset();
}

void GpuVideoDecoder::Initialize(DemuxerStream* demuxer_stream,
                                 const PipelineStatusCB& callback,
                                 const StatisticsCallback& stats_callback) {
  if (MessageLoop::current() != message_loop_) {
    message_loop_->PostTask(FROM_HERE, base::Bind(
        &GpuVideoDecoder::Initialize, this,
        make_scoped_refptr(demuxer_stream), callback, stats_callback));
    return;
  }

  DCHECK(!demuxer_stream_);
  if (!demuxer_stream) {
    callback.Run(PIPELINE_ERROR_DECODE);
    return;
  }

  const VideoDecoderConfig& config = demuxer_stream->video_decoder_config();
  // TODO(scherkus): this check should go in PipelineImpl prior to creating
  // decoder objects.
  if (!config.IsValidConfig()) {
    DLOG(ERROR) << "Invalid video stream - " << config.AsHumanReadableString();
    callback.Run(PIPELINE_ERROR_DECODE);
    return;
  }

  vda_ = factories_->CreateVideoDecodeAccelerator(config.profile(), this);
  if (!vda_) {
    callback.Run(DECODER_ERROR_NOT_SUPPORTED);
    return;
  }

  demuxer_stream_ = demuxer_stream;
  statistics_callback_ = stats_callback;

  demuxer_stream_->EnableBitstreamConverter();

  pts_stream_.Initialize(GetFrameDuration(config));
  natural_size_ = config.natural_size();

  callback.Run(PIPELINE_OK);
}

void GpuVideoDecoder::Read(const ReadCB& callback) {
  if (MessageLoop::current() != message_loop_) {
    message_loop_->PostTask(FROM_HERE, base::Bind(
        &GpuVideoDecoder::Read, this, callback));
    return;
  }

  if (!vda_) {
    callback.Run(VideoFrame::CreateEmptyFrame());
    return;
  }

  DCHECK(pending_read_cb_.is_null());
  pending_read_cb_ = callback;

  if (!ready_video_frames_.empty()) {
    DeliverFrame(ready_video_frames_.front());
    ready_video_frames_.pop_front();
    return;
  }
  EnsureDemuxOrDecode();
}

void GpuVideoDecoder::RequestBufferDecode(const scoped_refptr<Buffer>& buffer) {
  if (MessageLoop::current() != message_loop_) {
    message_loop_->PostTask(FROM_HERE, base::Bind(
        &GpuVideoDecoder::RequestBufferDecode, this, buffer));
    return;
  }
  demuxer_read_in_progress_ = false;

  if (!vda_) {
    DeliverFrame(VideoFrame::CreateEmptyFrame());
    return;
  }

  if (buffer->IsEndOfStream()) {
    if (!flush_in_progress_) {
      flush_in_progress_ = true;
      vda_->Flush();
    }
    return;
  }

  size_t size = buffer->GetDataSize();
  SHMBuffer* shm_buffer = GetSHM(size);
  memcpy(shm_buffer->shm->memory(), buffer->GetData(), size);
  BitstreamBuffer bitstream_buffer(
      next_bitstream_buffer_id_++, shm_buffer->shm->handle(), size);
  bool inserted = bitstream_buffers_in_decoder_.insert(std::make_pair(
      bitstream_buffer.id(), BufferPair(shm_buffer, buffer))).second;
  DCHECK(inserted);
  pts_stream_.EnqueuePts(buffer.get());

  vda_->Decode(bitstream_buffer);
}

const gfx::Size& GpuVideoDecoder::natural_size() {
  return natural_size_;
}

bool GpuVideoDecoder::HasAlpha() const {
  return true;
}

void GpuVideoDecoder::NotifyInitializeDone() {
  NOTREACHED() << "GpuVideoDecodeAcceleratorHost::Initialize is synchronous!";
}

void GpuVideoDecoder::ProvidePictureBuffers(uint32 count,
                                            const gfx::Size& size) {
  if (MessageLoop::current() != message_loop_) {
    message_loop_->PostTask(FROM_HERE, base::Bind(
        &GpuVideoDecoder::ProvidePictureBuffers, this, count, size));
    return;
  }

  std::vector<uint32> texture_ids;
  if (!factories_->CreateTextures(count, size, &texture_ids)) {
    NotifyError(VideoDecodeAccelerator::PLATFORM_FAILURE);
    return;
  }

  if (!vda_)
    return;

  std::vector<PictureBuffer> picture_buffers;
  for (size_t i = 0; i < texture_ids.size(); ++i) {
    picture_buffers.push_back(PictureBuffer(
        next_picture_buffer_id_++, size, texture_ids[i]));
    bool inserted = picture_buffers_in_decoder_.insert(std::make_pair(
        picture_buffers.back().id(), picture_buffers.back())).second;
    DCHECK(inserted);
  }
  vda_->AssignPictureBuffers(picture_buffers);
}

void GpuVideoDecoder::DismissPictureBuffer(int32 id) {
  if (MessageLoop::current() != message_loop_) {
    message_loop_->PostTask(FROM_HERE, base::Bind(
        &GpuVideoDecoder::DismissPictureBuffer, this, id));
    return;
  }
  std::map<int32, PictureBuffer>::iterator it =
      picture_buffers_in_decoder_.find(id);
  if (it == picture_buffers_in_decoder_.end()) {
    NOTREACHED() << "Missing picture buffer: " << id;
    return;
  }
  if (!factories_->DeleteTexture(it->second.texture_id())) {
    NotifyError(VideoDecodeAccelerator::PLATFORM_FAILURE);
    return;
  }
  picture_buffers_in_decoder_.erase(it);
}

static void ResetAndRunCB(VideoDecoder::ReadCB* cb,
                          scoped_refptr<VideoFrame> frame) {
  DCHECK(!cb->is_null());
  VideoDecoder::ReadCB tmp_cb(*cb);
  cb->Reset();
  tmp_cb.Run(frame);
}

void GpuVideoDecoder::PictureReady(const media::Picture& picture) {
  if (MessageLoop::current() != message_loop_) {
    message_loop_->PostTask(FROM_HERE, base::Bind(
        &GpuVideoDecoder::PictureReady, this, picture));
    return;
  }
  std::map<int32, PictureBuffer>::iterator it =
      picture_buffers_in_decoder_.find(picture.picture_buffer_id());
  if (it == picture_buffers_in_decoder_.end()) {
    NOTREACHED() << "Missing picture buffer: " << picture.picture_buffer_id();
    NotifyError(VideoDecodeAccelerator::PLATFORM_FAILURE);
    return;
  }
  const PictureBuffer& pb = it->second;

  // Update frame's timestamp.
  base::TimeDelta timestamp;
  base::TimeDelta duration;
  std::map<int32, BufferPair>::const_iterator buf_it =
      bitstream_buffers_in_decoder_.find(picture.bitstream_buffer_id());
  if (buf_it != bitstream_buffers_in_decoder_.end()) {
    // Sufficiently out-of-order decoding could have already called
    // NotifyEndOfBitstreamBuffer on this buffer, but that's ok since we only
    // need the buffer's time info for best-effort PTS updating.
    timestamp = buf_it->second.buffer->GetTimestamp();
    duration = buf_it->second.buffer->GetDuration();
  }

  scoped_refptr<VideoFrame> frame(VideoFrame::WrapNativeTexture(
      pb.texture_id(), pb.size().width(),
      pb.size().height(), timestamp, duration,
      base::Bind(&GpuVideoDecoder::ReusePictureBuffer, this,
                 picture.picture_buffer_id())));
  pts_stream_.UpdatePtsAndDuration(frame.get());
  frame->SetTimestamp(pts_stream_.current_pts());
  frame->SetDuration(pts_stream_.current_duration());

  // Deliver the frame.
  DeliverFrame(frame);
}

void GpuVideoDecoder::DeliverFrame(const scoped_refptr<VideoFrame>& frame) {
    message_loop_->PostTask(FROM_HERE, base::Bind(
        &GpuVideoDecoder::DeliverFrameOutOfLine, this, frame));
}

void GpuVideoDecoder::DeliverFrameOutOfLine(
    const scoped_refptr<VideoFrame>& frame) {
  if (pending_read_cb_.is_null()) {
    ready_video_frames_.push_back(frame);
    return;
  }
  ResetAndRunCB(&pending_read_cb_, frame);
}

void GpuVideoDecoder::ReusePictureBuffer(int64 picture_buffer_id) {
  if (MessageLoop::current() != message_loop_) {
    message_loop_->PostTask(FROM_HERE, base::Bind(
        &GpuVideoDecoder::ReusePictureBuffer, this, picture_buffer_id));
    return;
  }
  if (!vda_)
    return;
  vda_->ReusePictureBuffer(picture_buffer_id);
}

GpuVideoDecoder::SHMBuffer* GpuVideoDecoder::GetSHM(size_t min_size) {
  DCHECK(MessageLoop::current() == message_loop_);
  if (available_shm_segments_.empty() ||
      available_shm_segments_.back()->size < min_size) {
    size_t size_to_allocate = std::max(min_size, kSharedMemorySegmentBytes);
    base::SharedMemory* shm = factories_->CreateSharedMemory(size_to_allocate);
    DCHECK(shm);
    return new SHMBuffer(shm, size_to_allocate);
  }
  SHMBuffer* ret = available_shm_segments_.back();
  available_shm_segments_.pop_back();
  return ret;
}

void GpuVideoDecoder::PutSHM(SHMBuffer* shm_buffer) {
  DCHECK(MessageLoop::current() == message_loop_);
  available_shm_segments_.push_back(shm_buffer);
}

void GpuVideoDecoder::NotifyEndOfStream() {
  if (MessageLoop::current() != message_loop_) {
    message_loop_->PostTask(FROM_HERE, base::Bind(
        &GpuVideoDecoder::NotifyEndOfStream, this));
    return;
  }
  DeliverFrame(VideoFrame::CreateEmptyFrame());
}

void GpuVideoDecoder::NotifyEndOfBitstreamBuffer(int32 id) {
  if (MessageLoop::current() != message_loop_) {
    message_loop_->PostTask(FROM_HERE, base::Bind(
        &GpuVideoDecoder::NotifyEndOfBitstreamBuffer, this, id));
    return;
  }

  std::map<int32, BufferPair>::iterator it =
      bitstream_buffers_in_decoder_.find(id);
  if (it == bitstream_buffers_in_decoder_.end()) {
    NotifyError(VideoDecodeAccelerator::PLATFORM_FAILURE);
    NOTREACHED() << "Missing bitstream buffer: " << id;
    return;
  }
  PutSHM(it->second.shm_buffer);
  const scoped_refptr<Buffer>& buffer = it->second.buffer;
  if (buffer->GetDataSize()) {
    PipelineStatistics statistics;
    statistics.video_bytes_decoded = buffer->GetDataSize();
    statistics_callback_.Run(statistics);
  }
  bitstream_buffers_in_decoder_.erase(it);

  if (!pending_read_cb_.is_null()) {
    DCHECK(ready_video_frames_.empty());
    EnsureDemuxOrDecode();
  }
}

void GpuVideoDecoder::EnsureDemuxOrDecode() {
  DCHECK(MessageLoop::current() == message_loop_);
  if (demuxer_read_in_progress_ || !bitstream_buffers_in_decoder_.empty())
    return;
  demuxer_read_in_progress_ = true;
  demuxer_stream_->Read(base::Bind(
      &GpuVideoDecoder::RequestBufferDecode, this));
}

void GpuVideoDecoder::NotifyFlushDone() {
  if (MessageLoop::current() != message_loop_) {
    message_loop_->PostTask(FROM_HERE, base::Bind(
        &GpuVideoDecoder::NotifyFlushDone, this));
    return;
  }
  DCHECK(flush_in_progress_);
  flush_in_progress_ = false;
  DeliverFrame(VideoFrame::CreateEmptyFrame());
}

void GpuVideoDecoder::NotifyResetDone() {
  if (MessageLoop::current() != message_loop_) {
    message_loop_->PostTask(FROM_HERE, base::Bind(
        &GpuVideoDecoder::NotifyResetDone, this));
    return;
  }
  // Throw away any already-decoded frames that have come in during the reset.
  ready_video_frames_.clear();
  ResetAndRunCB(&pending_flush_cb_);
}

void GpuVideoDecoder::NotifyError(media::VideoDecodeAccelerator::Error error) {
  if (MessageLoop::current() != message_loop_) {
    message_loop_->PostTask(FROM_HERE, base::Bind(
        &GpuVideoDecoder::NotifyError, this, error));
    return;
  }
  vda_ = NULL;
  DLOG(ERROR) << "VDA Error: " << error;
  if (host())
    host()->SetError(PIPELINE_ERROR_DECODE);
}

}  // namespace media
