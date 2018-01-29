// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/audio_sync_reader.h"

#include <algorithm>
#include <limits>
#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/format_macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/shared_memory.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "media/audio/audio_device_thread.h"
#include "media/base/audio_parameters.h"
#include "media/base/media_switches.h"

namespace {

// Used to log if any audio glitches have been detected during an audio session.
// Elements in this enum should not be added, deleted or rearranged.
enum AudioGlitchResult {
  AUDIO_RENDERER_NO_AUDIO_GLITCHES = 0,
  AUDIO_RENDERER_AUDIO_GLITCHES = 1,
  AUDIO_RENDERER_AUDIO_GLITCHES_MAX = AUDIO_RENDERER_AUDIO_GLITCHES
};

void LogAudioGlitchResult(AudioGlitchResult result) {
  UMA_HISTOGRAM_ENUMERATION("Media.AudioRendererAudioGlitches", result,
                            AUDIO_RENDERER_AUDIO_GLITCHES_MAX + 1);
}

}  // namespace

namespace media {

AudioSyncReader::AudioSyncReader(
    base::RepeatingCallback<void(const std::string&)> log_callback,
    const AudioParameters& params,
    std::unique_ptr<base::SharedMemory> shared_memory,
    std::unique_ptr<base::CancelableSyncSocket> socket)
    : log_callback_(std::move(log_callback)),
      shared_memory_(std::move(shared_memory)),
      mute_audio_(base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kMuteAudio)),
      had_socket_error_(false),
      socket_(std::move(socket)),
      // Validated for reasonable size in Create.
      output_bus_buffer_size_(
          AudioBus::CalculateMemorySize(params.channels(),
                                        params.frames_per_buffer())),
      renderer_callback_count_(0),
      renderer_missed_callback_count_(0),
      trailing_renderer_missed_callback_count_(0),
#if defined(OS_MACOSX) || defined(OS_CHROMEOS)
      maximum_wait_time_(params.GetBufferDuration() / 2),
#else
      // TODO(dalecurtis): Investigate if we can reduce this on all platforms.
      maximum_wait_time_(base::TimeDelta::FromMilliseconds(20)),
#endif
      buffer_index_(0) {
  DCHECK_EQ(base::checked_cast<uint32_t>(shared_memory_->requested_size()),
            ComputeAudioOutputBufferSize(params));
  AudioOutputBuffer* buffer =
      reinterpret_cast<AudioOutputBuffer*>(shared_memory_->memory());
  output_bus_ = AudioBus::WrapMemory(params, buffer->audio);
  output_bus_->Zero();
  output_bus_->set_is_bitstream_format(params.IsBitstreamFormat());
}

AudioSyncReader::~AudioSyncReader() {
  if (!renderer_callback_count_)
    return;

  DVLOG(1) << "Trailing glitch count on destruction: "
           << trailing_renderer_missed_callback_count_;

  // Subtract 'trailing' count of callbacks missed just before the destructor
  // call. This happens if the renderer process was killed or e.g. the page
  // refreshed while the output device was open etc.
  // This trims off the end of both the missed and total counts so that we
  // preserve the proportion of counts before the teardown period.
  DCHECK_LE(trailing_renderer_missed_callback_count_,
            renderer_missed_callback_count_);
  DCHECK_LE(trailing_renderer_missed_callback_count_, renderer_callback_count_);

  renderer_missed_callback_count_ -= trailing_renderer_missed_callback_count_;
  renderer_callback_count_ -= trailing_renderer_missed_callback_count_;

  if (!renderer_callback_count_)
    return;

  // Recording the percentage of deadline misses gives us a rough overview of
  // how many users might be running into audio glitches.
  int percentage_missed =
      100.0 * renderer_missed_callback_count_ / renderer_callback_count_;
  UMA_HISTOGRAM_PERCENTAGE("Media.AudioRendererMissedDeadline",
                           percentage_missed);

  // Add more detailed information regarding detected audio glitches where
  // a non-zero value of |renderer_missed_callback_count_| is added to the
  // AUDIO_RENDERER_AUDIO_GLITCHES bin.
  renderer_missed_callback_count_ > 0
      ? LogAudioGlitchResult(AUDIO_RENDERER_AUDIO_GLITCHES)
      : LogAudioGlitchResult(AUDIO_RENDERER_NO_AUDIO_GLITCHES);
  log_callback_.Run(base::StringPrintf(
      "ASR: number of detected audio glitches: %" PRIuS " out of %" PRIuS,
      renderer_missed_callback_count_, renderer_callback_count_));
}

