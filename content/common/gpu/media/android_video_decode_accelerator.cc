// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu/media/android_video_decode_accelerator.h"

#include <stddef.h>

#include "base/android/build_info.h"
#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/histogram.h"
#include "base/trace_event/trace_event.h"
#include "content/common/gpu/gpu_channel.h"
#include "content/common/gpu/media/android_copying_backing_strategy.h"
#include "content/common/gpu/media/android_deferred_rendering_backing_strategy.h"
#include "gpu/command_buffer/service/gles2_cmd_decoder.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/bitstream_buffer.h"
#include "media/base/limits.h"
#include "media/base/media_switches.h"
#include "media/base/timestamp_constants.h"
#include "media/base/video_decoder_config.h"
#include "media/video/picture.h"
#include "ui/gl/android/scoped_java_surface.h"
#include "ui/gl/android/surface_texture.h"
#include "ui/gl/gl_bindings.h"

#if defined(ENABLE_MOJO_MEDIA_IN_GPU_PROCESS)
#include "media/base/media_keys.h"
#include "media/mojo/services/mojo_cdm_service.h"
#endif

#define POST_ERROR(error_code, error_message)                        \
  do {                                                               \
    DLOG(ERROR) << error_message;                                    \
    PostError(FROM_HERE, media::VideoDecodeAccelerator::error_code); \
  } while (0)

