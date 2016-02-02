// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/pipeline/av_pipeline_impl.h"

#include <utility>

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
                               const AvPipelineClient& client)
    : decoder_(decoder),
      client_(client),
      state_(kUninitialized),
      buffered_time_(::media::kNoTimestamp()),
      playable_buffered_time_(::media::kNoTimestamp()),
      enable_feeding_(false),
      pending_read_(false),
      enable_time_update_(false),
      pending_time_update_task_(false),
      media_keys_(NULL),
      media_keys_callback_id_(kNoCallbackId),
      weak_factory_(this) {
  DCHECK(decoder_);
  decoder_->SetDelegate(this);
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

  if (state_ == kFlushing) {
    // Break the feeding loop.
    enable_feeding_ = false;

    // Remove any pending buffer.
    pending_buffer_ = nullptr;
  }
}

void AvPipelineImpl::SetCodedFrameProvider(
    scoped_ptr<CodedFrameProvider> frame_provider,
    size_t max_buffer_size,
    size_t max_frame_size) {
  DCHECK_EQ(state_, kUninitialized);
  DCHECK(frame_provider);

  // Wrap the incoming frame provider to add some buffering capabilities.
  frame_provider_.reset(new BufferingFrameProvider(
      std::move(frame_provider), max_buffer_size, max_frame_size,
      base::Bind(&AvPipelineImpl::OnDataBuffered, weak_this_)));
}

bool AvPipelineImpl::StartPlayingFrom(
    base::TimeDelta time,
    const scoped_refptr<BufferingState>& buffering_state) {
  CMALOG(kLogControl) << __FUNCTION__ << " t0=" << time.InMilliseconds();
  DCHECK(thread_checker_.CalledOnValidThread());

  // Reset the pipeline statistics.
  previous_stats_ = ::media::PipelineStatistics();

  if (state_ == kError) {
    CMALOG(kLogControl) << __FUNCTION__ << " called while in error state";
    return false;
  }
  DCHECK_EQ(state_, kFlushed);

  // Buffering related initialization.
  DCHECK(frame_provider_);
  buffering_state_ = buffering_state;
  if (buffering_state_.get())
    buffering_state_->SetMediaTime(time);

  // Start feeding the pipeline.
  enable_feeding_ = true;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&AvPipelineImpl::FetchBuffer, weak_this_));

  TransitionToState(kPlaying);
  return true;
}

bool AvPipelineImpl::StartFlush() {
  CMALOG(kLogControl) << __FUNCTION__;
  if (state_ == kError)
    return false;
  DCHECK_EQ(state_, kPlaying);
  TransitionToState(kFlushing);
  return true;
}

void AvPipelineImpl::Flush(const ::media::PipelineStatusCB& status_cb) {
  CMALOG(kLogControl) << __FUNCTION__;
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_EQ(state_, kFlushing);

  // Remove any frames left in the frame provider.
  pending_read_ = false;
  buffered_time_ = ::media::kNoTimestamp();
  playable_buffered_time_ = ::media::kNoTimestamp();
  non_playable_frames_.clear();
  frame_provider_->Flush(
      base::Bind(&AvPipelineImpl::OnFlushDone, weak_this_, status_cb));
}

void AvPipelineImpl::BackendStopped() {
  CMALOG(kLogControl) << __FUNCTION__;
  DCHECK(thread_checker_.CalledOnValidThread());

  // Note: returning to idle state aborts any pending frame push.
  pushed_buffer_ = nullptr;
}

void AvPipelineImpl::OnFlushDone(const ::media::PipelineStatusCB& status_cb) {
  CMALOG(kLogControl) << __FUNCTION__;
  DCHECK(thread_checker_.CalledOnValidThread());
  if (state_ == kError) {
    status_cb.Run(::media::PIPELINE_ERROR_ABORT);
    return;
  }
  TransitionToState(kFlushed);
  status_cb.Run(::media::PIPELINE_OK);
}

