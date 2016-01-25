// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/gpu_video_decoder.h"

#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/cpu.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/histogram.h"
#include "base/stl_util.h"
#include "base/task_runner_util.h"
#include "build/build_config.h"
#include "gpu/command_buffer/common/mailbox_holder.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/decoder_buffer.h"
#include "media/base/media_switches.h"
#include "media/base/pipeline.h"
#include "media/base/video_decoder_config.h"
#include "media/renderers/gpu_video_accelerator_factories.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace media {

const char GpuVideoDecoder::kDecoderName[] = "GpuVideoDecoder";

// Maximum number of concurrent VDA::Decode() operations GVD will maintain.
// Higher values allow better pipelining in the GPU, but also require more
// resources.
enum { kMaxInFlightDecodes = 4 };

// Size of shared-memory segments we allocate.  Since we reuse them we let them
// be on the beefy side.
static const size_t kSharedMemorySegmentBytes = 100 << 10;

GpuVideoDecoder::SHMBuffer::SHMBuffer(scoped_ptr<base::SharedMemory> m,
                                      size_t s)
    : shm(std::move(m)), size(s) {}

GpuVideoDecoder::SHMBuffer::~SHMBuffer() {}

GpuVideoDecoder::PendingDecoderBuffer::PendingDecoderBuffer(
    SHMBuffer* s,
    const scoped_refptr<DecoderBuffer>& b,
    const DecodeCB& done_cb)
    : shm_buffer(s), buffer(b), done_cb(done_cb) {
}

GpuVideoDecoder::PendingDecoderBuffer::~PendingDecoderBuffer() {}

GpuVideoDecoder::BufferData::BufferData(int32_t bbid,
                                        base::TimeDelta ts,
                                        const gfx::Rect& vr,
                                        const gfx::Size& ns)
    : bitstream_buffer_id(bbid),
      timestamp(ts),
      visible_rect(vr),
      natural_size(ns) {}

GpuVideoDecoder::BufferData::~BufferData() {}

GpuVideoDecoder::GpuVideoDecoder(GpuVideoAcceleratorFactories* factories)
    : needs_bitstream_conversion_(false),
      factories_(factories),
      state_(kNormal),
      decoder_texture_target_(0),
      next_picture_buffer_id_(0),
      next_bitstream_buffer_id_(0),
      available_pictures_(0),
      needs_all_picture_buffers_to_decode_(false),
      weak_factory_(this) {
  DCHECK(factories_);
}

void GpuVideoDecoder::Reset(const base::Closure& closure)  {
  DVLOG(3) << "Reset()";
  DCheckGpuVideoAcceleratorFactoriesTaskRunnerIsCurrent();

  if (state_ == kDrainingDecoder) {
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(
            &GpuVideoDecoder::Reset, weak_factory_.GetWeakPtr(), closure));
    return;
  }

  if (!vda_) {
    base::MessageLoop::current()->PostTask(FROM_HERE, closure);
    return;
  }

  DCHECK(pending_reset_cb_.is_null());
  pending_reset_cb_ = BindToCurrentLoop(closure);

  vda_->Reset();
}

static bool IsCodedSizeSupported(const gfx::Size& coded_size,
                                 const gfx::Size& min_resolution,
                                 const gfx::Size& max_resolution) {
  return (coded_size.width() <= max_resolution.width() &&
      coded_size.height() <= max_resolution.height() &&
      coded_size.width() >= min_resolution.width() &&
      coded_size.height() >= min_resolution.height());
}

// Report |success| to UMA and run |cb| with it.  This is super-specific to the
// UMA stat reported because the UMA_HISTOGRAM_ENUMERATION API requires a
// callsite to always be called with the same stat name (can't parameterize it).
static void ReportGpuVideoDecoderInitializeStatusToUMAAndRunCB(
    const VideoDecoder::InitCB& cb,
    bool success) {
  // TODO(xhwang): Report |success| directly.
  PipelineStatus status = success ? PIPELINE_OK : DECODER_ERROR_NOT_SUPPORTED;
  UMA_HISTOGRAM_ENUMERATION(
      "Media.GpuVideoDecoderInitializeStatus", status, PIPELINE_STATUS_MAX + 1);
  cb.Run(success);
}

