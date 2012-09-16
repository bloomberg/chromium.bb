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
#include "media/base/pipeline.h"
#include "media/base/pipeline_status.h"
#include "media/base/video_decoder_config.h"
#include "media/ffmpeg/ffmpeg_common.h"

namespace media {

// Maximum number of concurrent VDA::Decode() operations GVD will maintain.
// Higher values allow better pipelining in the GPU, but also require more
// resources.
enum { kMaxInFlightDecodes = 4 };

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

GpuVideoDecoder::BufferData::BufferData(
    int32 bbid, base::TimeDelta ts, const gfx::Size& ns)
    : bitstream_buffer_id(bbid), timestamp(ts), natural_size(ns) {
}

GpuVideoDecoder::BufferData::~BufferData() {}

GpuVideoDecoder::GpuVideoDecoder(
    const MessageLoopFactoryCB& message_loop_factory_cb,
    const scoped_refptr<base::MessageLoopProxy>& vda_loop_proxy,
    const scoped_refptr<Factories>& factories)
    : message_loop_factory_cb_(message_loop_factory_cb),
      gvd_loop_proxy_(NULL),
      vda_loop_proxy_(vda_loop_proxy),
      factories_(factories),
      state_(kNormal),
      demuxer_read_in_progress_(false),
      decoder_texture_target_(0),
      next_picture_buffer_id_(0),
      next_bitstream_buffer_id_(0),
      error_occured_(false) {
  DCHECK(!message_loop_factory_cb_.is_null());
  DCHECK(factories_);
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

  if (!vda_.get()) {
    closure.Run();
    return;
  }

  DCHECK(pending_reset_cb_.is_null());
  DCHECK(!closure.is_null());

  // VideoRendererBase::Flush() can't complete while it has a pending read to
  // us, so we fulfill such a read here.
  if (!pending_read_cb_.is_null())
    EnqueueFrameAndTriggerFrameDelivery(VideoFrame::CreateEmptyFrame());

  pending_reset_cb_ = closure;
  vda_loop_proxy_->PostTask(FROM_HERE, base::Bind(
      &VideoDecodeAccelerator::Reset, weak_vda_));
}

void GpuVideoDecoder::Stop(const base::Closure& closure) {
  if (!gvd_loop_proxy_->BelongsToCurrentThread()) {
    gvd_loop_proxy_->PostTask(FROM_HERE, base::Bind(
        &GpuVideoDecoder::Stop, this, closure));
    return;
  }
  if (!vda_.get()) {
    closure.Run();
    return;
  }
  DestroyVDA();
  closure.Run();
}

void GpuVideoDecoder::Initialize(const scoped_refptr<DemuxerStream>& stream,
                                 const PipelineStatusCB& orig_status_cb,
                                 const StatisticsCB& statistics_cb) {
  if (!gvd_loop_proxy_) {
    gvd_loop_proxy_ = base::ResetAndReturn(&message_loop_factory_cb_).Run();
    gvd_loop_proxy_->PostTask(FROM_HERE, base::Bind(
        &GpuVideoDecoder::Initialize,
        this, stream, orig_status_cb, statistics_cb));
    return;
  }

  DCHECK(gvd_loop_proxy_->BelongsToCurrentThread());
  PipelineStatusCB status_cb = CreateUMAReportingPipelineCB(
      "Media.GpuVideoDecoderInitializeStatus", orig_status_cb);

  DCHECK(!demuxer_stream_);
  if (!stream) {
    status_cb.Run(PIPELINE_ERROR_DECODE);
    return;
  }

  // TODO(scherkus): this check should go in Pipeline prior to creating
  // decoder objects.
  const VideoDecoderConfig& config = stream->video_decoder_config();
  if (!config.IsValidConfig()) {
    DLOG(ERROR) << "Invalid video stream - " << config.AsHumanReadableString();
    status_cb.Run(PIPELINE_ERROR_DECODE);
    return;
  }

  VideoDecodeAccelerator* vda =
      factories_->CreateVideoDecodeAccelerator(config.profile(), this);
  if (!vda) {
    status_cb.Run(DECODER_ERROR_NOT_SUPPORTED);
    return;
  }

  if (config.codec() == kCodecH264)
    stream->EnableBitstreamConverter();

  demuxer_stream_ = stream;
  statistics_cb_ = statistics_cb;

  DVLOG(1) << "GpuVideoDecoder::Initialize() succeeded.";
  vda_loop_proxy_->PostTaskAndReply(
      FROM_HERE,
      base::Bind(&GpuVideoDecoder::SetVDA, this, vda),
      base::Bind(status_cb, PIPELINE_OK));
}

void GpuVideoDecoder::SetVDA(VideoDecodeAccelerator* vda) {
  DCHECK(vda_loop_proxy_->BelongsToCurrentThread());
  DCHECK(!vda_.get());
  vda_.reset(vda);
  weak_vda_ = vda->AsWeakPtr();
}

void GpuVideoDecoder::DestroyVDA() {
  DCHECK(gvd_loop_proxy_->BelongsToCurrentThread());
  VideoDecodeAccelerator* vda ALLOW_UNUSED = vda_.release();
  // Tricky: |this| needs to stay alive until after VDA::Destroy is actually
  // called, not just posted, so we take an artificial ref to |this| and release
  // it as |reply| after VDA::Destroy() returns.
  AddRef();
  vda_loop_proxy_->PostTaskAndReply(
      FROM_HERE,
      base::Bind(&VideoDecodeAccelerator::Destroy, weak_vda_),
      base::Bind(&GpuVideoDecoder::Release, this));
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

  if (!vda_.get()) {
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
    DemuxerStream::Status status,
    const scoped_refptr<DecoderBuffer>& buffer) {
  DCHECK_EQ(status != DemuxerStream::kOk, !buffer) << status;

  if (!gvd_loop_proxy_->BelongsToCurrentThread()) {
    gvd_loop_proxy_->PostTask(FROM_HERE, base::Bind(
        &GpuVideoDecoder::RequestBufferDecode, this, status, buffer));
    return;
  }
  demuxer_read_in_progress_ = false;

  if (status != DemuxerStream::kOk) {
    if (pending_read_cb_.is_null())
      return;

    // TODO(acolwell): Add support for reinitializing the decoder when
    // |status| == kConfigChanged. For now we just trigger a decode error.
    Status decoder_status =
        (status == DemuxerStream::kAborted) ? kOk : kDecodeError;
    gvd_loop_proxy_->PostTask(FROM_HERE, base::Bind(
        pending_read_cb_, decoder_status, scoped_refptr<VideoFrame>()));
    pending_read_cb_.Reset();
    return;
  }

  if (!vda_.get()) {
    EnqueueFrameAndTriggerFrameDelivery(VideoFrame::CreateEmptyFrame());
    return;
  }

  if (buffer->IsEndOfStream()) {
    if (state_ == kNormal) {
      state_ = kDrainingDecoder;
      vda_loop_proxy_->PostTask(FROM_HERE, base::Bind(
          &VideoDecodeAccelerator::Flush, weak_vda_));
    }
    return;
  }

  if (!pending_reset_cb_.is_null())
    return;

  size_t size = buffer->GetDataSize();
  SHMBuffer* shm_buffer = GetSHM(size);
  memcpy(shm_buffer->shm->memory(), buffer->GetData(), size);
  BitstreamBuffer bitstream_buffer(
      next_bitstream_buffer_id_++, shm_buffer->shm->handle(), size);
  bool inserted = bitstream_buffers_in_decoder_.insert(std::make_pair(
      bitstream_buffer.id(), BufferPair(shm_buffer, buffer))).second;
  DCHECK(inserted);
  RecordBufferData(bitstream_buffer, *buffer);

  vda_loop_proxy_->PostTask(FROM_HERE, base::Bind(
      &VideoDecodeAccelerator::Decode, weak_vda_, bitstream_buffer));

  if (bitstream_buffers_in_decoder_.size() < kMaxInFlightDecodes)
    EnsureDemuxOrDecode();
}

void GpuVideoDecoder::RecordBufferData(
    const BitstreamBuffer& bitstream_buffer, const Buffer& buffer) {
  input_buffer_data_.push_front(BufferData(
      bitstream_buffer.id(), buffer.GetTimestamp(),
      demuxer_stream_->video_decoder_config().natural_size()));
  // Why this value?  Because why not.  avformat.h:MAX_REORDER_DELAY is 16, but
  // that's too small for some pathological B-frame test videos.  The cost of
  // using too-high a value is low (192 bits per extra slot).
  static const size_t kMaxInputBufferDataSize = 128;
  // Pop from the back of the list, because that's the oldest and least likely
  // to be useful in the future data.
  if (input_buffer_data_.size() > kMaxInputBufferDataSize)
    input_buffer_data_.pop_back();
}

void GpuVideoDecoder::GetBufferData(int32 id, base::TimeDelta* timestamp,
                                    gfx::Size* natural_size) {
  for (std::list<BufferData>::const_iterator it =
           input_buffer_data_.begin(); it != input_buffer_data_.end();
       ++it) {
    if (it->bitstream_buffer_id != id)
      continue;
    *timestamp = it->timestamp;
    *natural_size = it->natural_size;
    return;
  }
  NOTREACHED() << "Missing bitstreambuffer id: " << id;
}

bool GpuVideoDecoder::HasAlpha() const {
  return true;
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

  if (!vda_.get())
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
      &VideoDecodeAccelerator::AssignPictureBuffers, weak_vda_,
      picture_buffers));
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
  gfx::Size natural_size;
  GetBufferData(picture.bitstream_buffer_id(), &timestamp, &natural_size);
  DCHECK(decoder_texture_target_);
  scoped_refptr<VideoFrame> frame(VideoFrame::WrapNativeTexture(
      pb.texture_id(), decoder_texture_target_, pb.size(), natural_size,
      timestamp,
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
  if (!vda_.get())
    return;
  vda_loop_proxy_->PostTask(FROM_HERE, base::Bind(
      &VideoDecodeAccelerator::ReusePictureBuffer, weak_vda_,
      picture_buffer_id));
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

  if (pending_reset_cb_.is_null() && state_ != kDrainingDecoder &&
      bitstream_buffers_in_decoder_.size() < kMaxInFlightDecodes) {
    EnsureDemuxOrDecode();
  }
}

GpuVideoDecoder::~GpuVideoDecoder() {
  DCHECK(!vda_.get());  // Stop should have been already called.
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

  if (!vda_.get())
    return;

  DCHECK(ready_video_frames_.empty());

  // This needs to happen after the Reset() on vda_ is done to ensure pictures
  // delivered during the reset can find their time data.
  input_buffer_data_.clear();

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
  if (!vda_.get())
    return;

  DLOG(ERROR) << "VDA Error: " << error;
  DestroyVDA();

  error_occured_ = true;

  if (!pending_read_cb_.is_null()) {
    base::ResetAndReturn(&pending_read_cb_).Run(kDecodeError, NULL);
    return;
  }
}

}  // namespace media
