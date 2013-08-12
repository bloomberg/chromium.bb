// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/gpu_video_decoder.h"

#include <algorithm>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/cpu.h"
#include "base/message_loop/message_loop.h"
#include "base/stl_util.h"
#include "base/task_runner_util.h"
#include "media/base/bind_to_loop.h"
#include "media/base/decoder_buffer.h"
#include "media/base/pipeline.h"
#include "media/base/pipeline_status.h"
#include "media/base/video_decoder_config.h"
#include "media/filters/gpu_video_decoder_factories.h"

namespace media {

// Maximum number of concurrent VDA::Decode() operations GVD will maintain.
// Higher values allow better pipelining in the GPU, but also require more
// resources.
enum { kMaxInFlightDecodes = 4 };

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
    int32 bbid, base::TimeDelta ts, const gfx::Rect& vr, const gfx::Size& ns)
    : bitstream_buffer_id(bbid), timestamp(ts), visible_rect(vr),
      natural_size(ns) {
}

GpuVideoDecoder::BufferData::~BufferData() {}

GpuVideoDecoder::GpuVideoDecoder(
    const scoped_refptr<GpuVideoDecoderFactories>& factories)
    : needs_bitstream_conversion_(false),
      gvd_loop_proxy_(factories->GetMessageLoop()),
      weak_factory_(this),
      factories_(factories),
      state_(kNormal),
      decoder_texture_target_(0),
      next_picture_buffer_id_(0),
      next_bitstream_buffer_id_(0),
      available_pictures_(0) {
  DCHECK(factories_.get());
}

void GpuVideoDecoder::Reset(const base::Closure& closure)  {
  DVLOG(3) << "Reset()";
  DCHECK(gvd_loop_proxy_->BelongsToCurrentThread());

  if (state_ == kDrainingDecoder && !factories_->IsAborted()) {
    gvd_loop_proxy_->PostTask(FROM_HERE, base::Bind(
        &GpuVideoDecoder::Reset, weak_this_, closure));
    // NOTE: if we're deferring Reset() until a Flush() completes, return
    // queued pictures to the VDA so they can be used to finish that Flush().
    if (pending_decode_cb_.is_null())
      ready_video_frames_.clear();
    return;
  }

  // Throw away any already-decoded, not-yet-delivered frames.
  ready_video_frames_.clear();

  if (!vda_) {
    gvd_loop_proxy_->PostTask(FROM_HERE, closure);
    return;
  }

  if (!pending_decode_cb_.is_null())
    EnqueueFrameAndTriggerFrameDelivery(VideoFrame::CreateEmptyFrame());

  DCHECK(pending_reset_cb_.is_null());
  pending_reset_cb_ = BindToCurrentLoop(closure);

  vda_->Reset();
}

void GpuVideoDecoder::Stop(const base::Closure& closure) {
  DCHECK(gvd_loop_proxy_->BelongsToCurrentThread());
  if (vda_)
    DestroyVDA();
  if (!pending_decode_cb_.is_null())
    EnqueueFrameAndTriggerFrameDelivery(VideoFrame::CreateEmptyFrame());
  if (!pending_reset_cb_.is_null())
    base::ResetAndReturn(&pending_reset_cb_).Run();
  BindToCurrentLoop(closure).Run();
}

static bool IsCodedSizeSupported(const gfx::Size& coded_size) {
  // Only non-Windows, Ivy Bridge+ platforms can support more than 1920x1080.
  // We test against 1088 to account for 16x16 macroblocks.
  if (coded_size.width() <= 1920 && coded_size.height() <= 1088)
    return true;

  base::CPU cpu;
  bool hw_large_video_support =
      (cpu.vendor_name() == "GenuineIntel") && cpu.model() >= 58;
  bool os_large_video_support = true;
#if defined(OS_WIN)
  os_large_video_support = false;
#endif
  return os_large_video_support && hw_large_video_support;
}