std::string GpuVideoDecoder::GetDisplayName() const {
  return kDecoderName;
}

void GpuVideoDecoder::Initialize(const VideoDecoderConfig& config,
                                 bool /* low_delay */,
                                 const SetCdmReadyCB& set_cdm_ready_cb,
                                 const InitCB& init_cb,
                                 const OutputCB& output_cb) {
  DVLOG(3) << "Initialize()";
  DCheckGpuVideoAcceleratorFactoriesTaskRunnerIsCurrent();
  DCHECK(config.IsValidConfig());

  InitCB bound_init_cb =
      base::Bind(&ReportGpuVideoDecoderInitializeStatusToUMAAndRunCB,
                 BindToCurrentLoop(init_cb));

#if !defined(OS_ANDROID)
  if (config.is_encrypted()) {
    DVLOG(1) << "Encrypted stream not supported.";
    bound_init_cb.Run(false);
    return;
  }
#endif

  bool previously_initialized = config_.IsValidConfig();
  DVLOG(1) << "(Re)initializing GVD with config: "
           << config.AsHumanReadableString();

  // TODO(posciak): destroy and create a new VDA on codec/profile change
  // (http://crbug.com/260224).
  if (previously_initialized && (config_.profile() != config.profile())) {
    DVLOG(1) << "Codec or profile changed, cannot reinitialize.";
    bound_init_cb.Run(false);
    return;
  }

  VideoDecodeAccelerator::Capabilities capabilities =
      factories_->GetVideoDecodeAcceleratorCapabilities();
  if (!IsProfileSupported(capabilities, config.profile(),
                          config.coded_size())) {
    DVLOG(1) << "Profile " << config.profile() << " or coded size "
             << config.coded_size().ToString() << " not supported.";
    bound_init_cb.Run(false);
    return;
  }

  config_ = config;
  needs_all_picture_buffers_to_decode_ =
      capabilities.flags &
      VideoDecodeAccelerator::Capabilities::NEEDS_ALL_PICTURE_BUFFERS_TO_DECODE;
  needs_bitstream_conversion_ = (config.codec() == kCodecH264);
  output_cb_ = BindToCurrentLoop(output_cb);

  if (previously_initialized) {
    DVLOG(3) << __FUNCTION__
             << " Expecting initialized VDA to detect in-stream config change.";
    // Reinitialization with a different config (but same codec and profile).
    // VDA should handle it by detecting this in-stream by itself,
    // no need to notify it.
    bound_init_cb.Run(true);
    return;
  }

  vda_ = factories_->CreateVideoDecodeAccelerator();

  VideoDecodeAccelerator::Config vda_config(config);

  if (!vda_ || !vda_->Initialize(vda_config, this)) {
    DVLOG(1) << "VDA initialization failed.";
    bound_init_cb.Run(false);
    return;
  }

  if (config.is_encrypted()) {
    init_cb_ = bound_init_cb;
    set_cdm_ready_cb_ = set_cdm_ready_cb;
    set_cdm_ready_cb_.Run(BindToCurrentLoop(
        base::Bind(&GpuVideoDecoder::SetCdm, weak_factory_.GetWeakPtr())));
    return;
  }

  DVLOG(3) << "GpuVideoDecoder::Initialize() succeeded.";
  bound_init_cb.Run(true);
}

void GpuVideoDecoder::SetCdm(CdmContext* cdm_context,
                             const CdmAttachedCB& cdm_attached_cb) {
  DVLOG(2) << __FUNCTION__;
  DCheckGpuVideoAcceleratorFactoriesTaskRunnerIsCurrent();

  DCHECK(!init_cb_.is_null());
  DCHECK(!set_cdm_ready_cb_.is_null());
  set_cdm_ready_cb_.Reset();

  if (!cdm_context || cdm_context->GetCdmId() == CdmContext::kInvalidCdmId) {
    DVLOG(1) << "CDM ID not available.";
    cdm_attached_cb.Run(false);
    base::ResetAndReturn(&init_cb_).Run(false);
    return;
  }

  cdm_attached_cb_ = cdm_attached_cb;
  vda_->SetCdm(cdm_context->GetCdmId());
}

