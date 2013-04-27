// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/gpu_video_decoder.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/cpu.h"
#include "base/message_loop.h"
#include "base/stl_util.h"
#include "base/task_runner_util.h"
#include "media/base/bind_to_loop.h"
#include "media/base/decoder_buffer.h"
#include "media/base/demuxer_stream.h"
#include "media/base/pipeline.h"
#include "media/base/pipeline_status.h"
#include "media/base/video_decoder_config.h"

namespace media {

// Proxies calls to a VideoDecodeAccelerator::Client from the calling thread to
// the client's thread.
//
// TODO(scherkus): VDAClientProxy should hold onto GpuVideoDecoder::Factories
// and take care of some of the work that GpuVideoDecoder does to minimize
// thread hopping. See following for discussion:
//
// https://codereview.chromium.org/12989009/diff/27035/media/filters/gpu_video_decoder.cc#newcode23
class VDAClientProxy
    : public base::RefCountedThreadSafe<VDAClientProxy>,
      public VideoDecodeAccelerator::Client {
 public:
  explicit VDAClientProxy(VideoDecodeAccelerator::Client* client);

  // Detaches the proxy. |weak_client_| will no longer be called and can be
  // safely deleted. Any pending/future calls will be discarded.
  //
  // Must be called on |client_loop_|.
  void Detach();

  // VideoDecodeAccelerator::Client implementation.
  virtual void NotifyInitializeDone() OVERRIDE;
  virtual void ProvidePictureBuffers(uint32 count,
                                     const gfx::Size& size,
                                     uint32 texture_target) OVERRIDE;
  virtual void DismissPictureBuffer(int32 id) OVERRIDE;
  virtual void PictureReady(const media::Picture& picture) OVERRIDE;
  virtual void NotifyEndOfBitstreamBuffer(int32 id) OVERRIDE;
  virtual void NotifyFlushDone() OVERRIDE;
  virtual void NotifyResetDone() OVERRIDE;
  virtual void NotifyError(media::VideoDecodeAccelerator::Error error) OVERRIDE;

 private:
  friend class base::RefCountedThreadSafe<VDAClientProxy>;
  virtual ~VDAClientProxy();

  scoped_refptr<base::MessageLoopProxy> client_loop_;

  // Weak pointers are used to invalidate tasks posted to |client_loop_| after
  // Detach() has been called.
  base::WeakPtrFactory<VideoDecodeAccelerator::Client> weak_client_factory_;
  base::WeakPtr<VideoDecodeAccelerator::Client> weak_client_;

  DISALLOW_COPY_AND_ASSIGN(VDAClientProxy);
};

VDAClientProxy::VDAClientProxy(VideoDecodeAccelerator::Client* client)
    : client_loop_(base::MessageLoopProxy::current()),
      weak_client_factory_(client),
      weak_client_(weak_client_factory_.GetWeakPtr()) {
  DCHECK(weak_client_);
}

VDAClientProxy::~VDAClientProxy() {}

void VDAClientProxy::Detach() {
  DCHECK(client_loop_->BelongsToCurrentThread());
  DCHECK(weak_client_) << "Detach() already called";
  weak_client_factory_.InvalidateWeakPtrs();
}

void VDAClientProxy::NotifyInitializeDone() {
  client_loop_->PostTask(FROM_HERE, base::Bind(
      &VideoDecodeAccelerator::Client::NotifyInitializeDone, weak_client_));
}

void VDAClientProxy::ProvidePictureBuffers(uint32 count,
                                           const gfx::Size& size,
                                           uint32 texture_target) {
  client_loop_->PostTask(FROM_HERE, base::Bind(
      &VideoDecodeAccelerator::Client::ProvidePictureBuffers, weak_client_,
      count, size, texture_target));
}

void VDAClientProxy::DismissPictureBuffer(int32 id) {
  client_loop_->PostTask(FROM_HERE, base::Bind(
      &VideoDecodeAccelerator::Client::DismissPictureBuffer, weak_client_, id));
}

void VDAClientProxy::PictureReady(const media::Picture& picture) {
  client_loop_->PostTask(FROM_HERE, base::Bind(
      &VideoDecodeAccelerator::Client::PictureReady, weak_client_, picture));
}

void VDAClientProxy::NotifyEndOfBitstreamBuffer(int32 id) {
  client_loop_->PostTask(FROM_HERE, base::Bind(
      &VideoDecodeAccelerator::Client::NotifyEndOfBitstreamBuffer, weak_client_,
      id));
}

void VDAClientProxy::NotifyFlushDone() {
  client_loop_->PostTask(FROM_HERE, base::Bind(
      &VideoDecodeAccelerator::Client::NotifyFlushDone, weak_client_));
}

void VDAClientProxy::NotifyResetDone() {
  client_loop_->PostTask(FROM_HERE, base::Bind(
      &VideoDecodeAccelerator::Client::NotifyResetDone, weak_client_));
}

void VDAClientProxy::NotifyError(media::VideoDecodeAccelerator::Error error) {
  client_loop_->PostTask(FROM_HERE, base::Bind(
      &VideoDecodeAccelerator::Client::NotifyError, weak_client_, error));
}


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
    int32 bbid, base::TimeDelta ts, const gfx::Rect& vr, const gfx::Size& ns)
    : bitstream_buffer_id(bbid), timestamp(ts), visible_rect(vr),
      natural_size(ns) {
}