void GpuVideoDecoder::Initialize(const VideoDecoderConfig& config,
                                 const PipelineStatusCB& orig_status_cb) {
  DVLOG(3) << "Initialize()";
  DCHECK(gvd_loop_proxy_->BelongsToCurrentThread());
  DCHECK(config.IsValidConfig());
  DCHECK(!config.is_encrypted());

  weak_this_ = weak_factory_.GetWeakPtr();

  PipelineStatusCB status_cb = CreateUMAReportingPipelineCB(
      "Media.GpuVideoDecoderInitializeStatus",
      BindToCurrentLoop(orig_status_cb));

  bool previously_initialized = config_.IsValidConfig();
#if !defined(OS_CHROMEOS)
  if (previously_initialized) {
    // TODO(xhwang): Make GpuVideoDecoder reinitializable.
    // See http://crbug.com/233608
    DVLOG(1) << "GpuVideoDecoder reinitialization not supported.";
    status_cb.Run(DECODER_ERROR_NOT_SUPPORTED);
    return;
  }
#endif
  DVLOG(1) << "(Re)initializing GVD with config: "
           << config.AsHumanReadableString();

  // TODO(posciak): destroy and create a new VDA on codec/profile change
  // (http://crbug.com/260224).
  if (previously_initialized && (config_.profile() != config.profile())) {
    DVLOG(1) << "Codec or profile changed, cannot reinitialize.";
    status_cb.Run(DECODER_ERROR_NOT_SUPPORTED);
    return;
  }

  if (!IsCodedSizeSupported(config.coded_size())) {
    status_cb.Run(DECODER_ERROR_NOT_SUPPORTED);
    return;
  }

  config_ = config;
  needs_bitstream_conversion_ = (config.codec() == kCodecH264);

  if (previously_initialized) {
    // Reinitialization with a different config (but same codec and profile).
    // VDA should handle it by detecting this in-stream by itself,
    // no need to notify it.
    status_cb.Run(PIPELINE_OK);
    return;
  }

  vda_.reset(factories_->CreateVideoDecodeAccelerator(config.profile(), this));
  if (!vda_) {
    status_cb.Run(DECODER_ERROR_NOT_SUPPORTED);
    return;
  }

  DVLOG(3) << "GpuVideoDecoder::Initialize() succeeded.";
  status_cb.Run(PIPELINE_OK);
}

void GpuVideoDecoder::DestroyTextures() {
  std::map<int32, PictureBuffer>::iterator it;

  for (it = assigned_picture_buffers_.begin();
       it != assigned_picture_buffers_.end(); ++it) {
    factories_->DeleteTexture(it->second.texture_id());
  }
  assigned_picture_buffers_.clear();

  for (it = dismissed_picture_buffers_.begin();
       it != dismissed_picture_buffers_.end(); ++it) {
    factories_->DeleteTexture(it->second.texture_id());
  }
  dismissed_picture_buffers_.clear();
}

void GpuVideoDecoder::DestroyVDA() {
  DCHECK(gvd_loop_proxy_->BelongsToCurrentThread());

  if (vda_)
    vda_.release()->Destroy();

  DestroyTextures();
}

void GpuVideoDecoder::Decode(const scoped_refptr<DecoderBuffer>& buffer,
                             const DecodeCB& decode_cb) {
  DCHECK(gvd_loop_proxy_->BelongsToCurrentThread());
  DCHECK(pending_reset_cb_.is_null());
  DCHECK(pending_decode_cb_.is_null());

  pending_decode_cb_ = BindToCurrentLoop(decode_cb);

  if (state_ == kError || !vda_) {
    base::ResetAndReturn(&pending_decode_cb_).Run(kDecodeError, NULL);
    return;
  }

  switch (state_) {
    case kDecoderDrained:
      if (!ready_video_frames_.empty()) {
        EnqueueFrameAndTriggerFrameDelivery(NULL);
        return;
      }
      state_ = kNormal;
      // Fall-through.
    case kNormal:
      break;
    case kDrainingDecoder:
      DCHECK(buffer->end_of_stream());
      // Do nothing.  Will be satisfied either by a PictureReady or
      // NotifyFlushDone below.
      return;
    case kError:
      NOTREACHED();
      return;
  }

  if (buffer->end_of_stream()) {
    if (state_ == kNormal) {
      state_ = kDrainingDecoder;
      vda_->Flush();
    }
    return;
  }

  size_t size = buffer->data_size();
  SHMBuffer* shm_buffer = GetSHM(size);
  if (!shm_buffer) {
    base::ResetAndReturn(&pending_decode_cb_).Run(kDecodeError, NULL);
    return;
  }

  memcpy(shm_buffer->shm->memory(), buffer->data(), size);
  BitstreamBuffer bitstream_buffer(
      next_bitstream_buffer_id_, shm_buffer->shm->handle(), size);
  // Mask against 30 bits, to avoid (undefined) wraparound on signed integer.
  next_bitstream_buffer_id_ = (next_bitstream_buffer_id_ + 1) & 0x3FFFFFFF;
  bool inserted = bitstream_buffers_in_decoder_.insert(std::make_pair(
      bitstream_buffer.id(), BufferPair(shm_buffer, buffer))).second;
  DCHECK(inserted);
  RecordBufferData(bitstream_buffer, *buffer.get());

  vda_->Decode(bitstream_buffer);

  if (!ready_video_frames_.empty()) {
    EnqueueFrameAndTriggerFrameDelivery(NULL);
    return;
  }

  if (CanMoreDecodeWorkBeDone())
    base::ResetAndReturn(&pending_decode_cb_).Run(kNotEnoughData, NULL);
}