// static
std::unique_ptr<AudioSyncReader> AudioSyncReader::Create(
    base::RepeatingCallback<void(const std::string&)> log_callback,
    const AudioParameters& params,
    base::CancelableSyncSocket* foreign_socket) {
  base::CheckedNumeric<size_t> memory_size =
      ComputeAudioOutputBufferSizeChecked(params);

  auto shared_memory = std::make_unique<base::SharedMemory>();
  auto socket = std::make_unique<base::CancelableSyncSocket>();

  if (!memory_size.IsValid() ||
      !shared_memory->CreateAndMapAnonymous(memory_size.ValueOrDie()) ||
      !base::CancelableSyncSocket::CreatePair(socket.get(), foreign_socket)) {
    return nullptr;
  }

  return std::make_unique<AudioSyncReader>(std::move(log_callback), params,
                                           std::move(shared_memory),
                                           std::move(socket));
}

// AudioOutputController::SyncReader implementations.
void AudioSyncReader::RequestMoreData(base::TimeDelta delay,
                                      base::TimeTicks delay_timestamp,
                                      int prior_frames_skipped) {
  // We don't send arguments over the socket since sending more than 4
  // bytes might lead to being descheduled. The reading side will zero
  // them when consumed.
  AudioOutputBuffer* buffer =
      reinterpret_cast<AudioOutputBuffer*>(shared_memory_->memory());
  // Increase the number of skipped frames stored in shared memory.
  buffer->params.frames_skipped += prior_frames_skipped;
  buffer->params.delay = delay.InMicroseconds();
  buffer->params.delay_timestamp =
      (delay_timestamp - base::TimeTicks()).InMicroseconds();

  // Zero out the entire output buffer to avoid stuttering/repeating-buffers
  // in the anomalous case if the renderer is unable to keep up with real-time.
  output_bus_->Zero();

  uint32_t control_signal = 0;
  if (delay.is_max()) {
    // std::numeric_limits<uint32_t>::max() is a special signal which is
    // returned after the browser stops the output device in response to a
    // renderer side request.
    control_signal = std::numeric_limits<uint32_t>::max();
  }

  size_t sent_bytes = socket_->Send(&control_signal, sizeof(control_signal));
  if (sent_bytes != sizeof(control_signal)) {
    // Ensure we don't log consecutive errors as this can lead to a large
    // amount of logs.
    if (!had_socket_error_) {
      had_socket_error_ = true;
      static const char* socket_send_failure_message =
          "ASR: No room in socket buffer.";
      PLOG(WARNING) << socket_send_failure_message;
      log_callback_.Run(socket_send_failure_message);
      TRACE_EVENT_INSTANT0("audio", socket_send_failure_message,
                           TRACE_EVENT_SCOPE_THREAD);
    }
  } else {
    had_socket_error_ = false;
  }
  ++buffer_index_;
}