GpuVideoDecoder::BufferData::~BufferData() {}

GpuVideoDecoder::GpuVideoDecoder(
    const scoped_refptr<base::MessageLoopProxy>& message_loop,
    const scoped_refptr<Factories>& factories)
    : demuxer_stream_(NULL),
      gvd_loop_proxy_(message_loop),
      weak_factory_(this),
      vda_loop_proxy_(factories->GetMessageLoop()),
      factories_(factories),
      state_(kNormal),
      demuxer_read_in_progress_(false),
      decoder_texture_target_(0),
      next_picture_buffer_id_(0),
      next_bitstream_buffer_id_(0),
      available_pictures_(-1) {
  DCHECK(factories_);
}

void GpuVideoDecoder::Reset(const base::Closure& closure)  {
  DCHECK(gvd_loop_proxy_->BelongsToCurrentThread());

  if (state_ == kDrainingDecoder && !factories_->IsAborted()) {
    gvd_loop_proxy_->PostTask(FROM_HERE, base::Bind(
        &GpuVideoDecoder::Reset, weak_this_, closure));
    // NOTE: if we're deferring Reset() until a Flush() completes, return
    // queued pictures to the VDA so they can be used to finish that Flush().
    if (pending_read_cb_.is_null())
      ready_video_frames_.clear();
    return;
  }

  // Throw away any already-decoded, not-yet-delivered frames.
  ready_video_frames_.clear();

  if (!vda_) {
    gvd_loop_proxy_->PostTask(FROM_HERE, closure);
    return;
  }

  if (!pending_read_cb_.is_null())
    EnqueueFrameAndTriggerFrameDelivery(VideoFrame::CreateEmptyFrame());

  DCHECK(pending_reset_cb_.is_null());
  pending_reset_cb_ = BindToCurrentLoop(closure);

  vda_loop_proxy_->PostTask(FROM_HERE, base::Bind(
      &VideoDecodeAccelerator::Reset, weak_vda_));
}

