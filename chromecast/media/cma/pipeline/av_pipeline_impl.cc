// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/pipeline/av_pipeline_impl.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/thread_task_runner_handle.h"
#include "chromecast/media/base/decrypt_context_impl.h"
#include "chromecast/media/cdm/browser_cdm_cast.h"
#include "chromecast/media/cma/base/buffering_frame_provider.h"
#include "chromecast/media/cma/base/buffering_state.h"
#include "chromecast/media/cma/base/cma_logging.h"
#include "chromecast/media/cma/base/coded_frame_provider.h"
#include "chromecast/media/cma/base/decoder_buffer_base.h"
#include "chromecast/media/cma/pipeline/decrypt_util.h"
#include "chromecast/public/media/cast_decrypt_config.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/decrypt_config.h"
#include "media/base/timestamp_constants.h"

namespace chromecast {
namespace media {

namespace {

const int kNoCallbackId = -1;

}  // namespace

AvPipelineImpl::AvPipelineImpl(MediaPipelineBackend::Decoder* decoder,
                               const UpdateConfigCB& update_config_cb)
    : update_config_cb_(update_config_cb),
      decoder_(decoder),
      state_(kUninitialized),
      buffered_time_(::media::kNoTimestamp()),
      playable_buffered_time_(::media::kNoTimestamp()),
      enable_feeding_(false),
      pending_read_(false),
      pushed_buffer_(nullptr),
      enable_time_update_(false),
      pending_time_update_task_(false),
      media_keys_(NULL),
      media_keys_callback_id_(kNoCallbackId),
      weak_factory_(this) {
  DCHECK(decoder_);
  weak_this_ = weak_factory_.GetWeakPtr();
  thread_checker_.DetachFromThread();
}

AvPipelineImpl::~AvPipelineImpl() {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (media_keys_ && media_keys_callback_id_ != kNoCallbackId)
    media_keys_->UnregisterPlayer(media_keys_callback_id_);
}

void AvPipelineImpl::TransitionToState(State state) {
  DCHECK(thread_checker_.CalledOnValidThread());
  state_ = state;
}

void AvPipelineImpl::SetCodedFrameProvider(
    scoped_ptr<CodedFrameProvider> frame_provider,
    size_t max_buffer_size,
    size_t max_frame_size) {
  DCHECK_EQ(state_, kUninitialized);
  DCHECK(frame_provider);

  // Wrap the incoming frame provider to add some buffering capabilities.
  frame_provider_.reset(new BufferingFrameProvider(
      frame_provider.Pass(),
      max_buffer_size,
      max_frame_size,
      base::Bind(&AvPipelineImpl::OnDataBuffered, weak_this_)));
}

bool AvPipelineImpl::StartPlayingFrom(
    base::TimeDelta time,
    const scoped_refptr<BufferingState>& buffering_state) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_EQ(state_, kFlushed);

  // Buffering related initialization.
  DCHECK(frame_provider_);
  buffering_state_ = buffering_state;
  if (buffering_state_.get())
    buffering_state_->SetMediaTime(time);

  // Start feeding the pipeline.
  enable_feeding_ = true;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&AvPipelineImpl::FetchBufferIfNeeded, weak_this_));

  return true;
}

void AvPipelineImpl::Flush(const base::Closure& done_cb) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_EQ(state_, kFlushing);

  // Note: returning to idle state aborts any pending frame push.
  pushed_buffer_.set_buffer(nullptr);

  // Break the feeding loop.
  enable_feeding_ = false;

  // Remove any pending buffer.
  pending_buffer_ = scoped_refptr<DecoderBufferBase>();

  // Finally, remove any frames left in the frame provider.
  pending_read_ = false;
  buffered_time_ = ::media::kNoTimestamp();
  playable_buffered_time_ = ::media::kNoTimestamp();
  non_playable_frames_.clear();
  frame_provider_->Flush(done_cb);
}

void AvPipelineImpl::Stop() {
  DCHECK(thread_checker_.CalledOnValidThread());

  // Stop can be called from any state.
  if (state_ == kUninitialized || state_ == kStopped)
    return;

  // Stop feeding the pipeline.
  enable_feeding_ = false;
}

void AvPipelineImpl::SetCdm(BrowserCdmCast* media_keys) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(media_keys);

  if (media_keys_ && media_keys_callback_id_ != kNoCallbackId)
    media_keys_->UnregisterPlayer(media_keys_callback_id_);

  media_keys_ = media_keys;
  media_keys_callback_id_ = media_keys_->RegisterPlayer(
      base::Bind(&AvPipelineImpl::OnCdmStateChanged, weak_this_),
      base::Bind(&AvPipelineImpl::OnCdmDestroyed, weak_this_));
}

void AvPipelineImpl::FetchBufferIfNeeded() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!enable_feeding_)
    return;

  if (pending_read_ || pending_buffer_.get())
    return;

  pending_read_ = true;
  frame_provider_->Read(
      base::Bind(&AvPipelineImpl::OnNewFrame, weak_this_));
}

void AvPipelineImpl::OnNewFrame(
    const scoped_refptr<DecoderBufferBase>& buffer,
    const ::media::AudioDecoderConfig& audio_config,
    const ::media::VideoDecoderConfig& video_config) {
  DCHECK(thread_checker_.CalledOnValidThread());
  pending_read_ = false;

  if (audio_config.IsValidConfig() || video_config.IsValidConfig())
    update_config_cb_.Run(buffer->stream_id(), audio_config, video_config);

  pending_buffer_ = buffer;
  ProcessPendingBuffer();

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&AvPipelineImpl::FetchBufferIfNeeded, weak_this_));
}

