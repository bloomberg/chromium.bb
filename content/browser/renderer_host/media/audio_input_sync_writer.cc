// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/audio_input_sync_writer.h"

#include <algorithm>
#include <utility>

#include "base/format_macros.h"
#include "base/memory/shared_memory.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/stringprintf.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "content/browser/renderer_host/media/media_stream_manager.h"
#include "content/public/browser/browser_thread.h"

using media::AudioBus;
using media::AudioInputBuffer;
using media::AudioInputBufferParameters;

namespace content {

namespace {

// Used to log if any audio glitches have been detected during an audio session.
// Elements in this enum should not be added, deleted or rearranged.
enum AudioGlitchResult {
  AUDIO_CAPTURER_NO_AUDIO_GLITCHES = 0,
  AUDIO_CAPTURER_AUDIO_GLITCHES = 1,
  AUDIO_CAPTURER_AUDIO_GLITCHES_MAX = AUDIO_CAPTURER_AUDIO_GLITCHES
};

}  // namespace

AudioInputSyncWriter::OverflowData::OverflowData(
    double volume,
    bool key_pressed,
    base::TimeTicks capture_time,
    std::unique_ptr<media::AudioBus> audio_bus)
    : volume_(volume),
      key_pressed_(key_pressed),
      capture_time_(capture_time),
      audio_bus_(std::move(audio_bus)) {}
AudioInputSyncWriter::OverflowData::~OverflowData() {}
AudioInputSyncWriter::OverflowData::OverflowData(
    AudioInputSyncWriter::OverflowData&&) = default;
AudioInputSyncWriter::OverflowData& AudioInputSyncWriter::OverflowData::
operator=(AudioInputSyncWriter::OverflowData&& other) = default;

AudioInputSyncWriter::AudioInputSyncWriter(
    std::unique_ptr<base::SharedMemory> shared_memory,
    std::unique_ptr<base::CancelableSyncSocket> socket,
    uint32_t shared_memory_segment_count,
    const media::AudioParameters& params)
    : socket_(std::move(socket)),
      shared_memory_(std::move(shared_memory)),
      shared_memory_segment_size_(
          (CHECK(shared_memory_segment_count > 0),
           shared_memory_->requested_size() / shared_memory_segment_count)),
      creation_time_(base::Time::Now()),
      audio_bus_memory_size_(AudioBus::CalculateMemorySize(params)) {
  // We use CHECKs since this class is used for IPC.
  CHECK(socket_);
  CHECK(shared_memory_);
  CHECK_EQ(shared_memory_segment_size_ * shared_memory_segment_count,
           shared_memory_->requested_size());
  CHECK_EQ(shared_memory_segment_size_,
           audio_bus_memory_size_ + sizeof(AudioInputBufferParameters));
  DVLOG(1) << "shared memory size: " << shared_memory_->requested_size();
  DVLOG(1) << "shared memory segment count: " << shared_memory_segment_count;
  DVLOG(1) << "audio bus memory size: " << audio_bus_memory_size_;

  audio_buses_.resize(shared_memory_segment_count);

  // Create vector of audio buses by wrapping existing blocks of memory.
  uint8_t* ptr = static_cast<uint8_t*>(shared_memory_->memory());
  CHECK(ptr);
  for (auto& bus : audio_buses_) {
    CHECK_EQ(0U, reinterpret_cast<uintptr_t>(ptr) &
                     (AudioBus::kChannelAlignment - 1));
    AudioInputBuffer* buffer = reinterpret_cast<AudioInputBuffer*>(ptr);
    bus = AudioBus::WrapMemory(params, buffer->audio);
    ptr += shared_memory_segment_size_;
  }
}

AudioInputSyncWriter::~AudioInputSyncWriter() {
  // We log the following:
  // - Percentage of data written to fifo (and not to shared memory).
  // - Percentage of data dropped (fifo reached max size or socket buffer full).
  // - Glitch yes/no (at least 1 drop).
  //
  // Subtract 'trailing' counts that will happen if the renderer process was
  // killed or e.g. the page refreshed while the input device was open etc.
  // This trims off the end of both the error and write counts so that we
  // preserve the proportion of counts before the teardown period. We pick
  // the largest trailing count as the time we consider that the trailing errors
  // begun, and subract that from the total write count.
  DCHECK_LE(trailing_write_to_fifo_count_, write_to_fifo_count_);
  DCHECK_LE(trailing_write_to_fifo_count_, write_count_);
  DCHECK_LE(trailing_write_error_count_, write_error_count_);
  DCHECK_LE(trailing_write_error_count_, write_count_);

  write_to_fifo_count_ -= trailing_write_to_fifo_count_;
  write_error_count_ -= trailing_write_error_count_;
  write_count_ -= std::max(trailing_write_to_fifo_count_,
                           trailing_write_error_count_);

  if (write_count_ == 0)
    return;

  UMA_HISTOGRAM_PERCENTAGE(
      "Media.AudioCapturerMissedReadDeadline",
      100.0 * write_to_fifo_count_ / write_count_);

  UMA_HISTOGRAM_PERCENTAGE(
      "Media.AudioCapturerDroppedData",
      100.0 * write_error_count_ / write_count_);

  UMA_HISTOGRAM_ENUMERATION("Media.AudioCapturerAudioGlitches",
                            write_error_count_ == 0 ?
                                AUDIO_CAPTURER_NO_AUDIO_GLITCHES :
                                AUDIO_CAPTURER_AUDIO_GLITCHES,
                            AUDIO_CAPTURER_AUDIO_GLITCHES_MAX + 1);

  std::string log_string = base::StringPrintf(
      "AISW: number of detected audio glitches: %" PRIuS " out of %" PRIuS,
      write_error_count_, write_count_);
  MediaStreamManager::SendMessageToNativeLog(log_string);
  DVLOG(1) << log_string;
}

// static
std::unique_ptr<AudioInputSyncWriter> AudioInputSyncWriter::Create(
    uint32_t shared_memory_segment_count,
    const media::AudioParameters& params,
    base::CancelableSyncSocket* foreign_socket) {
  // Having no shared memory doesn't make sense, so fail creation in that case.
  if (shared_memory_segment_count == 0)
    return nullptr;

  base::CheckedNumeric<uint32_t> requested_memory_size =
      media::ComputeAudioInputBufferSizeChecked(params,
                                                shared_memory_segment_count);

  auto shared_memory = base::MakeUnique<base::SharedMemory>();
  if (!requested_memory_size.IsValid() ||
      !shared_memory->CreateAndMapAnonymous(
          requested_memory_size.ValueOrDie())) {
    return nullptr;
  }

  auto socket = base::MakeUnique<base::CancelableSyncSocket>();
  if (!base::CancelableSyncSocket::CreatePair(socket.get(), foreign_socket)) {
    return nullptr;
  }

  return base::MakeUnique<AudioInputSyncWriter>(
      std::move(shared_memory), std::move(socket), shared_memory_segment_count,
      params);
}

void AudioInputSyncWriter::Write(const AudioBus* data,
                                 double volume,
                                 bool key_pressed,
                                 base::TimeTicks capture_time) {
  TRACE_EVENT0("audio", "AudioInputSyncWriter::Write");
  ++write_count_;
  CheckTimeSinceLastWrite();

  // Check that the renderer side has read data so that we don't overwrite data
  // that hasn't been read yet. The renderer side sends a signal over the socket
  // each time it has read data. Here, we read those verifications before
  // writing. We verify that each buffer index is in sequence.
  size_t number_of_indices_available = socket_->Peek() / sizeof(uint32_t);
  if (number_of_indices_available > 0) {
    auto indices = base::MakeUnique<uint32_t[]>(number_of_indices_available);
    size_t bytes_received = socket_->Receive(
        &indices[0],
        number_of_indices_available * sizeof(indices[0]));
    CHECK_EQ(number_of_indices_available * sizeof(indices[0]), bytes_received);
    for (size_t i = 0; i < number_of_indices_available; ++i) {
      ++next_read_buffer_index_;
      CHECK_EQ(indices[i], next_read_buffer_index_);
      CHECK_GT(number_of_filled_segments_, 0u);
      --number_of_filled_segments_;
    }
  }

  bool write_error = !WriteDataFromFifoToSharedMemory();

  // Write the current data to the shared memory if there is room, otherwise
  // put it in the fifo.
  if (number_of_filled_segments_ < audio_buses_.size()) {
    WriteParametersToCurrentSegment(volume, key_pressed, capture_time);

    // Copy data into shared memory using pre-allocated audio buses.
    data->CopyTo(audio_buses_[current_segment_id_].get());

    if (!SignalDataWrittenAndUpdateCounters())
      write_error = true;

    trailing_write_to_fifo_count_ = 0;
  } else {
    if (!PushDataToFifo(data, volume, key_pressed, capture_time))
      write_error = true;

    ++write_to_fifo_count_;
    ++trailing_write_to_fifo_count_;
  }

  // Increase write error counts if error, or reset the trailing error counter
  // if all write operations went well (no data dropped).
  if (write_error) {
    ++write_error_count_;
    ++trailing_write_error_count_;
    TRACE_EVENT_INSTANT0("audio", "AudioInputSyncWriter write error",
                         TRACE_EVENT_SCOPE_THREAD);
  } else {
    trailing_write_error_count_ = 0;
  }
}

void AudioInputSyncWriter::Close() {
  socket_->Close();
}


void AudioInputSyncWriter::CheckTimeSinceLastWrite() {
#if !defined(OS_ANDROID)
  static const base::TimeDelta kLogDelayThreadhold =
      base::TimeDelta::FromMilliseconds(500);

  std::ostringstream oss;
  if (last_write_time_.is_null()) {
    // This is the first time Write is called.
    base::TimeDelta interval = base::Time::Now() - creation_time_;
    oss << "AISW::Write: audio input data received for the first time: delay "
           "= " << interval.InMilliseconds() << "ms";
  } else {
    base::TimeDelta interval = base::Time::Now() - last_write_time_;
    if (interval > kLogDelayThreadhold) {
      oss << "AISW::Write: audio input data delay unexpectedly long: delay = "
          << interval.InMilliseconds() << "ms";
    }
  }
  const std::string log_message = oss.str();
  if (!log_message.empty()) {
    AddToNativeLog(log_message);
    DVLOG(1) << log_message;
  }

  last_write_time_ = base::Time::Now();
#endif
}

void AudioInputSyncWriter::AddToNativeLog(const std::string& message) {
  MediaStreamManager::SendMessageToNativeLog(message);
}

bool AudioInputSyncWriter::PushDataToFifo(const AudioBus* data,
                                          double volume,
                                          bool key_pressed,
                                          base::TimeTicks capture_time) {
  if (overflow_data_.size() == kMaxOverflowBusesSize) {
    // We use |write_error_count_| for capping number of log messages.
    // |write_error_count_| also includes socket Send() errors, but those should
    // be rare.
    if (write_error_count_ <= 50) {
      const std::string error_message = "AISW: No room in fifo.";
      LOG(WARNING) << error_message;
      AddToNativeLog(error_message);
      if (write_error_count_ == 50) {
        const std::string error_message =
            "AISW: Log cap reached, suppressing further fifo overflow logs.";
        LOG(WARNING) << error_message;
        AddToNativeLog(error_message);
      }
    }
    return false;
  }

  if (overflow_data_.empty()) {
    const std::string message = "AISW: Starting to use fifo.";
    DVLOG(1) << message;
    AddToNativeLog(message);
  }

  // Push data to fifo.
  std::unique_ptr<AudioBus> audio_bus =
      AudioBus::Create(data->channels(), data->frames());
  data->CopyTo(audio_bus.get());
  overflow_data_.emplace_back(volume, key_pressed, capture_time,
                              std::move(audio_bus));
  DCHECK_LE(overflow_data_.size(), static_cast<size_t>(kMaxOverflowBusesSize));

  return true;
}

bool AudioInputSyncWriter::WriteDataFromFifoToSharedMemory() {
  if (overflow_data_.empty())
    return true;

  const size_t segment_count = audio_buses_.size();
  bool write_error = false;
  auto data_it = overflow_data_.begin();

  while (data_it != overflow_data_.end() &&
         number_of_filled_segments_ < segment_count) {
    // Write parameters to shared memory.
    WriteParametersToCurrentSegment(data_it->volume_, data_it->key_pressed_,
                                    data_it->capture_time_);

    // Copy data from the fifo into shared memory using pre-allocated audio
    // buses.
    data_it->audio_bus_->CopyTo(audio_buses_[current_segment_id_].get());

    if (!SignalDataWrittenAndUpdateCounters())
      write_error = true;

    ++data_it;
  }

  // Erase all copied data from fifo.
  overflow_data_.erase(overflow_data_.begin(), data_it);

  if (overflow_data_.empty()) {
    const std::string message = "AISW: Fifo emptied.";
    DVLOG(1) << message;
    AddToNativeLog(message);
  }

  return !write_error;
}

void AudioInputSyncWriter::WriteParametersToCurrentSegment(
    double volume,
    bool key_pressed,
    base::TimeTicks capture_time) {
  uint8_t* ptr = static_cast<uint8_t*>(shared_memory_->memory());
  CHECK_LT(current_segment_id_, audio_buses_.size());
  ptr += current_segment_id_ * shared_memory_segment_size_;
  AudioInputBuffer* buffer = reinterpret_cast<AudioInputBuffer*>(ptr);
  buffer->params.volume = volume;
  buffer->params.size = audio_bus_memory_size_;
  buffer->params.key_pressed = key_pressed;
  buffer->params.capture_time =
      (capture_time - base::TimeTicks()).InMicroseconds();
  buffer->params.id = next_buffer_id_;
}

bool AudioInputSyncWriter::SignalDataWrittenAndUpdateCounters() {
  if (socket_->Send(&current_segment_id_, sizeof(current_segment_id_)) !=
      sizeof(current_segment_id_)) {
    // Ensure we don't log consecutive errors as this can lead to a large
    // amount of logs.
    if (!had_socket_error_) {
      had_socket_error_ = true;
      const std::string error_message = "AISW: No room in socket buffer.";
      PLOG(WARNING) << error_message;
      AddToNativeLog(error_message);
      TRACE_EVENT_INSTANT0("audio",
                           "AudioInputSyncWriter: No room in socket buffer",
                           TRACE_EVENT_SCOPE_THREAD);
    }
    return false;
  } else {
    had_socket_error_ = false;
  }

  if (++current_segment_id_ >= audio_buses_.size())
    current_segment_id_ = 0;
  ++number_of_filled_segments_;
  CHECK_LE(number_of_filled_segments_, audio_buses_.size());
  ++next_buffer_id_;

  return true;
}

}  // namespace content