namespace content {

enum { kNumPictureBuffers = media::limits::kMaxVideoFrames + 1 };

// Max number of bitstreams notified to the client with
// NotifyEndOfBitstreamBuffer() before getting output from the bitstream.
enum { kMaxBitstreamsNotifiedInAdvance = 32 };

// MediaCodec is only guaranteed to support baseline, but some devices may
// support others. Advertise support for all H264 profiles and let the
// MediaCodec fail when decoding if it's not actually supported. It's assumed
// that consumers won't have software fallback for H264 on Android anyway.
static const media::VideoCodecProfile kSupportedH264Profiles[] = {
  media::H264PROFILE_BASELINE,
  media::H264PROFILE_MAIN,
  media::H264PROFILE_EXTENDED,
  media::H264PROFILE_HIGH,
  media::H264PROFILE_HIGH10PROFILE,
  media::H264PROFILE_HIGH422PROFILE,
  media::H264PROFILE_HIGH444PREDICTIVEPROFILE,
  media::H264PROFILE_SCALABLEBASELINE,
  media::H264PROFILE_SCALABLEHIGH,
  media::H264PROFILE_STEREOHIGH,
  media::H264PROFILE_MULTIVIEWHIGH
};

// Because MediaCodec is thread-hostile (must be poked on a single thread) and
// has no callback mechanism (b/11990118), we must drive it by polling for
// complete frames (and available input buffers, when the codec is fully
// saturated).  This function defines the polling delay.  The value used is an
// arbitrary choice that trades off CPU utilization (spinning) against latency.
// Mirrors android_video_encode_accelerator.cc:EncodePollDelay().
static inline const base::TimeDelta DecodePollDelay() {
  // An alternative to this polling scheme could be to dedicate a new thread
  // (instead of using the ChildThread) to run the MediaCodec, and make that
  // thread use the timeout-based flavor of MediaCodec's dequeue methods when it
  // believes the codec should complete "soon" (e.g. waiting for an input
  // buffer, or waiting for a picture when it knows enough complete input
  // pictures have been fed to saturate any internal buffering).  This is
  // speculative and it's unclear that this would be a win (nor that there's a
  // reasonably device-agnostic way to fill in the "believes" above).
  return base::TimeDelta::FromMilliseconds(10);
}

static inline const base::TimeDelta NoWaitTimeOut() {
  return base::TimeDelta::FromMicroseconds(0);
}

static inline const base::TimeDelta IdleTimerTimeOut() {
  return base::TimeDelta::FromSeconds(1);
}

// Handle OnFrameAvailable callbacks safely.  Since they occur asynchronously,
// we take care that the AVDA that wants them still exists.  A WeakPtr to
// the AVDA would be preferable, except that OnFrameAvailable callbacks can
// occur off the gpu main thread.  We also can't guarantee when the
// SurfaceTexture will quit sending callbacks to coordinate with the
// destruction of the AVDA, so we have a separate object that the cb can own.
class AndroidVideoDecodeAccelerator::OnFrameAvailableHandler
    : public base::RefCountedThreadSafe<OnFrameAvailableHandler> {
 public:
  // We do not retain ownership of |owner|.  It must remain valid until
  // after ClearOwner() is called.  This will register with
  // |surface_texture| to receive OnFrameAvailable callbacks.
  OnFrameAvailableHandler(
      AndroidVideoDecodeAccelerator* owner,
      const scoped_refptr<gfx::SurfaceTexture>& surface_texture)
      : owner_(owner) {
    // Note that the callback owns a strong ref to us.
    surface_texture->SetFrameAvailableCallbackOnAnyThread(
        base::Bind(&OnFrameAvailableHandler::OnFrameAvailable,
                   scoped_refptr<OnFrameAvailableHandler>(this)));
  }

  // Forget about our owner, which is required before one deletes it.
  // No further callbacks will happen once this completes.
  void ClearOwner() {
    base::AutoLock lock(lock_);
    // No callback can happen until we release the lock.
    owner_ = nullptr;
  }

  // Call back into our owner if it hasn't been deleted.
  void OnFrameAvailable() {
    base::AutoLock auto_lock(lock_);
    // |owner_| can't be deleted while we have the lock.
    if (owner_)
      owner_->OnFrameAvailable();
  }

 private:
  friend class base::RefCountedThreadSafe<OnFrameAvailableHandler>;
  virtual ~OnFrameAvailableHandler() {}

  // Protects changes to owner_.
  base::Lock lock_;

  // AVDA that wants the OnFrameAvailable callback.
  AndroidVideoDecodeAccelerator* owner_;

  DISALLOW_COPY_AND_ASSIGN(OnFrameAvailableHandler);
};

// Time between when we notice an error, and when we actually notify somebody.
// This is to prevent codec errors caused by SurfaceView fullscreen transitions
// from breaking the pipeline, if we're about to be reset anyway.
static inline const base::TimeDelta ErrorPostingDelay() {
  return base::TimeDelta::FromSeconds(2);
}

AndroidVideoDecodeAccelerator::AndroidVideoDecodeAccelerator(
    const base::WeakPtr<gpu::gles2::GLES2Decoder> decoder,
    const base::Callback<bool(void)>& make_context_current)
    : client_(NULL),
      make_context_current_(make_context_current),
      codec_(media::kCodecH264),
      is_encrypted_(false),
      needs_protected_surface_(false),
      state_(NO_ERROR),
      picturebuffers_requested_(false),
      gl_decoder_(decoder),
      cdm_registration_id_(0),
      pending_input_buf_index_(-1),
      error_sequence_token_(0),
      defer_errors_(false),
      weak_this_factory_(this) {
  if (UseDeferredRenderingStrategy())
    strategy_.reset(new AndroidDeferredRenderingBackingStrategy());
  else
    strategy_.reset(new AndroidCopyingBackingStrategy());
}

AndroidVideoDecodeAccelerator::~AndroidVideoDecodeAccelerator() {
  DCHECK(thread_checker_.CalledOnValidThread());

#if defined(ENABLE_MOJO_MEDIA_IN_GPU_PROCESS)
  if (cdm_) {
    DCHECK(cdm_registration_id_);
    static_cast<media::MediaDrmBridge*>(cdm_.get())
        ->UnregisterPlayer(cdm_registration_id_);
  }
#endif  // defined(ENABLE_MOJO_MEDIA_IN_GPU_PROCESS)
}

bool AndroidVideoDecodeAccelerator::Initialize(const Config& config,
                                               Client* client) {
  DCHECK(!media_codec_);
  DCHECK(thread_checker_.CalledOnValidThread());
  TRACE_EVENT0("media", "AVDA::Initialize");

  DVLOG(1) << __FUNCTION__ << ": " << config.AsHumanReadableString();

  DCHECK(client);
  client_ = client;
  codec_ = VideoCodecProfileToVideoCodec(config.profile);
  is_encrypted_ = config.is_encrypted;

  bool profile_supported = codec_ == media::kCodecVP8 ||
                           codec_ == media::kCodecVP9 ||
                           codec_ == media::kCodecH264;

  if (!profile_supported) {
    LOG(ERROR) << "Unsupported profile: " << config.profile;
    return false;
  }

  // Only use MediaCodec for VP8/9 if it's likely backed by hardware
  // or if the stream is encrypted.
  if ((codec_ == media::kCodecVP8 || codec_ == media::kCodecVP9) &&
      !is_encrypted_) {
    if (media::VideoCodecBridge::IsKnownUnaccelerated(
            codec_, media::MEDIA_CODEC_DECODER)) {
      DVLOG(1) << "Initialization failed: "
               << (codec_ == media::kCodecVP8 ? "vp8" : "vp9")
               << " is not hardware accelerated";
      return false;
    }
  }

  if (!make_context_current_.Run()) {
    LOG(ERROR) << "Failed to make this decoder's GL context current.";
    return false;
  }

  if (!gl_decoder_) {
    LOG(ERROR) << "Failed to get gles2 decoder instance.";
    return false;
  }

  strategy_->Initialize(this);

  surface_texture_ = strategy_->CreateSurfaceTexture();
  on_frame_available_handler_ =
      new OnFrameAvailableHandler(this, surface_texture_);

  // For encrypted streams we postpone configuration until MediaCrypto is
  // available.
  if (is_encrypted_)
    return true;

  return ConfigureMediaCodec();
}

void AndroidVideoDecodeAccelerator::SetCdm(int cdm_id) {
  DVLOG(2) << __FUNCTION__ << ": " << cdm_id;

#if defined(ENABLE_MOJO_MEDIA_IN_GPU_PROCESS)
  using media::MediaDrmBridge;

  DCHECK(client_) << "SetCdm() must be called after Initialize().";

  if (cdm_) {
    NOTREACHED() << "We do not support resetting CDM.";
    NotifyCdmAttached(false);
    return;
  }

  cdm_ = media::MojoCdmService::GetCdm(cdm_id);
  DCHECK(cdm_);

  // On Android platform the MediaKeys will be its subclass MediaDrmBridge.
  MediaDrmBridge* drm_bridge = static_cast<MediaDrmBridge*>(cdm_.get());

  // Register CDM callbacks. The callbacks registered will be posted back to
  // this thread via BindToCurrentLoop.

  // Since |this| holds a reference to the |cdm_|, by the time the CDM is
  // destructed, UnregisterPlayer() must have been called and |this| has been
  // destructed as well. So the |cdm_unset_cb| will never have a chance to be
  // called.
  // TODO(xhwang): Remove |cdm_unset_cb| after it's not used on all platforms.
  cdm_registration_id_ =
      drm_bridge->RegisterPlayer(media::BindToCurrentLoop(base::Bind(
                                     &AndroidVideoDecodeAccelerator::OnKeyAdded,
                                     weak_this_factory_.GetWeakPtr())),
                                 base::Bind(&base::DoNothing));

  drm_bridge->SetMediaCryptoReadyCB(media::BindToCurrentLoop(
      base::Bind(&AndroidVideoDecodeAccelerator::OnMediaCryptoReady,
                 weak_this_factory_.GetWeakPtr())));

  // Postpone NotifyCdmAttached() call till we create the MediaCodec after
  // OnMediaCryptoReady().

#else

  NOTIMPLEMENTED();
  NotifyCdmAttached(false);

#endif  // !defined(ENABLE_MOJO_MEDIA_IN_GPU_PROCESS)
}

void AndroidVideoDecodeAccelerator::DoIOTask() {
  DCHECK(thread_checker_.CalledOnValidThread());
  TRACE_EVENT0("media", "AVDA::DoIOTask");
  if (state_ == ERROR) {
    return;
  }

  bool did_work = QueueInput();
  while (DequeueOutput())
    did_work = true;

  ManageTimer(did_work);
}

bool AndroidVideoDecodeAccelerator::QueueInput() {
  DCHECK(thread_checker_.CalledOnValidThread());
  TRACE_EVENT0("media", "AVDA::QueueInput");
  base::AutoReset<bool> auto_reset(&defer_errors_, true);
  if (bitstreams_notified_in_advance_.size() > kMaxBitstreamsNotifiedInAdvance)
    return false;
  if (pending_bitstream_buffers_.empty())
    return false;
  if (state_ == WAITING_FOR_KEY)
    return false;

  int input_buf_index = pending_input_buf_index_;

  // Do not dequeue a new input buffer if we failed with MEDIA_CODEC_NO_KEY.
  // That status does not return this buffer back to the pool of
  // available input buffers. We have to reuse it in QueueSecureInputBuffer().
  if (input_buf_index == -1) {
    media::MediaCodecStatus status =
        media_codec_->DequeueInputBuffer(NoWaitTimeOut(), &input_buf_index);
    switch (status) {
      case media::MEDIA_CODEC_DEQUEUE_INPUT_AGAIN_LATER:
        return false;
      case media::MEDIA_CODEC_ERROR:
        POST_ERROR(PLATFORM_FAILURE, "Failed to DequeueInputBuffer");
        return false;
      case media::MEDIA_CODEC_OK:
        break;
      default:
        NOTREACHED() << "Unknown DequeueInputBuffer status " << status;
        return false;
    }
  }

  DCHECK_NE(input_buf_index, -1);

  base::Time queued_time = pending_bitstream_buffers_.front().second;
  UMA_HISTOGRAM_TIMES("Media.AVDA.InputQueueTime",
                      base::Time::Now() - queued_time);
  media::BitstreamBuffer bitstream_buffer =
      pending_bitstream_buffers_.front().first;

  if (bitstream_buffer.id() == -1) {
    pending_bitstream_buffers_.pop();
    TRACE_COUNTER1("media", "AVDA::PendingBitstreamBufferCount",
                   pending_bitstream_buffers_.size());

    media_codec_->QueueEOS(input_buf_index);
    return true;
  }

  scoped_ptr<base::SharedMemory> shm;

  if (pending_input_buf_index_ != -1) {
    // The buffer is already dequeued from MediaCodec, filled with data and
    // bitstream_buffer.handle() is closed.
    shm.reset(new base::SharedMemory());
  } else {
    shm.reset(new base::SharedMemory(bitstream_buffer.handle(), true));

    if (!shm->Map(bitstream_buffer.size())) {
      POST_ERROR(UNREADABLE_INPUT, "Failed to SharedMemory::Map()");
      return false;
    }
  }

  const base::TimeDelta presentation_timestamp =
      bitstream_buffer.presentation_timestamp();
  DCHECK(presentation_timestamp != media::kNoTimestamp())
      << "Bitstream buffers must have valid presentation timestamps";

  // There may already be a bitstream buffer with this timestamp, e.g., VP9 alt
  // ref frames, but it's OK to overwrite it because we only expect a single
  // output frame to have that timestamp. AVDA clients only use the bitstream
  // buffer id in the returned Pictures to map a bitstream buffer back to a
  // timestamp on their side, so either one of the bitstream buffer ids will
  // result in them finding the right timestamp.
  bitstream_buffers_in_decoder_[presentation_timestamp] = bitstream_buffer.id();

  // Notice that |memory| will be null if we repeatedly enqueue the same buffer,
  // this happens after MEDIA_CODEC_NO_KEY.
  const uint8_t* memory = static_cast<const uint8_t*>(shm->memory());
  const std::string& key_id = bitstream_buffer.key_id();
  const std::string& iv = bitstream_buffer.iv();
  const std::vector<media::SubsampleEntry>& subsamples =
      bitstream_buffer.subsamples();

  media::MediaCodecStatus status;
  if (key_id.empty() || iv.empty()) {
    status = media_codec_->QueueInputBuffer(input_buf_index, memory,
                                            bitstream_buffer.size(),
                                            presentation_timestamp);
  } else {
    status = media_codec_->QueueSecureInputBuffer(
        input_buf_index, memory, bitstream_buffer.size(), key_id, iv,
        subsamples, presentation_timestamp);
  }

  DVLOG(2) << __FUNCTION__
           << ": Queue(Secure)InputBuffer: pts:" << presentation_timestamp
           << " status:" << status;

  if (status == media::MEDIA_CODEC_NO_KEY) {
    // Keep trying to enqueue the same input buffer.
    // The buffer is owned by us (not the MediaCodec) and is filled with data.
    DVLOG(1) << "QueueSecureInputBuffer failed: NO_KEY";
    pending_input_buf_index_ = input_buf_index;
    state_ = WAITING_FOR_KEY;
    return false;
  }

  pending_input_buf_index_ = -1;
  pending_bitstream_buffers_.pop();
  TRACE_COUNTER1("media", "AVDA::PendingBitstreamBufferCount",
                 pending_bitstream_buffers_.size());

  if (status != media::MEDIA_CODEC_OK) {
    POST_ERROR(PLATFORM_FAILURE, "Failed to QueueInputBuffer: " << status);
    return false;
  }

  // We should call NotifyEndOfBitstreamBuffer(), when no more decoded output
  // will be returned from the bitstream buffer. However, MediaCodec API is
  // not enough to guarantee it.
  // So, here, we calls NotifyEndOfBitstreamBuffer() in advance in order to
  // keep getting more bitstreams from the client, and throttle them by using
  // |bitstreams_notified_in_advance_|.
  // TODO(dwkang): check if there is a way to remove this workaround.
  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&AndroidVideoDecodeAccelerator::NotifyEndOfBitstreamBuffer,
                 weak_this_factory_.GetWeakPtr(), bitstream_buffer.id()));
  bitstreams_notified_in_advance_.push_back(bitstream_buffer.id());

  return true;
}