bool GpuVideoDecoder::CanMoreDecodeWorkBeDone() {
  return bitstream_buffers_in_decoder_.size() < kMaxInFlightDecodes;
}

void GpuVideoDecoder::RecordBufferData(const BitstreamBuffer& bitstream_buffer,
                                       const DecoderBuffer& buffer) {
  input_buffer_data_.push_front(BufferData(bitstream_buffer.id(),
                                           buffer.timestamp(),
                                           config_.visible_rect(),
                                           config_.natural_size()));
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
                                    gfx::Rect* visible_rect,
                                    gfx::Size* natural_size) {
  for (std::list<BufferData>::const_iterator it =
           input_buffer_data_.begin(); it != input_buffer_data_.end();
       ++it) {
    if (it->bitstream_buffer_id != id)
      continue;
    *timestamp = it->timestamp;
    *visible_rect = it->visible_rect;
    *natural_size = it->natural_size;
    return;
  }
  NOTREACHED() << "Missing bitstreambuffer id: " << id;
}

bool GpuVideoDecoder::HasAlpha() const {
  DCHECK(gvd_loop_proxy_->BelongsToCurrentThread());
  return true;
}

bool GpuVideoDecoder::NeedsBitstreamConversion() const {
  DCHECK(gvd_loop_proxy_->BelongsToCurrentThread());
  return needs_bitstream_conversion_;
}

bool GpuVideoDecoder::CanReadWithoutStalling() const {
  DCHECK(gvd_loop_proxy_->BelongsToCurrentThread());
  return available_pictures_ > 0 || !ready_video_frames_.empty();
}

void GpuVideoDecoder::NotifyInitializeDone() {
  NOTREACHED() << "GpuVideoDecodeAcceleratorHost::Initialize is synchronous!";
}

void GpuVideoDecoder::ProvidePictureBuffers(uint32 count,
                                            const gfx::Size& size,
                                            uint32 texture_target) {
  DVLOG(3) << "ProvidePictureBuffers(" << count << ", "
           << size.width() << "x" << size.height() << ")";
  DCHECK(gvd_loop_proxy_->BelongsToCurrentThread());

  std::vector<uint32> texture_ids;
  std::vector<gpu::Mailbox> texture_mailboxes;
  decoder_texture_target_ = texture_target;
  // Discards the sync point returned here since PictureReady will imply that
  // the produce has already happened, and the texture is ready for use.
  if (!factories_->CreateTextures(count,
                                  size,
                                  &texture_ids,
                                  &texture_mailboxes,
                                  decoder_texture_target_)) {
    NotifyError(VideoDecodeAccelerator::PLATFORM_FAILURE);
    return;
  }
  DCHECK_EQ(count, texture_ids.size());
  DCHECK_EQ(count, texture_mailboxes.size());

  if (!vda_)
    return;

  std::vector<PictureBuffer> picture_buffers;
  for (size_t i = 0; i < texture_ids.size(); ++i) {
    picture_buffers.push_back(PictureBuffer(
        next_picture_buffer_id_++, size, texture_ids[i], texture_mailboxes[i]));
    bool inserted = assigned_picture_buffers_.insert(std::make_pair(
        picture_buffers.back().id(), picture_buffers.back())).second;
    DCHECK(inserted);
  }

  available_pictures_ += count;

  vda_->AssignPictureBuffers(picture_buffers);
}