void AvPipelineImpl::Stop() {
  DCHECK(thread_checker_.CalledOnValidThread());
  CMALOG(kLogControl) << __FUNCTION__;
  // Stop feeding the pipeline.
  enable_feeding_ = false;
  TransitionToState(kStopped);
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

void AvPipelineImpl::FetchBuffer() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!enable_feeding_)
    return;

  DCHECK(!pending_read_ && !pending_buffer_);

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

  if (!enable_feeding_)
    return;

  if (audio_config.IsValidConfig() || video_config.IsValidConfig())
    OnUpdateConfig(buffer->stream_id(), audio_config, video_config);

  pending_buffer_ = buffer;
  ProcessPendingBuffer();
}

void AvPipelineImpl::ProcessPendingBuffer() {
  if (!enable_feeding_)
    return;

  DCHECK(!pushed_buffer_);

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
                          << pending_buffer_->timestamp();
      return;
    }
    decrypt_context = media_keys_->GetDecryptContext(key_id);
    if (!decrypt_context.get()) {
      CMALOG(kLogControl) << "frame(pts=" << pending_buffer_->timestamp()
                          << "): waiting for key id "
                          << base::HexEncode(&key_id[0], key_id.size());
      if (!client_.wait_for_key_cb.is_null())
        client_.wait_for_key_cb.Run();
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
    base::TimeDelta timestamp =
        base::TimeDelta::FromMicroseconds(pending_buffer_->timestamp());
    if (timestamp != ::media::kNoTimestamp())
      buffering_state_->SetMaxRenderingTime(timestamp);
  }

  DCHECK(!pushed_buffer_);
  pushed_buffer_ = pending_buffer_;
  if (decrypt_context && decrypt_context->GetKeySystem() != KEY_SYSTEM_NONE)
    pushed_buffer_->set_decrypt_context(std::move(decrypt_context));
  pending_buffer_ = nullptr;
  MediaPipelineBackend::BufferStatus status =
      decoder_->PushBuffer(pushed_buffer_.get());

  if (status != MediaPipelineBackend::kBufferPending)
    OnPushBufferComplete(status);
}

void AvPipelineImpl::OnPushBufferComplete(BufferStatus status) {
  DCHECK(thread_checker_.CalledOnValidThread());
  pushed_buffer_ = nullptr;
  if (status == MediaPipelineBackend::kBufferFailed) {
    LOG(WARNING) << "AvPipelineImpl: PushFrame failed";
    enable_feeding_ = false;
    state_ = kError;
    return;
  }
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&AvPipelineImpl::FetchBuffer, weak_this_));
}

void AvPipelineImpl::OnEndOfStream() {
  if (!client_.eos_cb.is_null())
    client_.eos_cb.Run();
}

void AvPipelineImpl::OnDecoderError() {
  if (!client_.playback_error_cb.is_null())
    client_.playback_error_cb.Run(::media::PIPELINE_ERROR_COULD_NOT_RENDER);
}

void AvPipelineImpl::OnKeyStatusChanged(const std::string& key_id,
                                        CastKeyStatus key_status,
                                        uint32_t system_code) {
  CMALOG(kLogControl) << __FUNCTION__ << " key_status= " << key_status
                      << " system_code=" << system_code;
  DCHECK(media_keys_);
  media_keys_->SetKeyStatus(key_id, key_status, system_code);
}

void AvPipelineImpl::OnVideoResolutionChanged(const Size& size) {
  // Ignored here; VideoPipelineImpl overrides this method.
}

void AvPipelineImpl::OnCdmStateChanged() {
  DCHECK(thread_checker_.CalledOnValidThread());

  // Update the buffering state if needed.
  if (buffering_state_.get())
    UpdatePlayableFrames();

  // Process the pending buffer in case the CDM now has the frame key id.
  if (pending_buffer_)
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

  if (!buffer->end_of_stream() &&
      (buffered_time_ == ::media::kNoTimestamp() ||
       buffered_time_ <
           base::TimeDelta::FromMicroseconds(buffer->timestamp()))) {
    buffered_time_ = base::TimeDelta::FromMicroseconds(buffer->timestamp());
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
          playable_buffered_time_ < base::TimeDelta::FromMicroseconds(
                                        non_playable_frame->timestamp())) {
        playable_buffered_time_ =
            base::TimeDelta::FromMicroseconds(non_playable_frame->timestamp());
        buffering_state_->SetBufferedTime(playable_buffered_time_);
      }
    }

    // The frame is playable: remove it from the list of non playable frames.
    non_playable_frames_.pop_front();
  }
}

}  // namespace media
}  // namespace chromecast
