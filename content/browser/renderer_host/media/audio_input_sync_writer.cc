// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/audio_input_sync_writer.h"

#include <algorithm>

#include "base/metrics/histogram.h"
#include "base/strings/stringprintf.h"
#include "content/browser/renderer_host/media/media_stream_manager.h"

using media::AudioBus;

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

AudioInputSyncWriter::AudioInputSyncWriter(void* shared_memory,
                                           size_t shared_memory_size,
                                           int shared_memory_segment_count,
                                           const media::AudioParameters& params)
    : shared_memory_(static_cast<uint8*>(shared_memory)),
      shared_memory_segment_count_(shared_memory_segment_count),
      current_segment_id_(0),
      creation_time_(base::Time::Now()),
      audio_bus_memory_size_(AudioBus::CalculateMemorySize(params)),
      next_buffer_id_(0),
      next_read_buffer_index_(0),
      number_of_filled_segments_(0),
      write_count_(0),
      write_error_count_(0),
      trailing_error_count_(0) {
  DCHECK_GT(shared_memory_segment_count, 0);
  DCHECK_EQ(shared_memory_size % shared_memory_segment_count, 0u);
  shared_memory_segment_size_ =
      shared_memory_size / shared_memory_segment_count;
  DVLOG(1) << "shared_memory_size: " << shared_memory_size;
  DVLOG(1) << "shared_memory_segment_count: " << shared_memory_segment_count;
  DVLOG(1) << "audio_bus_memory_size: " << audio_bus_memory_size_;

  // Create vector of audio buses by wrapping existing blocks of memory.
  uint8* ptr = shared_memory_;
  for (int i = 0; i < shared_memory_segment_count; ++i) {
    CHECK_EQ(0U, reinterpret_cast<uintptr_t>(ptr) &
        (media::AudioBus::kChannelAlignment - 1));
    media::AudioInputBuffer* buffer =
        reinterpret_cast<media::AudioInputBuffer*>(ptr);
    scoped_ptr<media::AudioBus> audio_bus =
        media::AudioBus::WrapMemory(params, buffer->audio);
    audio_buses_.push_back(audio_bus.release());
    ptr += shared_memory_segment_size_;
  }
}

AudioInputSyncWriter::~AudioInputSyncWriter() {
  // Subtract 'trailing' errors that will happen if the renderer process was
  // killed or e.g. the page refreshed while the input device was open etc.
  // This trims off the end of both the error and write counts so that we
  // preserve the proportion of errors before the teardown period.
  DCHECK_LE(trailing_error_count_, write_error_count_);
  DCHECK_LE(trailing_error_count_, write_count_);
  write_error_count_ -= trailing_error_count_;
  write_count_ -= trailing_error_count_;

  if (write_count_ == 0)
    return;

  UMA_HISTOGRAM_PERCENTAGE(
      "Media.AudioCapturerMissedReadDeadline",
      100.0 * write_error_count_ / write_count_);

  UMA_HISTOGRAM_ENUMERATION("Media.AudioCapturerAudioGlitches",
                            write_error_count_ == 0 ?
                                AUDIO_CAPTURER_NO_AUDIO_GLITCHES :
                                AUDIO_CAPTURER_AUDIO_GLITCHES,
                            AUDIO_CAPTURER_AUDIO_GLITCHES_MAX + 1);

  std::string log_string = base::StringPrintf(
#if defined(COMPILER_MSVC)
      "AISW: number of detected audio glitches: %Iu out of %Iu",
#else
      "AISW: number of detected audio glitches: %zu out of %zu",
#endif
      write_error_count_,
      write_count_);
  MediaStreamManager::SendMessageToNativeLog(log_string);
  DVLOG(1) << log_string;
}

void AudioInputSyncWriter::Write(const media::AudioBus* data,
                                 double volume,
                                 bool key_pressed,
                                 uint32 hardware_delay_bytes) {
  ++write_count_;

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
  if (!oss.str().empty()) {
    AddToNativeLog(oss.str());
    DVLOG(1) << oss.str();
  }

  last_write_time_ = base::Time::Now();
#endif

  // Check that the renderer side has read data so that we don't overwrite data
  // that hasn't been read yet. The renderer side sends a signal over the socket
  // each time it has read data. Here, we read those verifications before
  // writing. We verify that each buffer index is in sequence.
  size_t number_of_indices_available = socket_->Peek() / sizeof(uint32_t);
  if (number_of_indices_available > 0) {
    scoped_ptr<uint32_t[]> indices(new uint32_t[number_of_indices_available]);
    size_t bytes_received = socket_->Receive(
        &indices[0],
        number_of_indices_available * sizeof(indices[0]));
    DCHECK_EQ(number_of_indices_available * sizeof(indices[0]), bytes_received);
    for (size_t i = 0; i < number_of_indices_available; ++i) {
      ++next_read_buffer_index_;
      CHECK_EQ(indices[i], next_read_buffer_index_);
      --number_of_filled_segments_;
      CHECK_GE(number_of_filled_segments_, 0);
    }
  }

  // Check that the ring buffer isn't full, and if it is log error and drop the
  // buffer.
  if (number_of_filled_segments_ ==
      static_cast<int>(shared_memory_segment_count_)) {
    const std::string error_message =
        "No room in ring buffer to write data to. Dropping the data.";
    LOG(ERROR) << error_message;
    AddToNativeLog(error_message);
    ++write_error_count_;
    ++trailing_error_count_;
    return;
  }

  // Write audio parameters to shared memory.
  uint8* ptr = shared_memory_;
  ptr += current_segment_id_ * shared_memory_segment_size_;
  media::AudioInputBuffer* buffer =
      reinterpret_cast<media::AudioInputBuffer*>(ptr);
  buffer->params.volume = volume;
  buffer->params.size = audio_bus_memory_size_;
  buffer->params.key_pressed = key_pressed;
  buffer->params.hardware_delay_bytes = hardware_delay_bytes;
  buffer->params.id = next_buffer_id_++;

  // Copy data from the native audio layer into shared memory using pre-
  // allocated audio buses.
  media::AudioBus* audio_bus = audio_buses_[current_segment_id_];
  data->CopyTo(audio_bus);

  if (socket_->Send(&current_segment_id_, sizeof(current_segment_id_)) !=
      sizeof(current_segment_id_)) {
    const std::string error_message = "No room in socket buffer.";
    LOG(ERROR) << error_message;
    AddToNativeLog(error_message);
    ++write_error_count_;
    ++trailing_error_count_;
    return;
  }

  // Successfully delivered the buffer. Clear the trailing failure count.
  trailing_error_count_ = 0;

  if (++current_segment_id_ >= shared_memory_segment_count_)
    current_segment_id_ = 0;

  ++number_of_filled_segments_;
  CHECK_LE(number_of_filled_segments_,
           static_cast<int>(shared_memory_segment_count_));
}

void AudioInputSyncWriter::Close() {
  socket_->Close();
}

bool AudioInputSyncWriter::Init() {
  socket_.reset(new base::CancelableSyncSocket());
  foreign_socket_.reset(new base::CancelableSyncSocket());
  return base::CancelableSyncSocket::CreatePair(socket_.get(),
                                                foreign_socket_.get());
}

bool AudioInputSyncWriter::PrepareForeignSocket(
    base::ProcessHandle process_handle,
    base::SyncSocket::TransitDescriptor* descriptor) {
  return foreign_socket_->PrepareTransitDescriptor(process_handle, descriptor);
}

void AudioInputSyncWriter::AddToNativeLog(const std::string& message) {
  MediaStreamManager::SendMessageToNativeLog(message);
}

}  // namespace content