void AvPipelineImpl::ProcessPendingBuffer() {
  if (!enable_feeding_)
    return;

  // Initiate a read if there isn't already one.
  if (!pending_buffer_.get() && !pending_read_) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::Bind(&AvPipelineImpl::FetchBufferIfNeeded, weak_this_));
    return;
  }

  if (!pending_buffer_.get() || pushed_buffer_.buffer())
    return;

  // Break the feeding loop when the end of stream is reached.
  if (pending_buffer_->end_of_stream()) {
    CMALOG(kLogControl) << __FUNCTION__ << ": EOS reached, stopped feeding";
    enable_feeding_ = false;
  }

  scoped_ptr<DecryptContextImpl> decrypt_context;
  if (!pending_buffer_->end_of_stream() &&
      pending_buffer_->decrypt_config()) {
    // Verify that CDM has the key ID.
    // Should not send the frame if the key ID is not available yet.
    std::string key_id(pending_buffer_->decrypt_config()->key_id());
    if (!media_keys_) {
      CMALOG(kLogControl) << "No CDM for frame: pts="
                          << pending_buffer_->timestamp().InMilliseconds();
      return;
    }
    decrypt_context = media_keys_->GetDecryptContext(key_id);
    if (!decrypt_context.get()) {
      CMALOG(kLogControl) << "frame(pts="
                          << pending_buffer_->timestamp().InMilliseconds()
                          << "): waiting for key id "
                          << base::HexEncode(&key_id[0], key_id.size());
      return;
    }

    // If we do have the clear key, decrypt the pending buffer
    // and reset the decryption context (not needed anymore).
    crypto::SymmetricKey* key = decrypt_context->GetKey();
    if (key != NULL) {
      pending_buffer_ = DecryptDecoderBuffer(pending_buffer_, key);
      decrypt_context.reset();
    }
  }

  if (!pending_buffer_->end_of_stream() && buffering_state_.get()) {
    base::TimeDelta timestamp = pending_buffer_->timestamp();
    if (timestamp != ::media::kNoTimestamp())
      buffering_state_->SetMaxRenderingTime(timestamp);
  }

  DCHECK(!pushed_buffer_.buffer());
  pushed_buffer_.set_buffer(pending_buffer_);
  MediaPipelineBackend::BufferStatus status =
      decoder_->PushBuffer(decrypt_context.release(), &pushed_buffer_);
  pending_buffer_ = scoped_refptr<DecoderBufferBase>();

  if (status != MediaPipelineBackend::kBufferPending)
    OnBufferPushed(status);
}

void AvPipelineImpl::OnBufferPushed(MediaPipelineBackend::BufferStatus status) {
  DCHECK(thread_checker_.CalledOnValidThread());
  pushed_buffer_.set_buffer(nullptr);
  if (status == MediaPipelineBackend::kBufferFailed) {
    LOG(WARNING) << "AvPipelineImpl: PushFrame failed";
    enable_feeding_ = false;
    state_ = kError;
    return;
  }
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&AvPipelineImpl::ProcessPendingBuffer, weak_this_));
}

void AvPipelineImpl::OnCdmStateChanged() {
  DCHECK(thread_checker_.CalledOnValidThread());

  // Update the buffering state if needed.
  if (buffering_state_.get())
    UpdatePlayableFrames();

  // Process the pending buffer in case the CDM now has the frame key id.
  ProcessPendingBuffer();
}

void AvPipelineImpl::OnCdmDestroyed() {
  DCHECK(thread_checker_.CalledOnValidThread());
  media_keys_ = NULL;
}

void AvPipelineImpl::OnDataBuffered(
    const scoped_refptr<DecoderBufferBase>& buffer,
    bool is_at_max_capacity) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (!buffering_state_.get())
    return;

  if (!buffer->end_of_stream() && (buffered_time_ == ::media::kNoTimestamp() ||
                                   buffered_time_ < buffer->timestamp())) {
    buffered_time_ = buffer->timestamp();
  }

  if (is_at_max_capacity)
    buffering_state_->NotifyMaxCapacity(buffered_time_);

  // No need to update the list of playable frames,
  // if we are already blocking on a frame.
  bool update_playable_frames = non_playable_frames_.empty();
  non_playable_frames_.push_back(buffer);
  if (update_playable_frames)
    UpdatePlayableFrames();
}

void AvPipelineImpl::UpdatePlayableFrames() {
  while (!non_playable_frames_.empty()) {
    const scoped_refptr<DecoderBufferBase>& non_playable_frame =
        non_playable_frames_.front();

    if (non_playable_frame->end_of_stream()) {
      buffering_state_->NotifyEos();
    } else {
      const CastDecryptConfig* decrypt_config =
          non_playable_frame->decrypt_config();
      if (decrypt_config &&
          !(media_keys_ &&
            media_keys_->GetDecryptContext(decrypt_config->key_id()).get())) {
        // The frame is still not playable. All the following are thus not
        // playable.
        break;
      }

      if (playable_buffered_time_ == ::media::kNoTimestamp() ||
          playable_buffered_time_ < non_playable_frame->timestamp()) {
        playable_buffered_time_ = non_playable_frame->timestamp();
        buffering_state_->SetBufferedTime(playable_buffered_time_);
      }
    }

    // The frame is playable: remove it from the list of non playable frames.
    non_playable_frames_.pop_front();
  }
}

}  // namespace media
}  // namespace chromecast