void GpuVideoDecoder::DismissPictureBuffer(int32 id) {
  DVLOG(3) << "DismissPictureBuffer(" << id << ")";
  DCHECK(gvd_loop_proxy_->BelongsToCurrentThread());

  std::map<int32, PictureBuffer>::iterator it =
      assigned_picture_buffers_.find(id);
  if (it == assigned_picture_buffers_.end()) {
    NOTREACHED() << "Missing picture buffer: " << id;
    return;
  }

  PictureBuffer buffer_to_dismiss = it->second;
  assigned_picture_buffers_.erase(it);

  std::set<int32>::iterator at_display_it =
      picture_buffers_at_display_.find(id);

  if (at_display_it == picture_buffers_at_display_.end()) {
    // We can delete the texture immediately as it's not being displayed.
    factories_->DeleteTexture(buffer_to_dismiss.texture_id());
    CHECK_GT(available_pictures_, 0);
    --available_pictures_;
  } else {
    // Texture in display. Postpone deletion until after it's returned to us.
    bool inserted = dismissed_picture_buffers_.insert(std::make_pair(
        id, buffer_to_dismiss)).second;
    DCHECK(inserted);
  }
}

void GpuVideoDecoder::PictureReady(const media::Picture& picture) {
  DVLOG(3) << "PictureReady()";
  DCHECK(gvd_loop_proxy_->BelongsToCurrentThread());

  std::map<int32, PictureBuffer>::iterator it =
      assigned_picture_buffers_.find(picture.picture_buffer_id());
  if (it == assigned_picture_buffers_.end()) {
    NOTREACHED() << "Missing picture buffer: " << picture.picture_buffer_id();
    NotifyError(VideoDecodeAccelerator::PLATFORM_FAILURE);
    return;
  }
  const PictureBuffer& pb = it->second;

  // Update frame's timestamp.
  base::TimeDelta timestamp;
  gfx::Rect visible_rect;
  gfx::Size natural_size;
  GetBufferData(picture.bitstream_buffer_id(), &timestamp, &visible_rect,
                &natural_size);
  DCHECK(decoder_texture_target_);

  scoped_refptr<VideoFrame> frame(VideoFrame::WrapNativeTexture(
      new VideoFrame::MailboxHolder(
          pb.texture_mailbox(),
          0,  // sync_point
          BindToCurrentLoop(base::Bind(&GpuVideoDecoder::ReusePictureBuffer,
                                       weak_this_,
                                       picture.picture_buffer_id()))),
      decoder_texture_target_,
      pb.size(),
      visible_rect,
      natural_size,
      timestamp,
      base::Bind(&GpuVideoDecoderFactories::ReadPixels,
                 factories_,
                 pb.texture_id(),
                 decoder_texture_target_,
                 gfx::Size(visible_rect.width(), visible_rect.height())),
      base::Closure()));
  CHECK_GT(available_pictures_, 0);
  --available_pictures_;
  bool inserted =
      picture_buffers_at_display_.insert(picture.picture_buffer_id()).second;
  DCHECK(inserted);

  EnqueueFrameAndTriggerFrameDelivery(frame);
}

void GpuVideoDecoder::EnqueueFrameAndTriggerFrameDelivery(
    const scoped_refptr<VideoFrame>& frame) {
  DCHECK(gvd_loop_proxy_->BelongsToCurrentThread());

  // During a pending vda->Reset(), we don't accumulate frames.  Drop it on the
  // floor and return.
  if (!pending_reset_cb_.is_null())
    return;

  if (frame.get())
    ready_video_frames_.push_back(frame);
  else
    DCHECK(!ready_video_frames_.empty());

  if (pending_decode_cb_.is_null())
    return;

  base::ResetAndReturn(&pending_decode_cb_)
      .Run(kOk, ready_video_frames_.front());
  ready_video_frames_.pop_front();
}