bool AndroidVideoDecodeAccelerator::DequeueOutput() {
  DCHECK(thread_checker_.CalledOnValidThread());
  TRACE_EVENT0("media", "AVDA::DequeueOutput");
  base::AutoReset<bool> auto_reset(&defer_errors_, true);
  if (picturebuffers_requested_ && output_picture_buffers_.empty())
    return false;

  if (!output_picture_buffers_.empty() && free_picture_ids_.empty()) {
    // Don't have any picture buffer to send. Need to wait more.
    return false;
  }

  bool eos = false;
  base::TimeDelta presentation_timestamp;
  int32_t buf_index = 0;
  do {
    size_t offset = 0;
    size_t size = 0;

    TRACE_EVENT_BEGIN0("media", "AVDA::DequeueOutput");
    media::MediaCodecStatus status = media_codec_->DequeueOutputBuffer(
        NoWaitTimeOut(), &buf_index, &offset, &size, &presentation_timestamp,
        &eos, NULL);
    TRACE_EVENT_END2("media", "AVDA::DequeueOutput", "status", status,
                     "presentation_timestamp (ms)",
                     presentation_timestamp.InMilliseconds());

    switch (status) {
      case media::MEDIA_CODEC_ERROR:
        POST_ERROR(PLATFORM_FAILURE, "DequeueOutputBuffer failed.");
        return false;

      case media::MEDIA_CODEC_DEQUEUE_OUTPUT_AGAIN_LATER:
        return false;

      case media::MEDIA_CODEC_OUTPUT_FORMAT_CHANGED: {
        if (!output_picture_buffers_.empty()) {
          // TODO(chcunningham): This will likely dismiss a handful of decoded
          // frames that have not yet been drawn and returned to us for re-use.
          // Consider a more complicated design that would wait for them to be
          // drawn before dismissing.
          DismissPictureBuffers();
        }

        picturebuffers_requested_ = true;
        int32_t width, height;
        media_codec_->GetOutputFormat(&width, &height);
        size_ = gfx::Size(width, height);
        base::MessageLoop::current()->PostTask(
            FROM_HERE,
            base::Bind(&AndroidVideoDecodeAccelerator::RequestPictureBuffers,
                       weak_this_factory_.GetWeakPtr()));
        return false;
      }

      case media::MEDIA_CODEC_OUTPUT_BUFFERS_CHANGED:
        break;

      case media::MEDIA_CODEC_OK:
        DCHECK_GE(buf_index, 0);
        DVLOG(3) << "AVDA::DequeueOutput: pts:" << presentation_timestamp
                 << " buf_index:" << buf_index << " offset:" << offset
                 << " size:" << size << " eos:" << eos;
        break;

      default:
        NOTREACHED();
        break;
    }
  } while (buf_index < 0);

  if (eos) {
    DVLOG(3) << "AVDA::DequeueOutput: Resetting codec state after EOS";
    ResetCodecState();

    base::MessageLoop::current()->PostTask(
        FROM_HERE, base::Bind(&AndroidVideoDecodeAccelerator::NotifyFlushDone,
                              weak_this_factory_.GetWeakPtr()));
    return false;
  }

  // Get the bitstream buffer id from the timestamp.
  auto it = bitstream_buffers_in_decoder_.find(presentation_timestamp);

  if (it != bitstream_buffers_in_decoder_.end()) {
    const int32_t bitstream_buffer_id = it->second;
    bitstream_buffers_in_decoder_.erase(bitstream_buffers_in_decoder_.begin(),
                                        ++it);
    SendDecodedFrameToClient(buf_index, bitstream_buffer_id);

    // Removes ids former or equal than the id from decoder. Note that
    // |bitstreams_notified_in_advance_| does not mean bitstream ids in decoder
    // because of frame reordering issue. We just maintain this roughly and use
    // it for throttling.
    for (auto bitstream_it = bitstreams_notified_in_advance_.begin();
         bitstream_it != bitstreams_notified_in_advance_.end();
         ++bitstream_it) {
      if (*bitstream_it == bitstream_buffer_id) {
        bitstreams_notified_in_advance_.erase(
            bitstreams_notified_in_advance_.begin(), ++bitstream_it);
        break;
      }
    }
  } else {
    // Normally we assume that the decoder makes at most one output frame for
    // each distinct input timestamp. However MediaCodecBridge uses timestamp
    // correction and provides a non-decreasing timestamp sequence, which might
    // result in timestamp duplicates. Discard the frame if we cannot get the
    // corresponding buffer id.
    DVLOG(3) << "AVDA::DequeueOutput: Releasing buffer with unexpected PTS: "
             << presentation_timestamp;
    media_codec_->ReleaseOutputBuffer(buf_index, false);
  }

  // We got a decoded frame, so try for another.
  return true;
}