void GpuVideoDecoder::Stop(const base::Closure& closure) {
  DCHECK(gvd_loop_proxy_->BelongsToCurrentThread());
  if (vda_)
    DestroyVDA();
  if (!pending_read_cb_.is_null())
    EnqueueFrameAndTriggerFrameDelivery(VideoFrame::CreateEmptyFrame());
  if (!pending_reset_cb_.is_null())
    base::ResetAndReturn(&pending_reset_cb_).Run();
  demuxer_stream_ = NULL;
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

void GpuVideoDecoder::Initialize(DemuxerStream* stream,
                                 const PipelineStatusCB& orig_status_cb,
                                 const StatisticsCB& statistics_cb) {
  DCHECK(gvd_loop_proxy_->BelongsToCurrentThread());
  DCHECK(stream);

  weak_this_ = weak_factory_.GetWeakPtr();

  PipelineStatusCB status_cb = CreateUMAReportingPipelineCB(
      "Media.GpuVideoDecoderInitializeStatus",
      BindToCurrentLoop(orig_status_cb));

  if (demuxer_stream_) {
    // TODO(xhwang): Make GpuVideoDecoder reinitializable.
    // See http://crbug.com/233608
    DVLOG(1) << "GpuVideoDecoder reinitialization not supported.";
    status_cb.Run(DECODER_ERROR_NOT_SUPPORTED);
    return;
  }

  const VideoDecoderConfig& config = stream->video_decoder_config();
  DCHECK(config.IsValidConfig());
  DCHECK(!config.is_encrypted());

  if (!IsCodedSizeSupported(config.coded_size())) {
    status_cb.Run(DECODER_ERROR_NOT_SUPPORTED);
    return;
  }

  client_proxy_ = new VDAClientProxy(this);
  VideoDecodeAccelerator* vda =
      factories_->CreateVideoDecodeAccelerator(config.profile(), client_proxy_);
  if (!vda) {
    status_cb.Run(DECODER_ERROR_NOT_SUPPORTED);
    return;
  }

  if (config.codec() == kCodecH264)
    stream->EnableBitstreamConverter();

  demuxer_stream_ = stream;
  statistics_cb_ = statistics_cb;

  DVLOG(1) << "GpuVideoDecoder::Initialize() succeeded.";
  PostTaskAndReplyWithResult(
      vda_loop_proxy_, FROM_HERE,
      base::Bind(&VideoDecodeAccelerator::AsWeakPtr, base::Unretained(vda)),
      base::Bind(&GpuVideoDecoder::SetVDA, weak_this_, status_cb, vda));
}

void GpuVideoDecoder::SetVDA(
    const PipelineStatusCB& status_cb,
    VideoDecodeAccelerator* vda,
    base::WeakPtr<VideoDecodeAccelerator> weak_vda) {
  DCHECK(gvd_loop_proxy_->BelongsToCurrentThread());
  DCHECK(!vda_.get());
  vda_.reset(vda);
  weak_vda_ = weak_vda;
  status_cb.Run(PIPELINE_OK);
}

void GpuVideoDecoder::DestroyTextures() {
  for (std::map<int32, PictureBuffer>::iterator it =
          picture_buffers_in_decoder_.begin();
          it != picture_buffers_in_decoder_.end(); ++it) {
    factories_->DeleteTexture(it->second.texture_id());
  }
  picture_buffers_in_decoder_.clear();
}

static void DestroyVDAWithClientProxy(
    const scoped_refptr<VDAClientProxy>& client_proxy,
    base::WeakPtr<VideoDecodeAccelerator> weak_vda) {
  if (weak_vda) {
    weak_vda->Destroy();
    DCHECK(!weak_vda);  // Check VDA::Destroy() contract.
  }
}

void GpuVideoDecoder::DestroyVDA() {
  DCHECK(gvd_loop_proxy_->BelongsToCurrentThread());

  // |client_proxy| must stay alive until |weak_vda_| has been destroyed.
  vda_loop_proxy_->PostTask(FROM_HERE, base::Bind(
      &DestroyVDAWithClientProxy, client_proxy_, weak_vda_));

  VideoDecodeAccelerator* vda ALLOW_UNUSED = vda_.release();
  client_proxy_->Detach();
  client_proxy_ = NULL;

  DestroyTextures();
}

void GpuVideoDecoder::Read(const ReadCB& read_cb) {
  DCHECK(gvd_loop_proxy_->BelongsToCurrentThread());
  DCHECK(pending_reset_cb_.is_null());
  DCHECK(pending_read_cb_.is_null());
  pending_read_cb_ = BindToCurrentLoop(read_cb);

  if (state_ == kError) {
    base::ResetAndReturn(&pending_read_cb_).Run(kDecodeError, NULL);
    return;
  }

  // TODO(xhwang): It's odd that we return kOk after VDA has been released.
  // Fix this and simplify cases.
  if (!vda_) {
    base::ResetAndReturn(&pending_read_cb_).Run(
        kOk, VideoFrame::CreateEmptyFrame());
    return;
  }

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
    case kError:
      NOTREACHED();
      break;
  }
}