void GpuVideoDecoder::ReusePictureBuffer(int64 picture_buffer_id,
                                         uint32 sync_point) {
  DVLOG(3) << "ReusePictureBuffer(" << picture_buffer_id << ")";
  DCHECK(gvd_loop_proxy_->BelongsToCurrentThread());

  if (!vda_)
    return;

  CHECK(!picture_buffers_at_display_.empty());

  size_t num_erased = picture_buffers_at_display_.erase(picture_buffer_id);
  DCHECK(num_erased);

  std::map<int32, PictureBuffer>::iterator it =
      assigned_picture_buffers_.find(picture_buffer_id);

  if (it == assigned_picture_buffers_.end()) {
    // This picture was dismissed while in display, so we postponed deletion.
    it = dismissed_picture_buffers_.find(picture_buffer_id);
    DCHECK(it != dismissed_picture_buffers_.end());
    factories_->DeleteTexture(it->second.texture_id());
    dismissed_picture_buffers_.erase(it);
    return;
  }

  factories_->WaitSyncPoint(sync_point);
  ++available_pictures_;

  vda_->ReusePictureBuffer(picture_buffer_id);
}

GpuVideoDecoder::SHMBuffer* GpuVideoDecoder::GetSHM(size_t min_size) {
  DCHECK(gvd_loop_proxy_->BelongsToCurrentThread());
  if (available_shm_segments_.empty() ||
      available_shm_segments_.back()->size < min_size) {
    size_t size_to_allocate = std::max(min_size, kSharedMemorySegmentBytes);
    base::SharedMemory* shm = factories_->CreateSharedMemory(size_to_allocate);
    // CreateSharedMemory() can return NULL during Shutdown.
    if (!shm)
      return NULL;
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
  DVLOG(3) << "NotifyEndOfBitstreamBuffer(" << id << ")";
  DCHECK(gvd_loop_proxy_->BelongsToCurrentThread());

  std::map<int32, BufferPair>::iterator it =
      bitstream_buffers_in_decoder_.find(id);
  if (it == bitstream_buffers_in_decoder_.end()) {
    NotifyError(VideoDecodeAccelerator::PLATFORM_FAILURE);
    NOTREACHED() << "Missing bitstream buffer: " << id;
    return;
  }

  PutSHM(it->second.shm_buffer);
  bitstream_buffers_in_decoder_.erase(it);

  if (pending_reset_cb_.is_null() && state_ != kDrainingDecoder &&
      CanMoreDecodeWorkBeDone() && !pending_decode_cb_.is_null()) {
    base::ResetAndReturn(&pending_decode_cb_).Run(kNotEnoughData, NULL);
  }
}

GpuVideoDecoder::~GpuVideoDecoder() {
  DCHECK(!vda_.get());  // Stop should have been already called.
  DCHECK(pending_decode_cb_.is_null());
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

  DestroyTextures();
}

void GpuVideoDecoder::NotifyFlushDone() {
  DVLOG(3) << "NotifyFlushDone()";
  DCHECK(gvd_loop_proxy_->BelongsToCurrentThread());
  DCHECK_EQ(state_, kDrainingDecoder);
  state_ = kDecoderDrained;
  EnqueueFrameAndTriggerFrameDelivery(VideoFrame::CreateEmptyFrame());
}

void GpuVideoDecoder::NotifyResetDone() {
  DVLOG(3) << "NotifyResetDone()";
  DCHECK(gvd_loop_proxy_->BelongsToCurrentThread());
  DCHECK(ready_video_frames_.empty());

  // This needs to happen after the Reset() on vda_ is done to ensure pictures
  // delivered during the reset can find their time data.
  input_buffer_data_.clear();

  if (!pending_reset_cb_.is_null())
    base::ResetAndReturn(&pending_reset_cb_).Run();

  if (!pending_decode_cb_.is_null())
    EnqueueFrameAndTriggerFrameDelivery(VideoFrame::CreateEmptyFrame());
}

void GpuVideoDecoder::NotifyError(media::VideoDecodeAccelerator::Error error) {
  DCHECK(gvd_loop_proxy_->BelongsToCurrentThread());
  if (!vda_)
    return;

  DLOG(ERROR) << "VDA Error: " << error;
  DestroyVDA();

  state_ = kError;

  if (!pending_decode_cb_.is_null()) {
    base::ResetAndReturn(&pending_decode_cb_).Run(kDecodeError, NULL);
    return;
  }
}

}  // namespace media