void AndroidVideoDecodeAccelerator::SendDecodedFrameToClient(
    int32_t codec_buffer_index,
    int32_t bitstream_id) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_NE(bitstream_id, -1);
  DCHECK(!free_picture_ids_.empty());
  TRACE_EVENT0("media", "AVDA::SendDecodedFrameToClient");

  if (!make_context_current_.Run()) {
    POST_ERROR(PLATFORM_FAILURE, "Failed to make the GL context current.");
    return;
  }

  int32_t picture_buffer_id = free_picture_ids_.front();
  free_picture_ids_.pop();
  TRACE_COUNTER1("media", "AVDA::FreePictureIds", free_picture_ids_.size());

  OutputBufferMap::const_iterator i =
      output_picture_buffers_.find(picture_buffer_id);
  if (i == output_picture_buffers_.end()) {
    POST_ERROR(PLATFORM_FAILURE,
               "Can't find PictureBuffer id: " << picture_buffer_id);
    return;
  }

  // Connect the PictureBuffer to the decoded frame, via whatever
  // mechanism the strategy likes.
  strategy_->UseCodecBufferForPictureBuffer(codec_buffer_index, i->second);

  base::MessageLoop::current()->PostTask(
      FROM_HERE, base::Bind(&AndroidVideoDecodeAccelerator::NotifyPictureReady,
                            weak_this_factory_.GetWeakPtr(),
                            media::Picture(picture_buffer_id, bitstream_id,
                                           gfx::Rect(size_), false)));
}