void AudioSyncReader::Read(AudioBus* dest) {
  ++renderer_callback_count_;
  if (!WaitUntilDataIsReady()) {
    ++trailing_renderer_missed_callback_count_;
    ++renderer_missed_callback_count_;
    if (renderer_missed_callback_count_ <= 100 &&
        renderer_missed_callback_count_ % 10 == 0) {
      LOG(WARNING) << "AudioSyncReader::Read timed out, audio glitch count="
                   << renderer_missed_callback_count_;
      if (renderer_missed_callback_count_ == 100)
        LOG(WARNING) << "(log cap reached, suppressing further logs)";
    }
    dest->Zero();
    return;
  }

  trailing_renderer_missed_callback_count_ = 0;

  // Zeroed buffers may be discarded immediately when outputing compressed
  // bitstream.
  if (mute_audio_ && !output_bus_->is_bitstream_format()) {
    dest->Zero();
    return;
  }

  if (output_bus_->is_bitstream_format()) {
    // For bitstream formats, we need the real data size and PCM frame count.
    AudioOutputBuffer* buffer =
        reinterpret_cast<AudioOutputBuffer*>(shared_memory_->memory());
    uint32_t data_size = buffer->params.bitstream_data_size;
    uint32_t bitstream_frames = buffer->params.bitstream_frames;
    // |bitstream_frames| is cast to int below, so it must fit.
    if (data_size > output_bus_buffer_size_ ||
        !base::IsValueInRangeForNumericType<int>(bitstream_frames)) {
      // Received data doesn't fit in the buffer, shouldn't happen.
      dest->Zero();
      return;
    }
    output_bus_->SetBitstreamDataSize(data_size);
    output_bus_->SetBitstreamFrames(bitstream_frames);
  }
  output_bus_->CopyTo(dest);
}

void AudioSyncReader::Close() {
  socket_->Close();
}

bool AudioSyncReader::WaitUntilDataIsReady() {
  TRACE_EVENT0("audio", "AudioSyncReader::WaitUntilDataIsReady");
  base::TimeDelta timeout = maximum_wait_time_;
  const base::TimeTicks start_time = base::TimeTicks::Now();
  const base::TimeTicks finish_time = start_time + timeout;

  // Check if data is ready and if not, wait a reasonable amount of time for it.
  //
  // Data readiness is achieved via parallel counters, one on the renderer side
  // and one here.  Every time a buffer is requested via UpdatePendingBytes(),
  // |buffer_index_| is incremented.  Subsequently every time the renderer has a
  // buffer ready it increments its counter and sends the counter value over the
  // SyncSocket.  Data is ready when |buffer_index_| matches the counter value
  // received from the renderer.
  //
  // The counter values may temporarily become out of sync if the renderer is
  // unable to deliver audio fast enough.  It's assumed that the renderer will
  // catch up at some point, which means discarding counter values read from the
  // SyncSocket which don't match our current buffer index.
  size_t bytes_received = 0;
  uint32_t renderer_buffer_index = 0;
  while (timeout.InMicroseconds() > 0) {
    bytes_received = socket_->ReceiveWithTimeout(
        &renderer_buffer_index, sizeof(renderer_buffer_index), timeout);
    if (bytes_received != sizeof(renderer_buffer_index)) {
      bytes_received = 0;
      break;
    }

    if (renderer_buffer_index == buffer_index_)
      break;

    // Reduce the timeout value as receives succeed, but aren't the right index.
    timeout = finish_time - base::TimeTicks::Now();
  }

  // Receive timed out or another error occurred.  Receive can timeout if the
  // renderer is unable to deliver audio data within the allotted time.
  if (!bytes_received || renderer_buffer_index != buffer_index_) {
    TRACE_EVENT_INSTANT0("audio", "AudioSyncReader::Read timed out",
                         TRACE_EVENT_SCOPE_THREAD);

    base::TimeDelta time_since_start = base::TimeTicks::Now() - start_time;
    UMA_HISTOGRAM_CUSTOM_TIMES("Media.AudioOutputControllerDataNotReady",
                               time_since_start,
                               base::TimeDelta::FromMilliseconds(1),
                               base::TimeDelta::FromMilliseconds(1000), 50);
    return false;
  }

  return true;
}

}  // namespace media
