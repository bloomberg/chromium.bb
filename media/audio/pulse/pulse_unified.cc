// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/pulse/pulse_unified.h"

#include "base/message_loop.h"
#include "base/time.h"
#include "media/audio/audio_manager_base.h"
#include "media/audio/audio_parameters.h"
#include "media/audio/audio_util.h"
#include "media/audio/pulse/pulse_util.h"
#include "media/base/seekable_buffer.h"

namespace media {

using pulse::AutoPulseLock;
using pulse::WaitForOperationCompletion;

static const int kFifoSizeInPackets = 10;

// static, pa_stream_notify_cb
void PulseAudioUnifiedStream::StreamNotifyCallback(pa_stream* s,
                                                   void* user_data) {
  PulseAudioUnifiedStream* stream =
      static_cast<PulseAudioUnifiedStream*>(user_data);

  // Forward unexpected failures to the AudioSourceCallback if available.  All
  // these variables are only modified under pa_threaded_mainloop_lock() so this
  // should be thread safe.
  if (s && stream->source_callback_ &&
      pa_stream_get_state(s) == PA_STREAM_FAILED) {
    stream->source_callback_->OnError(stream);
  }

  pa_threaded_mainloop_signal(stream->pa_mainloop_, 0);
}

// static, used by pa_stream_set_read_callback.
void PulseAudioUnifiedStream::ReadCallback(pa_stream* handle, size_t length,
                                           void* user_data) {
  static_cast<PulseAudioUnifiedStream*>(user_data)->ReadData();
}

PulseAudioUnifiedStream::PulseAudioUnifiedStream(const AudioParameters& params,
                                                 AudioManagerBase* manager)
    : params_(params),
      manager_(manager),
      pa_context_(NULL),
      pa_mainloop_(NULL),
      input_stream_(NULL),
      output_stream_(NULL),
      volume_(1.0f),
      source_callback_(NULL) {
  DCHECK(manager_->GetMessageLoop()->BelongsToCurrentThread());
  CHECK(params_.IsValid());
  input_bus_ = AudioBus::Create(params_);
  output_bus_ = AudioBus::Create(params_);
}

PulseAudioUnifiedStream::~PulseAudioUnifiedStream() {
  // All internal structures should already have been freed in Close(), which
  // calls AudioManagerBase::ReleaseOutputStream() which deletes this object.
  DCHECK(!input_stream_);
  DCHECK(!output_stream_);
  DCHECK(!pa_context_);
  DCHECK(!pa_mainloop_);
}

bool PulseAudioUnifiedStream::Open() {
  DCHECK(manager_->GetMessageLoop()->BelongsToCurrentThread());
  // Prepare the recording buffers for the callbacks.
  fifo_.reset(new media::SeekableBuffer(
      0, kFifoSizeInPackets * params_.GetBytesPerBuffer()));
  input_data_buffer_.reset(new uint8[params_.GetBytesPerBuffer()]);

  if (!pulse::CreateOutputStream(&pa_mainloop_, &pa_context_, &output_stream_,
                                 params_, &StreamNotifyCallback, NULL, this))
    return false;

  // TODO(xians): Add support for non-default device.
  if (!pulse::CreateInputStream(pa_mainloop_, pa_context_, &input_stream_,
                                params_, AudioManagerBase::kDefaultDeviceId,
                                &StreamNotifyCallback, this))
    return false;

  DCHECK(pa_mainloop_);
  DCHECK(pa_context_);
  DCHECK(input_stream_);
  DCHECK(output_stream_);
  return true;
}

void PulseAudioUnifiedStream::Reset() {
  if (!pa_mainloop_) {
    DCHECK(!input_stream_);
    DCHECK(!output_stream_);
    DCHECK(!pa_context_);
    return;
  }

  {
    AutoPulseLock auto_lock(pa_mainloop_);

    // Close the input stream.
    if (input_stream_) {
      // Disable all the callbacks before disconnecting.
      pa_stream_set_state_callback(input_stream_, NULL, NULL);
      pa_stream_flush(input_stream_, NULL, NULL);
      pa_stream_disconnect(input_stream_);

      // Release PulseAudio structures.
      pa_stream_unref(input_stream_);
      input_stream_ = NULL;
    }

    // Close the ouput stream.
    if (output_stream_) {
      // Release PulseAudio output stream structures.
      pa_stream_set_state_callback(output_stream_, NULL, NULL);
      pa_stream_disconnect(output_stream_);
      pa_stream_unref(output_stream_);
      output_stream_ = NULL;
    }

    if (pa_context_) {
      pa_context_disconnect(pa_context_);
      pa_context_set_state_callback(pa_context_, NULL, NULL);
      pa_context_unref(pa_context_);
      pa_context_ = NULL;
    }
  }

  pa_threaded_mainloop_stop(pa_mainloop_);
  pa_threaded_mainloop_free(pa_mainloop_);
  pa_mainloop_ = NULL;
}

void PulseAudioUnifiedStream::Close() {
  DCHECK(manager_->GetMessageLoop()->BelongsToCurrentThread());
  Reset();

  // Signal to the manager that we're closed and can be removed.
  // This should be the last call in the function as it deletes "this".
  manager_->ReleaseOutputStream(this);
}

void PulseAudioUnifiedStream::WriteData(size_t requested_bytes) {
  CHECK_EQ(requested_bytes, static_cast<size_t>(params_.GetBytesPerBuffer()));

  void* buffer = NULL;
  int frames_filled = 0;
  if (source_callback_) {
    CHECK_GE(pa_stream_begin_write(
        output_stream_, &buffer, &requested_bytes), 0);
    uint32 hardware_delay = pulse::GetHardwareLatencyInBytes(
        output_stream_, params_.sample_rate(),
        params_.GetBytesPerFrame());
    source_callback_->WaitTillDataReady();
    fifo_->Read(input_data_buffer_.get(), requested_bytes);
    input_bus_->FromInterleaved(
        input_data_buffer_.get(), params_.frames_per_buffer(), 2);

    frames_filled = source_callback_->OnMoreIOData(
        input_bus_.get(),
        output_bus_.get(),
        AudioBuffersState(0, hardware_delay));
  }

  // Zero the unfilled data so it plays back as silence.
  if (frames_filled < output_bus_->frames()) {
    output_bus_->ZeroFramesPartial(
        frames_filled, output_bus_->frames() - frames_filled);
  }

  // Note: If this ever changes to output raw float the data must be clipped
  // and sanitized since it may come from an untrusted source such as NaCl.
  output_bus_->ToInterleaved(
      output_bus_->frames(), params_.bits_per_sample() / 8, buffer);
  media::AdjustVolume(buffer, requested_bytes, params_.channels(),
                      params_.bits_per_sample() / 8, volume_);

  if (pa_stream_write(output_stream_, buffer, requested_bytes, NULL, 0LL,
                      PA_SEEK_RELATIVE) < 0) {
    if (source_callback_) {
      source_callback_->OnError(this);
    }
  }
}

void PulseAudioUnifiedStream::ReadData() {
  do {
    size_t length = 0;
    const void* data = NULL;
    pa_stream_peek(input_stream_, &data, &length);
    if (!data || length == 0)
      break;

    fifo_->Append(reinterpret_cast<const uint8*>(data), length);

    // Deliver the recording data to the renderer and drive the playout.
    int packet_size = params_.GetBytesPerBuffer();
    while (fifo_->forward_bytes() >= packet_size) {
      WriteData(packet_size);
    }

    // Checks if we still have data.
    pa_stream_drop(input_stream_);
  } while (pa_stream_readable_size(input_stream_) > 0);

  pa_threaded_mainloop_signal(pa_mainloop_, 0);
}

void PulseAudioUnifiedStream::Start(AudioSourceCallback* callback) {
  DCHECK(manager_->GetMessageLoop()->BelongsToCurrentThread());
  CHECK(callback);
  CHECK(input_stream_);
  CHECK(output_stream_);
  AutoPulseLock auto_lock(pa_mainloop_);

  // Ensure the context and stream are ready.
  if (pa_context_get_state(pa_context_) != PA_CONTEXT_READY &&
      pa_stream_get_state(output_stream_) != PA_STREAM_READY &&
      pa_stream_get_state(input_stream_) != PA_STREAM_READY) {
    callback->OnError(this);
    return;
  }

  source_callback_ = callback;

  fifo_->Clear();

  // Uncork (resume) the input stream.
  pa_stream_set_read_callback(input_stream_, &ReadCallback, this);
  pa_stream_readable_size(input_stream_);
  pa_operation* operation = pa_stream_cork(input_stream_, 0, NULL, NULL);
  WaitForOperationCompletion(pa_mainloop_, operation);

  // Uncork (resume) the output stream.
  // We use the recording stream to drive the playback, so we do not need to
  // register the write callback using pa_stream_set_write_callback().
  operation = pa_stream_cork(output_stream_, 0,
                             &pulse::StreamSuccessCallback, pa_mainloop_);
  WaitForOperationCompletion(pa_mainloop_, operation);
}

void PulseAudioUnifiedStream::Stop() {
  DCHECK(manager_->GetMessageLoop()->BelongsToCurrentThread());

  // Cork (pause) the stream.  Waiting for the main loop lock will ensure
  // outstanding callbacks have completed.
  AutoPulseLock auto_lock(pa_mainloop_);

  // Set |source_callback_| to NULL so all FulfillWriteRequest() calls which may
  // occur while waiting on the flush and cork exit immediately.
  source_callback_ = NULL;

  // Set the read callback to NULL before flushing the stream, otherwise it
  // will cause deadlock on the operation.
  pa_stream_set_read_callback(input_stream_, NULL, NULL);
  pa_operation* operation = pa_stream_flush(
      input_stream_, &pulse::StreamSuccessCallback, pa_mainloop_);
  WaitForOperationCompletion(pa_mainloop_, operation);

  operation = pa_stream_cork(input_stream_, 1, &pulse::StreamSuccessCallback,
                             pa_mainloop_);
  WaitForOperationCompletion(pa_mainloop_, operation);

  // Flush the stream prior to cork, doing so after will cause hangs.  Write
  // callbacks are suspended while inside pa_threaded_mainloop_lock() so this
  // is all thread safe.
  operation = pa_stream_flush(
      output_stream_, &pulse::StreamSuccessCallback, pa_mainloop_);
  WaitForOperationCompletion(pa_mainloop_, operation);

  operation = pa_stream_cork(output_stream_, 1, &pulse::StreamSuccessCallback,
                             pa_mainloop_);
  WaitForOperationCompletion(pa_mainloop_, operation);
}

void PulseAudioUnifiedStream::SetVolume(double volume) {
  DCHECK(manager_->GetMessageLoop()->BelongsToCurrentThread());

  volume_ = static_cast<float>(volume);
}

void PulseAudioUnifiedStream::GetVolume(double* volume) {
  DCHECK(manager_->GetMessageLoop()->BelongsToCurrentThread());

  *volume = volume_;
}

}  // namespace media