void AndroidVideoDecodeAccelerator::Decode(
    const media::BitstreamBuffer& bitstream_buffer) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (bitstream_buffer.id() != -1 && bitstream_buffer.size() == 0) {
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&AndroidVideoDecodeAccelerator::NotifyEndOfBitstreamBuffer,
                   weak_this_factory_.GetWeakPtr(), bitstream_buffer.id()));
    return;
  }

  pending_bitstream_buffers_.push(
      std::make_pair(bitstream_buffer, base::Time::Now()));
  TRACE_COUNTER1("media", "AVDA::PendingBitstreamBufferCount",
                 pending_bitstream_buffers_.size());

  DoIOTask();
}

void AndroidVideoDecodeAccelerator::RequestPictureBuffers() {
  client_->ProvidePictureBuffers(kNumPictureBuffers, size_,
                                 strategy_->GetTextureTarget());
}

void AndroidVideoDecodeAccelerator::AssignPictureBuffers(
    const std::vector<media::PictureBuffer>& buffers) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(output_picture_buffers_.empty());
  DCHECK(free_picture_ids_.empty());

  if (buffers.size() < kNumPictureBuffers) {
    POST_ERROR(INVALID_ARGUMENT, "Not enough picture buffers assigned.");
    return;
  }

  for (size_t i = 0; i < buffers.size(); ++i) {
    if (buffers[i].size() != size_) {
      POST_ERROR(INVALID_ARGUMENT,
                 "Invalid picture buffer size assigned. Wanted "
                     << size_.ToString() << ", but got "
                     << buffers[i].size().ToString());
      return;
    }
    int32_t id = buffers[i].id();
    output_picture_buffers_.insert(std::make_pair(id, buffers[i]));
    free_picture_ids_.push(id);
    // Since the client might be re-using |picture_buffer_id| values, forget
    // about previously-dismissed IDs now.  See ReusePictureBuffer() comment
    // about "zombies" for why we maintain this set in the first place.
    dismissed_picture_ids_.erase(id);

    strategy_->AssignOnePictureBuffer(buffers[i]);
  }
  TRACE_COUNTER1("media", "AVDA::FreePictureIds", free_picture_ids_.size());

  DoIOTask();
}

