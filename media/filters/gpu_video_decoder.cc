// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/gpu_video_decoder.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/message_loop.h"
#include "base/stl_util.h"
#include "media/base/decoder_buffer.h"
#include "media/base/demuxer_stream.h"
#include "media/base/filter_host.h"
#include "media/base/pipeline.h"
#include "media/base/pipeline_status.h"
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
    SHMBuffer* s, const scoped_refptr<DecoderBuffer>& b)
    : shm_buffer(s), buffer(b) {
}

GpuVideoDecoder::BufferPair::~BufferPair() {}

GpuVideoDecoder::BufferTimeData::BufferTimeData(
    int32 bbid, base::TimeDelta ts, base::TimeDelta dur)
    : bitstream_buffer_id(bbid), timestamp(ts), duration(dur) {
}

GpuVideoDecoder::BufferTimeData::~BufferTimeData() {}

GpuVideoDecoder::GpuVideoDecoder(
    MessageLoop* message_loop,
    MessageLoop* vda_loop,
    const scoped_refptr<Factories>& factories)
    : gvd_loop_proxy_(message_loop->message_loop_proxy()),
      vda_loop_proxy_(vda_loop->message_loop_proxy()),
      factories_(factories),
      state_(kNormal),
      demuxer_read_in_progress_(false),
      decoder_texture_target_(0),
      next_picture_buffer_id_(0),
      next_bitstream_buffer_id_(0),
      shutting_down_(false),
      error_occured_(false) {
  DCHECK(gvd_loop_proxy_ && factories_);
}

void GpuVideoDecoder::Reset(const base::Closure& closure)  {
  if (!gvd_loop_proxy_->BelongsToCurrentThread() ||
      state_ == kDrainingDecoder) {
    gvd_loop_proxy_->PostTask(FROM_HERE, base::Bind(
        &GpuVideoDecoder::Reset, this, closure));
    return;
  }

  // Throw away any already-decoded, not-yet-delivered frames.
  ready_video_frames_.clear();

  if (!vda_) {
    closure.Run();
    return;
  }

  DCHECK(pending_reset_cb_.is_null());
  DCHECK(!closure.is_null());

  // VideoRendererBase::Flush() can't complete while it has a pending read to
  // us, so we fulfill such a read here.
  if (!pending_read_cb_.is_null())
    EnqueueFrameAndTriggerFrameDelivery(VideoFrame::CreateEmptyFrame());

  if (shutting_down_) {
    // Immediately fire the callback instead of waiting for the reset to
    // complete (which will happen after PipelineImpl::Stop() completes).
    gvd_loop_proxy_->PostTask(FROM_HERE, closure);
  } else {
    pending_reset_cb_ = closure;
  }

  vda_loop_proxy_->PostTask(FROM_HERE, base::Bind(
      &VideoDecodeAccelerator::Reset, vda_));
}

void GpuVideoDecoder::Stop(const base::Closure& closure) {
  if (!gvd_loop_proxy_->BelongsToCurrentThread()) {
    gvd_loop_proxy_->PostTask(FROM_HERE, base::Bind(
        &GpuVideoDecoder::Stop, this, closure));
    return;
  }
  if (!vda_) {
    closure.Run();
    return;
  }
  vda_loop_proxy_->PostTask(FROM_HERE, base::Bind(
      &VideoDecodeAccelerator::Destroy, vda_));
  vda_ = NULL;
  closure.Run();
}

void GpuVideoDecoder::Initialize(const scoped_refptr<DemuxerStream>& stream,
                                 const PipelineStatusCB& orig_status_cb,
                                 const StatisticsCB& statistics_cb) {
  if (!gvd_loop_proxy_->BelongsToCurrentThread()) {
    gvd_loop_proxy_->PostTask(FROM_HERE, base::Bind(
        &GpuVideoDecoder::Initialize,
        this, stream, orig_status_cb, statistics_cb));
    return;
  }

  PipelineStatusCB status_cb = CreateUMAReportingPipelineCB(
      "Media.GpuVideoDecoderInitializeStatus", orig_status_cb);

  DCHECK(!demuxer_stream_);
  if (!stream) {
    status_cb.Run(PIPELINE_ERROR_DECODE);
    return;
  }

  const VideoDecoderConfig& config = stream->video_decoder_config();
  // TODO(scherkus): this check should go in Pipeline prior to creating
  // decoder objects.
  if (!config.IsValidConfig()) {
    DLOG(ERROR) << "Invalid video stream - " << config.AsHumanReadableString();
    status_cb.Run(PIPELINE_ERROR_DECODE);
    return;
  }

  vda_ = factories_->CreateVideoDecodeAccelerator(config.profile(), this);
  if (!vda_) {
    status_cb.Run(DECODER_ERROR_NOT_SUPPORTED);
    return;
  }

  demuxer_stream_ = stream;
  statistics_cb_ = statistics_cb;

  demuxer_stream_->EnableBitstreamConverter();

  natural_size_ = config.natural_size();
  config_frame_duration_ = GetFrameDuration(config);

  DVLOG(1) << "GpuVideoDecoder::Initialize() succeeded.";
  status_cb.Run(PIPELINE_OK);
}