bool GpuVideoDecoder::CanMoreDecodeWorkBeDone() {
  return bitstream_buffers_in_decoder_.size() < kMaxInFlightDecodes;
}

void GpuVideoDecoder::RequestBufferDecode(
    DemuxerStream::Status status,
    const scoped_refptr<DecoderBuffer>& buffer) {
  DCHECK(gvd_loop_proxy_->BelongsToCurrentThread());
  DCHECK_EQ(status != DemuxerStream::kOk, !buffer) << status;

  demuxer_read_in_progress_ = false;

  if (status == DemuxerStream::kAborted) {
    if (pending_read_cb_.is_null())
      return;
    base::ResetAndReturn(&pending_read_cb_).Run(kOk, NULL);
    return;
  }

  if (status == DemuxerStream::kAborted) {
    if (pending_read_cb_.is_null())
      return;
    // TODO(acolwell): Add support for reinitializing the decoder when
    // |status| == kConfigChanged. For now we just trigger a decode error.
    state_ = kError;
    base::ResetAndReturn(&pending_read_cb_).Run(kDecodeError, NULL);
    return;
  }

  DCHECK_EQ(status, DemuxerStream::kOk);

  if (!vda_) {
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
  if (!shm_buffer)
    return;

  memcpy(shm_buffer->shm->memory(), buffer->GetData(), size);
  BitstreamBuffer bitstream_buffer(
      next_bitstream_buffer_id_, shm_buffer->shm->handle(), size);
  // Mask against 30 bits, to avoid (undefined) wraparound on signed integer.
  next_bitstream_buffer_id_ = (next_bitstream_buffer_id_ + 1) & 0x3FFFFFFF;
  bool inserted = bitstream_buffers_in_decoder_.insert(std::make_pair(
      bitstream_buffer.id(), BufferPair(shm_buffer, buffer))).second;
  DCHECK(inserted);
  RecordBufferData(bitstream_buffer, *buffer);

  vda_loop_proxy_->PostTask(FROM_HERE, base::Bind(
      &VideoDecodeAccelerator::Decode, weak_vda_, bitstream_buffer));

  if (CanMoreDecodeWorkBeDone()) {
    // Force post here to prevent reentrancy into DemuxerStream.
    gvd_loop_proxy_->PostTask(FROM_HERE, base::Bind(
        &GpuVideoDecoder::EnsureDemuxOrDecode, weak_this_));
  }
}

void GpuVideoDecoder::RecordBufferData(
    const BitstreamBuffer& bitstream_buffer, const DecoderBuffer& buffer) {
  input_buffer_data_.push_front(BufferData(
      bitstream_buffer.id(), buffer.GetTimestamp(),
      demuxer_stream_->video_decoder_config().visible_rect(),
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

bool GpuVideoDecoder::HasOutputFrameAvailable() const {
  DCHECK(gvd_loop_proxy_->BelongsToCurrentThread());
  return available_pictures_ > 0;
}

void GpuVideoDecoder::NotifyInitializeDone() {
  NOTREACHED() << "GpuVideoDecodeAcceleratorHost::Initialize is synchronous!";
}

void GpuVideoDecoder::ProvidePictureBuffers(uint32 count,
                                            const gfx::Size& size,
                                            uint32 texture_target) {
  DCHECK(gvd_loop_proxy_->BelongsToCurrentThread());

  std::vector<uint32> texture_ids;
  decoder_texture_target_ = texture_target;
  if (!factories_->CreateTextures(
      count, size, &texture_ids, decoder_texture_target_)) {
    NotifyError(VideoDecodeAccelerator::PLATFORM_FAILURE);
    return;
  }

  if (!vda_)
    return;

  CHECK_EQ(available_pictures_, -1);
  available_pictures_ = count;

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
  DCHECK(gvd_loop_proxy_->BelongsToCurrentThread());

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
  DCHECK(gvd_loop_proxy_->BelongsToCurrentThread());

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
  gfx::Rect visible_rect;
  gfx::Size natural_size;
  GetBufferData(picture.bitstream_buffer_id(), &timestamp, &visible_rect,
                &natural_size);
  DCHECK(decoder_texture_target_);
  scoped_refptr<VideoFrame> frame(
      VideoFrame::WrapNativeTexture(
          pb.texture_id(), decoder_texture_target_, pb.size(), visible_rect,
          natural_size, timestamp,
          base::Bind(&Factories::ReadPixels, factories_, pb.texture_id(),
                     decoder_texture_target_,
                     gfx::Size(visible_rect.width(), visible_rect.height())),
          BindToCurrentLoop(base::Bind(
              &GpuVideoDecoder::ReusePictureBuffer, weak_this_,
              picture.picture_buffer_id()))));
  CHECK_GT(available_pictures_, 0);
  available_pictures_--;

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

  base::ResetAndReturn(&pending_read_cb_).Run(kOk, ready_video_frames_.front());
  ready_video_frames_.pop_front();
}

void GpuVideoDecoder::ReusePictureBuffer(int64 picture_buffer_id) {
  DCHECK(gvd_loop_proxy_->BelongsToCurrentThread());
  CHECK_GE(available_pictures_, 0);
  available_pictures_++;

  if (!vda_)
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
  DCHECK(gvd_loop_proxy_->BelongsToCurrentThread());

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
      CanMoreDecodeWorkBeDone()) {
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

  DestroyTextures();
}

void GpuVideoDecoder::EnsureDemuxOrDecode() {
  DCHECK(gvd_loop_proxy_->BelongsToCurrentThread());

  // The second condition can happen during the tear-down process.
  // GpuVideoDecoder::Stop() returns the |pending_read_cb_| immediately without
  // waiting for the demuxer read to be returned. Therefore, this function could
  // be called even after the decoder has been stopped.
  if (demuxer_read_in_progress_ || !demuxer_stream_)
    return;

  demuxer_read_in_progress_ = true;
  demuxer_stream_->Read(base::Bind(
      &GpuVideoDecoder::RequestBufferDecode, weak_this_));
}

void GpuVideoDecoder::NotifyFlushDone() {
  DCHECK(gvd_loop_proxy_->BelongsToCurrentThread());
  DCHECK_EQ(state_, kDrainingDecoder);
  state_ = kDecoderDrained;
  EnqueueFrameAndTriggerFrameDelivery(VideoFrame::CreateEmptyFrame());
}

void GpuVideoDecoder::NotifyResetDone() {
  DCHECK(gvd_loop_proxy_->BelongsToCurrentThread());
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
  DCHECK(gvd_loop_proxy_->BelongsToCurrentThread());
  if (!vda_)
    return;

  DLOG(ERROR) << "VDA Error: " << error;
  DestroyVDA();

  state_ = kError;

  if (!pending_read_cb_.is_null()) {
    base::ResetAndReturn(&pending_read_cb_).Run(kDecodeError, NULL);
    return;
  }
}

}  // namespace media