void AndroidVideoDecodeAccelerator::ReusePictureBuffer(
    int32_t picture_buffer_id) {
  DCHECK(thread_checker_.CalledOnValidThread());

  // This ReusePictureBuffer() might have been in a pipe somewhere (queued in
  // IPC, or in a PostTask either at the sender or receiver) when we sent a
  // DismissPictureBuffer() for this |picture_buffer_id|.  Account for such
  // potential "zombie" IDs here.
  if (dismissed_picture_ids_.erase(picture_buffer_id))
    return;

  free_picture_ids_.push(picture_buffer_id);
  TRACE_COUNTER1("media", "AVDA::FreePictureIds", free_picture_ids_.size());

  OutputBufferMap::const_iterator i =
      output_picture_buffers_.find(picture_buffer_id);
  if (i == output_picture_buffers_.end()) {
    POST_ERROR(PLATFORM_FAILURE, "Can't find PictureBuffer id "
                                     << picture_buffer_id);
    return;
  }

  strategy_->ReuseOnePictureBuffer(i->second);

  DoIOTask();
}

void AndroidVideoDecodeAccelerator::Flush() {
  DCHECK(thread_checker_.CalledOnValidThread());

  Decode(media::BitstreamBuffer(-1, base::SharedMemoryHandle(), 0));
}

bool AndroidVideoDecodeAccelerator::ConfigureMediaCodec() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(surface_texture_.get());
  TRACE_EVENT0("media", "AVDA::ConfigureMediaCodec");

  gfx::ScopedJavaSurface surface(surface_texture_.get());

  jobject media_crypto = media_crypto_ ? media_crypto_->obj() : nullptr;

  // |needs_protected_surface_| implies encrypted stream.
  DCHECK(!needs_protected_surface_ || media_crypto);

  // Pass a dummy 320x240 canvas size and let the codec signal the real size
  // when it's known from the bitstream.
  media_codec_.reset(media::VideoCodecBridge::CreateDecoder(
      codec_, needs_protected_surface_, gfx::Size(320, 240),
      surface.j_surface().obj(), media_crypto));
  strategy_->CodecChanged(media_codec_.get(), output_picture_buffers_);
  if (!media_codec_) {
    LOG(ERROR) << "Failed to create MediaCodec instance.";
    return false;
  }

  ManageTimer(true);
  return true;
}