void GpuVideoDecoder::NotifyCdmAttached(bool success) {
  DVLOG_IF(2, !success) << __FUNCTION__ << ": CDM not attached.";
  DCHECK(!init_cb_.is_null());
  DCHECK(!cdm_attached_cb_.is_null());

  base::ResetAndReturn(&cdm_attached_cb_).Run(success);
  base::ResetAndReturn(&init_cb_).Run(success);
}

void GpuVideoDecoder::DestroyPictureBuffers(PictureBufferMap* buffers) {
  DCheckGpuVideoAcceleratorFactoriesTaskRunnerIsCurrent();
  for (PictureBufferMap::iterator it = buffers->begin(); it != buffers->end();
       ++it) {
    factories_->DeleteTexture(it->second.texture_id());
  }

  buffers->clear();
}

void GpuVideoDecoder::DestroyVDA() {
  DCheckGpuVideoAcceleratorFactoriesTaskRunnerIsCurrent();

  vda_.reset();

  // Not destroying PictureBuffers in |picture_buffers_at_display_| yet, since
  // their textures may still be in use by the user of this GpuVideoDecoder.
  for (PictureBufferTextureMap::iterator it =
           picture_buffers_at_display_.begin();
       it != picture_buffers_at_display_.end();
       ++it) {
    assigned_picture_buffers_.erase(it->first);
  }
  DestroyPictureBuffers(&assigned_picture_buffers_);
}