void GpuVideoDecoder::Read(const ReadCB& read_cb) {
  if (!gvd_loop_proxy_->BelongsToCurrentThread()) {
    gvd_loop_proxy_->PostTask(FROM_HERE, base::Bind(
        &GpuVideoDecoder::Read, this, read_cb));
    return;
  }

  if (error_occured_) {
    read_cb.Run(kDecodeError, NULL);
    return;
  }

  if (!vda_) {
    read_cb.Run(kOk, VideoFrame::CreateEmptyFrame());
    return;
  }

  DCHECK(pending_reset_cb_.is_null());
  DCHECK(pending_read_cb_.is_null());
  pending_read_cb_ = read_cb;

  if (!ready_video_frames_.empty()) {
    EnqueueFrameAndTriggerFrameDelivery(NULL);
    return;
  }

  switch (state_) {
    case kDecoderDrained:
      state_ = kNormal;
      // Fall-through.
    case kNormal:
      EnsureDemuxOrDecode();
      break;
    case kDrainingDecoder:
      // Do nothing.  Will be satisfied either by a PictureReady or
      // NotifyFlushDone below.
      break;
  }
}

void GpuVideoDecoder::RequestBufferDecode(
    const scoped_refptr<DecoderBuffer>& buffer) {
  if (!gvd_loop_proxy_->BelongsToCurrentThread()) {
    gvd_loop_proxy_->PostTask(FROM_HERE, base::Bind(
        &GpuVideoDecoder::RequestBufferDecode, this, buffer));
    return;
  }
  demuxer_read_in_progress_ = false;

  if (!buffer) {
    if (pending_read_cb_.is_null())
      return;

    gvd_loop_proxy_->PostTask(FROM_HERE, base::Bind(
        pending_read_cb_, kOk, scoped_refptr<VideoFrame>()));
    pending_read_cb_.Reset();
    return;
  }

  if (!vda_) {
    EnqueueFrameAndTriggerFrameDelivery(VideoFrame::CreateEmptyFrame());
    return;
  }

  if (buffer->IsEndOfStream()) {
    if (state_ == kNormal) {
      state_ = kDrainingDecoder;
      vda_loop_proxy_->PostTask(FROM_HERE, base::Bind(
          &VideoDecodeAccelerator::Flush, vda_));
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
  RecordBufferTimeData(bitstream_buffer, *buffer);

  vda_loop_proxy_->PostTask(FROM_HERE, base::Bind(
      &VideoDecodeAccelerator::Decode, vda_, bitstream_buffer));
}

void GpuVideoDecoder::RecordBufferTimeData(
    const BitstreamBuffer& bitstream_buffer, const Buffer& buffer) {
  base::TimeDelta duration = buffer.GetDuration();
  if (duration == base::TimeDelta())
    duration = config_frame_duration_;
  input_buffer_time_data_.push_front(BufferTimeData(
      bitstream_buffer.id(), buffer.GetTimestamp(), duration));
  // Why this value?  Because why not.  avformat.h:MAX_REORDER_DELAY is 16, but
  // that's too small for some pathological B-frame test videos.  The cost of
  // using too-high a value is low (192 bits per extra slot).
  static const size_t kMaxInputBufferTimeDataSize = 128;
  // Pop from the back of the list, because that's the oldest and least likely
  // to be useful in the future data.
  if (input_buffer_time_data_.size() > kMaxInputBufferTimeDataSize)
    input_buffer_time_data_.pop_back();
}

void GpuVideoDecoder::GetBufferTimeData(
    int32 id, base::TimeDelta* timestamp, base::TimeDelta* duration) {
  // If all else fails later, at least we can set a default duration if there
  // was one in the config.
  *duration = config_frame_duration_;
  for (std::list<BufferTimeData>::const_iterator it =
           input_buffer_time_data_.begin(); it != input_buffer_time_data_.end();
       ++it) {
    if (it->bitstream_buffer_id != id)
      continue;
    *timestamp = it->timestamp;
    *duration = it->duration;
    return;
  }
  NOTREACHED() << "Missing bitstreambuffer id: " << id;
}

const gfx::Size& GpuVideoDecoder::natural_size() {
  return natural_size_;
}

bool GpuVideoDecoder::HasAlpha() const {
  return true;
}

void GpuVideoDecoder::PrepareForShutdownHack() {
  if (!gvd_loop_proxy_->BelongsToCurrentThread()) {
    gvd_loop_proxy_->PostTask(FROM_HERE, base::Bind(
        &GpuVideoDecoder::PrepareForShutdownHack, this));
    return;
  }
  shutting_down_ = true;
}

void GpuVideoDecoder::NotifyInitializeDone() {
  NOTREACHED() << "GpuVideoDecodeAcceleratorHost::Initialize is synchronous!";
}

void GpuVideoDecoder::ProvidePictureBuffers(uint32 count,
                                            const gfx::Size& size,
                                            uint32 texture_target) {
  if (!gvd_loop_proxy_->BelongsToCurrentThread()) {
    gvd_loop_proxy_->PostTask(FROM_HERE, base::Bind(
        &GpuVideoDecoder::ProvidePictureBuffers, this, count, size,
        texture_target));
    return;
  }

  std::vector<uint32> texture_ids;
  decoder_texture_target_ = texture_target;
  if (!factories_->CreateTextures(
      count, size, &texture_ids, decoder_texture_target_)) {
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
  vda_loop_proxy_->PostTask(FROM_HERE, base::Bind(
      &VideoDecodeAccelerator::AssignPictureBuffers, vda_, picture_buffers));
}

void GpuVideoDecoder::DismissPictureBuffer(int32 id) {
  if (!gvd_loop_proxy_->BelongsToCurrentThread()) {
    gvd_loop_proxy_->PostTask(FROM_HERE, base::Bind(
        &GpuVideoDecoder::DismissPictureBuffer, this, id));
    return;
  }
  std::map<int32, PictureBuffer>::iterator it =
      picture_buffers_in_decoder_.find(id);
  if (it == picture_buffers_in_decoder_.end()) {
    NOTREACHED() << "Missing picture buffer: " << id;
    return;
  }
  factories_->DeleteTexture(it->second.texture_id());
  picture_buffers_in_decoder_.erase(it);
}

void GpuVideoDecoder::PictureReady(const media::Picture& picture) {
  if (!gvd_loop_proxy_->BelongsToCurrentThread()) {
    gvd_loop_proxy_->PostTask(FROM_HERE, base::Bind(
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
  GetBufferTimeData(picture.bitstream_buffer_id(), &timestamp, &duration);

  DCHECK(decoder_texture_target_);
  scoped_refptr<VideoFrame> frame(VideoFrame::WrapNativeTexture(
      pb.texture_id(), decoder_texture_target_, pb.size().width(),
      pb.size().height(), timestamp, duration,
      base::Bind(&GpuVideoDecoder::ReusePictureBuffer, this,
                 picture.picture_buffer_id())));

  EnqueueFrameAndTriggerFrameDelivery(frame);
}

void GpuVideoDecoder::EnqueueFrameAndTriggerFrameDelivery(
    const scoped_refptr<VideoFrame>& frame) {
  DCHECK(gvd_loop_proxy_->BelongsToCurrentThread());

  // During a pending vda->Reset(), we don't accumulate frames.  Drop it on the
  // floor and return.
  if (!pending_reset_cb_.is_null())
    return;

  if (frame)
    ready_video_frames_.push_back(frame);
  else
    DCHECK(!ready_video_frames_.empty());

  if (pending_read_cb_.is_null())
    return;

  gvd_loop_proxy_->PostTask(FROM_HERE, base::Bind(
      pending_read_cb_, kOk, ready_video_frames_.front()));
  pending_read_cb_.Reset();
  ready_video_frames_.pop_front();
}

void GpuVideoDecoder::ReusePictureBuffer(int64 picture_buffer_id) {
  if (!gvd_loop_proxy_->BelongsToCurrentThread()) {
    gvd_loop_proxy_->PostTask(FROM_HERE, base::Bind(
        &GpuVideoDecoder::ReusePictureBuffer, this, picture_buffer_id));
    return;
  }
  if (!vda_)
    return;
  vda_loop_proxy_->PostTask(FROM_HERE, base::Bind(
      &VideoDecodeAccelerator::ReusePictureBuffer, vda_, picture_buffer_id));
}

GpuVideoDecoder::SHMBuffer* GpuVideoDecoder::GetSHM(size_t min_size) {
  DCHECK(gvd_loop_proxy_->BelongsToCurrentThread());
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
  DCHECK(gvd_loop_proxy_->BelongsToCurrentThread());
  available_shm_segments_.push_back(shm_buffer);
}

void GpuVideoDecoder::NotifyEndOfBitstreamBuffer(int32 id) {
  if (!gvd_loop_proxy_->BelongsToCurrentThread()) {
    gvd_loop_proxy_->PostTask(FROM_HERE, base::Bind(
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
  const scoped_refptr<DecoderBuffer>& buffer = it->second.buffer;
  if (buffer->GetDataSize()) {
    PipelineStatistics statistics;
    statistics.video_bytes_decoded = buffer->GetDataSize();
    statistics_cb_.Run(statistics);
  }
  bitstream_buffers_in_decoder_.erase(it);

  if (!pending_read_cb_.is_null() && pending_reset_cb_.is_null() &&
      state_ != kDrainingDecoder &&
      bitstream_buffers_in_decoder_.empty()) {
    DCHECK(ready_video_frames_.empty());
    EnsureDemuxOrDecode();
  }
}

GpuVideoDecoder::~GpuVideoDecoder() {
  DCHECK(!vda_);  // Stop should have been already called.
  DCHECK(pending_read_cb_.is_null());
  for (size_t i = 0; i < available_shm_segments_.size(); ++i) {
    available_shm_segments_[i]->shm->Close();
    delete available_shm_segments_[i];
  }
  available_shm_segments_.clear();
  for (std::map<int32, BufferPair>::iterator it =
           bitstream_buffers_in_decoder_.begin();
       it != bitstream_buffers_in_decoder_.end(); ++it) {
    it->second.shm_buffer->shm->Close();
  }
  bitstream_buffers_in_decoder_.clear();
}

void GpuVideoDecoder::EnsureDemuxOrDecode() {
  DCHECK(gvd_loop_proxy_->BelongsToCurrentThread());
  if (demuxer_read_in_progress_)
    return;
  demuxer_read_in_progress_ = true;
  gvd_loop_proxy_->PostTask(FROM_HERE, base::Bind(
      &DemuxerStream::Read, demuxer_stream_.get(),
      base::Bind(&GpuVideoDecoder::RequestBufferDecode, this)));
}

void GpuVideoDecoder::NotifyFlushDone() {
  if (!gvd_loop_proxy_->BelongsToCurrentThread()) {
    gvd_loop_proxy_->PostTask(FROM_HERE, base::Bind(
        &GpuVideoDecoder::NotifyFlushDone, this));
    return;
  }
  DCHECK_EQ(state_, kDrainingDecoder);
  state_ = kDecoderDrained;
  EnqueueFrameAndTriggerFrameDelivery(VideoFrame::CreateEmptyFrame());
}

void GpuVideoDecoder::NotifyResetDone() {
  if (!gvd_loop_proxy_->BelongsToCurrentThread()) {
    gvd_loop_proxy_->PostTask(FROM_HERE, base::Bind(
        &GpuVideoDecoder::NotifyResetDone, this));
    return;
  }

  if (!vda_)
    return;

  DCHECK(ready_video_frames_.empty());

  // This needs to happen after the Reset() on vda_ is done to ensure pictures
  // delivered during the reset can find their time data.
  input_buffer_time_data_.clear();

  if (!pending_reset_cb_.is_null())
    base::ResetAndReturn(&pending_reset_cb_).Run();

  if (!pending_read_cb_.is_null())
    EnqueueFrameAndTriggerFrameDelivery(VideoFrame::CreateEmptyFrame());
}

void GpuVideoDecoder::NotifyError(media::VideoDecodeAccelerator::Error error) {
  if (!gvd_loop_proxy_->BelongsToCurrentThread()) {
    gvd_loop_proxy_->PostTask(FROM_HERE, base::Bind(
        &GpuVideoDecoder::NotifyError, this, error));
    return;
  }
  if (!vda_)
    return;
  vda_ = NULL;
  DLOG(ERROR) << "VDA Error: " << error;

  error_occured_ = true;

  if (!pending_read_cb_.is_null()) {
    base::ResetAndReturn(&pending_read_cb_).Run(kDecodeError, NULL);
    return;
  }
}

}  // namespace media