void AndroidVideoDecodeAccelerator::ResetCodecState() {
  DCHECK(thread_checker_.CalledOnValidThread());
  bitstream_buffers_in_decoder_.clear();

  // We don't dismiss picture buffers here since we might not get a format
  // changed message to re-request them, such as during a seek.  In that case,
  // we want to reuse the existing buffers.  However, we're about to invalidate
  // all the output buffers, so we must be sure that the strategy no longer
  // refers to them.

  if (pending_input_buf_index_ != -1) {
    // The data for that index exists in the input buffer, but corresponding
    // shm block been deleted. Check that it is safe to flush the coec, i.e.
    // |pending_bitstream_buffers_| is empty.
    // TODO(timav): keep shm block for that buffer and remove this restriction.
    DCHECK(pending_bitstream_buffers_.empty());
    pending_input_buf_index_ = -1;
  }

  if (state_ == WAITING_FOR_KEY)
    state_ = NO_ERROR;

  // We might increment error_sequence_token here to cancel any delayed errors,
  // but right now it's unclear that it's safe to do so.  If we are in an error
  // state because of a codec error, then it would be okay.  Otherwise, it's
  // less obvious that we are exiting the error state.  Since deferred errors
  // are only intended for fullscreen transitions right now, we take the more
  // conservative approach and let the errors post.
  // TODO(liberato): revisit this once we sort out the error state a bit more.

  // When codec is not in error state we can quickly reset (internally calls
  // flush()) for JB-MR2 and beyond. Prior to JB-MR2, flush() had several bugs
  // (b/8125974, b/8347958) so we must stop() and reconfigure MediaCodec. The
  // full reconfigure is much slower and may cause visible freezing if done
  // mid-stream.
  if (state_ == NO_ERROR &&
      base::android::BuildInfo::GetInstance()->sdk_int() >= 18) {
    DVLOG(3) << __FUNCTION__ << " Doing fast MediaCodec reset (flush).";
    media_codec_->Reset();
    // Since we just flushed all the output buffers, make sure that nothing is
    // using them.
    strategy_->CodecChanged(media_codec_.get(), output_picture_buffers_);
  } else {
    DVLOG(3) << __FUNCTION__
             << " Doing slow MediaCodec reset (stop/re-configure).";
    io_timer_.Stop();
    media_codec_->Stop();
    // Changing the codec will also notify the strategy to forget about any
    // output buffers it has currently.
    state_ = NO_ERROR;
    if (!ConfigureMediaCodec())
      POST_ERROR(PLATFORM_FAILURE, "Failed to create MediaCodec.");
  }
}

void AndroidVideoDecodeAccelerator::DismissPictureBuffers() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DVLOG(3) << __FUNCTION__;

  for (const auto& pb : output_picture_buffers_) {
    strategy_->DismissOnePictureBuffer(pb.second);
    client_->DismissPictureBuffer(pb.first);
    dismissed_picture_ids_.insert(pb.first);
  }
  output_picture_buffers_.clear();
  std::queue<int32_t> empty;
  std::swap(free_picture_ids_, empty);
  picturebuffers_requested_ = false;
}

void AndroidVideoDecodeAccelerator::Reset() {
  DCHECK(thread_checker_.CalledOnValidThread());
  TRACE_EVENT0("media", "AVDA::Reset");

  while (!pending_bitstream_buffers_.empty()) {
    int32_t bitstream_buffer_id = pending_bitstream_buffers_.front().first.id();
    pending_bitstream_buffers_.pop();

    if (bitstream_buffer_id != -1) {
      base::MessageLoop::current()->PostTask(
          FROM_HERE,
          base::Bind(&AndroidVideoDecodeAccelerator::NotifyEndOfBitstreamBuffer,
                     weak_this_factory_.GetWeakPtr(), bitstream_buffer_id));
    }
  }
  TRACE_COUNTER1("media", "AVDA::PendingBitstreamBufferCount", 0);
  bitstreams_notified_in_advance_.clear();

  // Any error that is waiting to post can be ignored.
  error_sequence_token_++;

  ResetCodecState();

  base::MessageLoop::current()->PostTask(
      FROM_HERE, base::Bind(&AndroidVideoDecodeAccelerator::NotifyResetDone,
                            weak_this_factory_.GetWeakPtr()));
}

void AndroidVideoDecodeAccelerator::Destroy() {
  DCHECK(thread_checker_.CalledOnValidThread());

  bool have_context = make_context_current_.Run();
  if (!have_context)
    LOG(WARNING) << "Failed make GL context current for Destroy, continuing.";

  strategy_->Cleanup(have_context, output_picture_buffers_);

  // If we have an OnFrameAvailable handler, tell it that we're going away.
  if (on_frame_available_handler_) {
    on_frame_available_handler_->ClearOwner();
    on_frame_available_handler_ = nullptr;
  }

  weak_this_factory_.InvalidateWeakPtrs();
  if (media_codec_) {
    io_timer_.Stop();
    media_codec_->Stop();
  }
  delete this;
}

bool AndroidVideoDecodeAccelerator::CanDecodeOnIOThread() {
  return false;
}

const gfx::Size& AndroidVideoDecodeAccelerator::GetSize() const {
  return size_;
}

const base::ThreadChecker& AndroidVideoDecodeAccelerator::ThreadChecker()
    const {
  return thread_checker_;
}

base::WeakPtr<gpu::gles2::GLES2Decoder>
AndroidVideoDecodeAccelerator::GetGlDecoder() const {
  return gl_decoder_;
}

void AndroidVideoDecodeAccelerator::OnFrameAvailable() {
  // Remember: this may be on any thread.
  DCHECK(strategy_);
  strategy_->OnFrameAvailable();
}