void GpuVideoDecoder::Decode(const scoped_refptr<DecoderBuffer>& buffer,
                             const DecodeCB& decode_cb) {
  DCheckGpuVideoAcceleratorFactoriesTaskRunnerIsCurrent();
  DCHECK(pending_reset_cb_.is_null());

  DVLOG(3) << __FUNCTION__ << " " << buffer->AsHumanReadableString();

  DecodeCB bound_decode_cb = BindToCurrentLoop(decode_cb);

  if (state_ == kError || !vda_) {
    bound_decode_cb.Run(kDecodeError);
    return;
  }

  switch (state_) {
    case kDecoderDrained:
      state_ = kNormal;
      // Fall-through.
    case kNormal:
      break;
    case kDrainingDecoder:
    case kError:
      NOTREACHED();
      return;
  }

  DCHECK_EQ(state_, kNormal);

  if (buffer->end_of_stream()) {
    DVLOG(3) << __FUNCTION__ << " Initiating Flush for EOS.";
    state_ = kDrainingDecoder;
    eos_decode_cb_ = bound_decode_cb;
    vda_->Flush();
    return;
  }

  size_t size = buffer->data_size();
  scoped_ptr<SHMBuffer> shm_buffer = GetSHM(size);
  if (!shm_buffer) {
    bound_decode_cb.Run(kDecodeError);
    return;
  }

  memcpy(shm_buffer->shm->memory(), buffer->data(), size);
  // AndroidVideoDecodeAccelerator needs the timestamp to output frames in
  // presentation order.
  BitstreamBuffer bitstream_buffer(next_bitstream_buffer_id_,
                                   shm_buffer->shm->handle(), size,
                                   buffer->timestamp());

  if (buffer->decrypt_config())
    bitstream_buffer.SetDecryptConfig(*buffer->decrypt_config());

  // Mask against 30 bits, to avoid (undefined) wraparound on signed integer.
  next_bitstream_buffer_id_ = (next_bitstream_buffer_id_ + 1) & 0x3FFFFFFF;
  DCHECK(!ContainsKey(bitstream_buffers_in_decoder_, bitstream_buffer.id()));
  bitstream_buffers_in_decoder_.insert(std::make_pair(
      bitstream_buffer.id(),
      PendingDecoderBuffer(shm_buffer.release(), buffer, decode_cb)));
  DCHECK_LE(static_cast<int>(bitstream_buffers_in_decoder_.size()),
            kMaxInFlightDecodes);
  RecordBufferData(bitstream_buffer, *buffer.get());

  vda_->Decode(bitstream_buffer);
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

void GpuVideoDecoder::GetBufferData(int32_t id,
                                    base::TimeDelta* timestamp,
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

bool GpuVideoDecoder::NeedsBitstreamConversion() const {
  DCheckGpuVideoAcceleratorFactoriesTaskRunnerIsCurrent();
  return needs_bitstream_conversion_;
}

bool GpuVideoDecoder::CanReadWithoutStalling() const {
  DCheckGpuVideoAcceleratorFactoriesTaskRunnerIsCurrent();
  return next_picture_buffer_id_ ==
             0 ||  // Decode() will ProvidePictureBuffers().
         (!needs_all_picture_buffers_to_decode_ && available_pictures_ > 0) ||
         available_pictures_ ==
             static_cast<int>(assigned_picture_buffers_.size());
}

int GpuVideoDecoder::GetMaxDecodeRequests() const {
  return kMaxInFlightDecodes;
}

void GpuVideoDecoder::ProvidePictureBuffers(uint32_t count,
                                            const gfx::Size& size,
                                            uint32_t texture_target) {
  DVLOG(3) << "ProvidePictureBuffers(" << count << ", "
           << size.width() << "x" << size.height() << ")";
  DCheckGpuVideoAcceleratorFactoriesTaskRunnerIsCurrent();

  std::vector<uint32_t> texture_ids;
  std::vector<gpu::Mailbox> texture_mailboxes;
  decoder_texture_target_ = texture_target;
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

void GpuVideoDecoder::DismissPictureBuffer(int32_t id) {
  DVLOG(3) << "DismissPictureBuffer(" << id << ")";
  DCheckGpuVideoAcceleratorFactoriesTaskRunnerIsCurrent();

  PictureBufferMap::iterator it = assigned_picture_buffers_.find(id);
  if (it == assigned_picture_buffers_.end()) {
    NOTREACHED() << "Missing picture buffer: " << id;
    return;
  }

  PictureBuffer buffer_to_dismiss = it->second;
  assigned_picture_buffers_.erase(it);

  if (!picture_buffers_at_display_.count(id)) {
    // We can delete the texture immediately as it's not being displayed.
    factories_->DeleteTexture(buffer_to_dismiss.texture_id());
    CHECK_GT(available_pictures_, 0);
    --available_pictures_;
  }
  // Not destroying a texture in display in |picture_buffers_at_display_|.
  // Postpone deletion until after it's returned to us.
}

void GpuVideoDecoder::PictureReady(const media::Picture& picture) {
  DVLOG(3) << "PictureReady()";
  DCheckGpuVideoAcceleratorFactoriesTaskRunnerIsCurrent();

  PictureBufferMap::iterator it =
      assigned_picture_buffers_.find(picture.picture_buffer_id());
  if (it == assigned_picture_buffers_.end()) {
    DLOG(ERROR) << "Missing picture buffer: " << picture.picture_buffer_id();
    NotifyError(VideoDecodeAccelerator::PLATFORM_FAILURE);
    return;
  }
  const PictureBuffer& pb = it->second;

  // Update frame's timestamp.
  base::TimeDelta timestamp;
  // Some of the VDAs like DXVA, AVDA, and VTVDA don't support and thus don't
  // provide us with visible size in picture.size, passing (0, 0) instead, so
  // for those cases drop it and use config information instead.
  gfx::Rect visible_rect;
  gfx::Size natural_size;
  GetBufferData(picture.bitstream_buffer_id(), &timestamp, &visible_rect,
                &natural_size);

  if (!picture.visible_rect().IsEmpty()) {
    visible_rect = picture.visible_rect();
  }
  if (!gfx::Rect(pb.size()).Contains(visible_rect)) {
    LOG(WARNING) << "Visible size " << visible_rect.ToString()
                 << " is larger than coded size " << pb.size().ToString();
    visible_rect = gfx::Rect(pb.size());
  }

  DCHECK(decoder_texture_target_);

  bool opaque = IsOpaque(config_.format());

  scoped_refptr<VideoFrame> frame(VideoFrame::WrapNativeTexture(
      opaque ? PIXEL_FORMAT_XRGB : PIXEL_FORMAT_ARGB,
      gpu::MailboxHolder(pb.texture_mailbox(), gpu::SyncToken(),
                         decoder_texture_target_),
      BindToCurrentLoop(base::Bind(
          &GpuVideoDecoder::ReleaseMailbox, weak_factory_.GetWeakPtr(),
          factories_, picture.picture_buffer_id(), pb.texture_id())),
      pb.size(), visible_rect, natural_size, timestamp));
  if (!frame) {
    DLOG(ERROR) << "Create frame failed for: " << picture.picture_buffer_id();
    NotifyError(VideoDecodeAccelerator::PLATFORM_FAILURE);
    return;
  }
  if (picture.allow_overlay())
    frame->metadata()->SetBoolean(VideoFrameMetadata::ALLOW_OVERLAY, true);
  CHECK_GT(available_pictures_, 0);
  --available_pictures_;
  bool inserted =
      picture_buffers_at_display_.insert(std::make_pair(
                                             picture.picture_buffer_id(),
                                             pb.texture_id())).second;
  DCHECK(inserted);

  DeliverFrame(frame);
}

void GpuVideoDecoder::DeliverFrame(
    const scoped_refptr<VideoFrame>& frame) {
  DCheckGpuVideoAcceleratorFactoriesTaskRunnerIsCurrent();

  // During a pending vda->Reset(), we don't accumulate frames.  Drop it on the
  // floor and return.
  if (!pending_reset_cb_.is_null())
    return;

  output_cb_.Run(frame);
}

// static
void GpuVideoDecoder::ReleaseMailbox(
    base::WeakPtr<GpuVideoDecoder> decoder,
    media::GpuVideoAcceleratorFactories* factories,
    int64_t picture_buffer_id,
    uint32_t texture_id,
    const gpu::SyncToken& release_sync_token) {
  DCHECK(factories->GetTaskRunner()->BelongsToCurrentThread());
  factories->WaitSyncToken(release_sync_token);

  if (decoder) {
    decoder->ReusePictureBuffer(picture_buffer_id);
    return;
  }
  // It's the last chance to delete the texture after display,
  // because GpuVideoDecoder was destructed.
  factories->DeleteTexture(texture_id);
}

void GpuVideoDecoder::ReusePictureBuffer(int64_t picture_buffer_id) {
  DVLOG(3) << "ReusePictureBuffer(" << picture_buffer_id << ")";
  DCheckGpuVideoAcceleratorFactoriesTaskRunnerIsCurrent();

  DCHECK(!picture_buffers_at_display_.empty());
  PictureBufferTextureMap::iterator display_iterator =
      picture_buffers_at_display_.find(picture_buffer_id);
  uint32_t texture_id = display_iterator->second;
  DCHECK(display_iterator != picture_buffers_at_display_.end());
  picture_buffers_at_display_.erase(display_iterator);

  if (!assigned_picture_buffers_.count(picture_buffer_id)) {
    // This picture was dismissed while in display, so we postponed deletion.
    factories_->DeleteTexture(texture_id);
    return;
  }

  ++available_pictures_;

  // DestroyVDA() might already have been called.
  if (vda_)
    vda_->ReusePictureBuffer(picture_buffer_id);
}

scoped_ptr<GpuVideoDecoder::SHMBuffer> GpuVideoDecoder::GetSHM(
    size_t min_size) {
  DCheckGpuVideoAcceleratorFactoriesTaskRunnerIsCurrent();
  if (available_shm_segments_.empty() ||
      available_shm_segments_.back()->size < min_size) {
    size_t size_to_allocate = std::max(min_size, kSharedMemorySegmentBytes);
    scoped_ptr<base::SharedMemory> shm =
        factories_->CreateSharedMemory(size_to_allocate);
    // CreateSharedMemory() can return NULL during Shutdown.
    if (!shm)
      return NULL;
    return make_scoped_ptr(new SHMBuffer(std::move(shm), size_to_allocate));
  }
  scoped_ptr<SHMBuffer> ret(available_shm_segments_.back());
  available_shm_segments_.pop_back();
  return ret;
}

void GpuVideoDecoder::PutSHM(scoped_ptr<SHMBuffer> shm_buffer) {
  DCheckGpuVideoAcceleratorFactoriesTaskRunnerIsCurrent();
  available_shm_segments_.push_back(shm_buffer.release());
}

void GpuVideoDecoder::NotifyEndOfBitstreamBuffer(int32_t id) {
  DVLOG(3) << "NotifyEndOfBitstreamBuffer(" << id << ")";
  DCheckGpuVideoAcceleratorFactoriesTaskRunnerIsCurrent();

  std::map<int32_t, PendingDecoderBuffer>::iterator it =
      bitstream_buffers_in_decoder_.find(id);
  if (it == bitstream_buffers_in_decoder_.end()) {
    NotifyError(VideoDecodeAccelerator::PLATFORM_FAILURE);
    NOTREACHED() << "Missing bitstream buffer: " << id;
    return;
  }

  PutSHM(make_scoped_ptr(it->second.shm_buffer));
  it->second.done_cb.Run(state_ == kError ? kDecodeError : kOk);
  bitstream_buffers_in_decoder_.erase(it);
}

GpuVideoDecoder::~GpuVideoDecoder() {
  DVLOG(3) << __FUNCTION__;
  DCheckGpuVideoAcceleratorFactoriesTaskRunnerIsCurrent();

  if (vda_)
    DestroyVDA();
  DCHECK(assigned_picture_buffers_.empty());

  if (!set_cdm_ready_cb_.is_null())
    base::ResetAndReturn(&set_cdm_ready_cb_).Run(CdmReadyCB());
  if (!cdm_attached_cb_.is_null())
    base::ResetAndReturn(&cdm_attached_cb_).Run(false);
  if (!init_cb_.is_null())
    base::ResetAndReturn(&init_cb_).Run(false);

  for (size_t i = 0; i < available_shm_segments_.size(); ++i) {
    delete available_shm_segments_[i];
  }
  available_shm_segments_.clear();

  for (std::map<int32_t, PendingDecoderBuffer>::iterator it =
           bitstream_buffers_in_decoder_.begin();
       it != bitstream_buffers_in_decoder_.end(); ++it) {
    delete it->second.shm_buffer;
    it->second.done_cb.Run(kAborted);
  }
  bitstream_buffers_in_decoder_.clear();

  if (!pending_reset_cb_.is_null())
    base::ResetAndReturn(&pending_reset_cb_).Run();
}

void GpuVideoDecoder::NotifyFlushDone() {
  DVLOG(3) << "NotifyFlushDone()";
  DCheckGpuVideoAcceleratorFactoriesTaskRunnerIsCurrent();
  DCHECK_EQ(state_, kDrainingDecoder);
  state_ = kDecoderDrained;
  base::ResetAndReturn(&eos_decode_cb_).Run(kOk);
}

void GpuVideoDecoder::NotifyResetDone() {
  DVLOG(3) << "NotifyResetDone()";
  DCheckGpuVideoAcceleratorFactoriesTaskRunnerIsCurrent();
  DCHECK(bitstream_buffers_in_decoder_.empty());

  // This needs to happen after the Reset() on vda_ is done to ensure pictures
  // delivered during the reset can find their time data.
  input_buffer_data_.clear();

  if (!pending_reset_cb_.is_null())
    base::ResetAndReturn(&pending_reset_cb_).Run();
}

void GpuVideoDecoder::NotifyError(media::VideoDecodeAccelerator::Error error) {
  DCheckGpuVideoAcceleratorFactoriesTaskRunnerIsCurrent();
  if (!vda_)
    return;

  state_ = kError;

  // If we have any bitstream buffers, then notify one that an error has
  // occurred.  This guarantees that somebody finds out about the error.  If
  // we don't do this, and if the max decodes are already in flight, then there
  // won't be another decode request to report the error.
  if (!bitstream_buffers_in_decoder_.empty()) {
    auto it = bitstream_buffers_in_decoder_.begin();
    it->second.done_cb.Run(kDecodeError);
    bitstream_buffers_in_decoder_.erase(it);
  }

  DLOG(ERROR) << "VDA Error: " << error;
  DestroyVDA();
}

bool GpuVideoDecoder::IsProfileSupported(
    const VideoDecodeAccelerator::Capabilities& capabilities,
    VideoCodecProfile profile,
    const gfx::Size& coded_size) {
  DCheckGpuVideoAcceleratorFactoriesTaskRunnerIsCurrent();
  for (const auto& supported_profile : capabilities.supported_profiles) {
    if (profile == supported_profile.profile) {
      return IsCodedSizeSupported(coded_size,
                                  supported_profile.min_resolution,
                                  supported_profile.max_resolution);
    }
  }
  return false;
}

void GpuVideoDecoder::DCheckGpuVideoAcceleratorFactoriesTaskRunnerIsCurrent()
    const {
  DCHECK(factories_->GetTaskRunner()->BelongsToCurrentThread());
}

}  // namespace media