void AndroidVideoDecodeAccelerator::PostError(
    const ::tracked_objects::Location& from_here,
    media::VideoDecodeAccelerator::Error error) {
  base::MessageLoop::current()->PostDelayedTask(
      from_here,
      base::Bind(&AndroidVideoDecodeAccelerator::NotifyError,
                 weak_this_factory_.GetWeakPtr(), error, error_sequence_token_),
      (defer_errors_ ? ErrorPostingDelay() : base::TimeDelta()));
  state_ = ERROR;
}

void AndroidVideoDecodeAccelerator::OnMediaCryptoReady(
    media::MediaDrmBridge::JavaObjectPtr media_crypto,
    bool needs_protected_surface) {
  DVLOG(1) << __FUNCTION__;

  if (!media_crypto) {
    LOG(ERROR) << "MediaCrypto is not available, can't play encrypted stream.";
    NotifyCdmAttached(false);
    return;
  }

  DCHECK(!media_crypto->is_null());

  // We assume this is a part of the initialization process, thus MediaCodec
  // is not created yet.
  DCHECK(!media_codec_);

  media_crypto_ = std::move(media_crypto);
  needs_protected_surface_ = needs_protected_surface;

  // After receiving |media_crypto_| we can configure MediaCodec.
  const bool success = ConfigureMediaCodec();
  NotifyCdmAttached(success);
}

void AndroidVideoDecodeAccelerator::OnKeyAdded() {
  DVLOG(1) << __FUNCTION__;

  if (state_ == WAITING_FOR_KEY)
    state_ = NO_ERROR;

  DoIOTask();
}

void AndroidVideoDecodeAccelerator::NotifyCdmAttached(bool success) {
  client_->NotifyCdmAttached(success);
}

void AndroidVideoDecodeAccelerator::NotifyPictureReady(
    const media::Picture& picture) {
  client_->PictureReady(picture);
}

void AndroidVideoDecodeAccelerator::NotifyEndOfBitstreamBuffer(
    int input_buffer_id) {
  client_->NotifyEndOfBitstreamBuffer(input_buffer_id);
}

void AndroidVideoDecodeAccelerator::NotifyFlushDone() {
  client_->NotifyFlushDone();
}

void AndroidVideoDecodeAccelerator::NotifyResetDone() {
  client_->NotifyResetDone();
}

void AndroidVideoDecodeAccelerator::NotifyError(
    media::VideoDecodeAccelerator::Error error,
    int token) {
  DVLOG(1) << __FUNCTION__ << ": error: " << error << " token: " << token
           << " current: " << error_sequence_token_;
  if (token != error_sequence_token_)
    return;

  client_->NotifyError(error);
}

void AndroidVideoDecodeAccelerator::ManageTimer(bool did_work) {
  bool should_be_running = true;

  base::TimeTicks now = base::TimeTicks::Now();
  if (!did_work) {
    // Make sure that we have done work recently enough, else stop the timer.
    if (now - most_recent_work_ > IdleTimerTimeOut())
      should_be_running = false;
  } else {
    most_recent_work_ = now;
  }

  if (should_be_running && !io_timer_.IsRunning()) {
    io_timer_.Start(FROM_HERE, DecodePollDelay(), this,
                    &AndroidVideoDecodeAccelerator::DoIOTask);
  } else if (!should_be_running && io_timer_.IsRunning()) {
    io_timer_.Stop();
  }
}

// static
bool AndroidVideoDecodeAccelerator::UseDeferredRenderingStrategy() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableUnifiedMediaPipeline);
}

// static
media::VideoDecodeAccelerator::Capabilities
AndroidVideoDecodeAccelerator::GetCapabilities() {
  Capabilities capabilities;
  SupportedProfiles& profiles = capabilities.supported_profiles;

  SupportedProfile profile;

  profile.profile = media::VP8PROFILE_ANY;
  profile.min_resolution.SetSize(0, 0);
  profile.max_resolution.SetSize(1920, 1088);
  profiles.push_back(profile);

  profile.profile = media::VP9PROFILE_ANY;
  profile.min_resolution.SetSize(0, 0);
  profile.max_resolution.SetSize(1920, 1088);
  profiles.push_back(profile);

  for (const auto& supported_profile : kSupportedH264Profiles) {
    SupportedProfile profile;
    profile.profile = supported_profile;
    profile.min_resolution.SetSize(0, 0);
    // Advertise support for 4k and let the MediaCodec fail when decoding if it
    // doesn't support the resolution. It's assumed that consumers won't have
    // software fallback for H264 on Android anyway.
    profile.max_resolution.SetSize(3840, 2160);
    profiles.push_back(profile);
  }

  if (UseDeferredRenderingStrategy()) {
    capabilities.flags = media::VideoDecodeAccelerator::Capabilities::
        NEEDS_ALL_PICTURE_BUFFERS_TO_DECODE;
  }

  return capabilities;
}

}  // namespace content
