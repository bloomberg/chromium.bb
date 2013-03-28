// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/audio_sync_reader.h"

#include <algorithm>

#include "base/metrics/histogram.h"
#include "base/process_util.h"
#include "base/shared_memory.h"
#include "media/audio/audio_buffers_state.h"
#include "media/audio/audio_parameters.h"
#include "media/audio/shared_memory_util.h"

using media::AudioBus;

namespace content {

AudioSyncReader::AudioSyncReader(base::SharedMemory* shared_memory,
                                 const media::AudioParameters& params,
                                 int input_channels)
    : shared_memory_(shared_memory),
      input_channels_(input_channels),
      renderer_callback_count_(0),
      renderer_missed_callback_count_(0) {
  packet_size_ = media::PacketSizeInBytes(shared_memory_->requested_size());
  int input_memory_size = 0;
  int output_memory_size = AudioBus::CalculateMemorySize(params);
  if (input_channels_ > 0) {
    // The input storage is after the output storage.
    int frames = params.frames_per_buffer();
    input_memory_size = AudioBus::CalculateMemorySize(input_channels_, frames);
    char* input_data =
        static_cast<char*>(shared_memory_->memory()) + output_memory_size;
    input_bus_ = AudioBus::WrapMemory(input_channels_, frames, input_data);
  }
  DCHECK_EQ(packet_size_, output_memory_size + input_memory_size);
  output_bus_ = AudioBus::WrapMemory(params, shared_memory->memory());
}

AudioSyncReader::~AudioSyncReader() {
  if (!renderer_callback_count_)
    return;

  // Recording the percentage of deadline misses gives us a rough overview of
  // how many users might be running into audio glitches.
  int percentage_missed =
      100.0 * renderer_missed_callback_count_ / renderer_callback_count_;
  UMA_HISTOGRAM_PERCENTAGE(
      "Media.AudioRendererMissedDeadline", percentage_missed);
}

bool AudioSyncReader::DataReady() {
  return !media::IsUnknownDataSize(shared_memory_, packet_size_);
}

// media::AudioOutputController::SyncReader implementations.
void AudioSyncReader::UpdatePendingBytes(uint32 bytes) {
  if (bytes != static_cast<uint32>(media::kPauseMark)) {
    // Store unknown length of data into buffer, so we later
    // can find out if data became available.
    media::SetUnknownDataSize(shared_memory_, packet_size_);
  }

  if (socket_.get()) {
    socket_->Send(&bytes, sizeof(bytes));
  }
}

int AudioSyncReader::Read(AudioBus* source, AudioBus* dest) {
  ++renderer_callback_count_;
  if (!DataReady())
    ++renderer_missed_callback_count_;

  // Copy optional synchronized live audio input for consumption by renderer
  // process.
  if (source && input_bus_.get()) {
    DCHECK_EQ(source->channels(), input_bus_->channels());
    DCHECK_LE(source->frames(), input_bus_->frames());
    source->CopyTo(input_bus_.get());
  }

  // Retrieve the actual number of bytes available from the shared memory.  If
  // the renderer has not completed rendering this value will be invalid (still
  // the marker stored in UpdatePendingBytes() above) and must be sanitized.
  // TODO(dalecurtis): Technically this is not the exact size.  Due to channel
  // padding for alignment, there may be more data available than this; AudioBus
  // will automatically do the right thing during CopyTo().  Rename this method
  // to GetActualFrameCount().
  uint32 size = media::GetActualDataSizeInBytes(shared_memory_, packet_size_);

  // Compute the actual number of frames read.  It's important to sanitize this
  // value for a couple reasons.  One, it might still be the unknown data size
  // marker.  Two, shared memory comes from a potentially untrusted source.
  int frames =
      size / (sizeof(*output_bus_->channel(0)) * output_bus_->channels());
  if (frames < 0)
    frames = 0;
  else if (frames > output_bus_->frames())
    frames = output_bus_->frames();

  // Copy data from the shared memory into the caller's AudioBus.
  output_bus_->CopyTo(dest);

  // Zero out any unfilled frames in the destination bus.
  dest->ZeroFramesPartial(frames, dest->frames() - frames);

  // Zero out the entire output buffer to avoid stuttering/repeating-buffers
  // in the anomalous case if the renderer is unable to keep up with real-time.
  output_bus_->Zero();

  // Store unknown length of data into buffer, in case renderer does not store
  // the length itself. It also helps in decision if we need to yield.
  media::SetUnknownDataSize(shared_memory_, packet_size_);

  // Return the actual number of frames read.
  return frames;
}

void AudioSyncReader::Close() {
  if (socket_.get()) {
    socket_->Close();
  }
}

bool AudioSyncReader::Init() {
  socket_.reset(new base::CancelableSyncSocket());
  foreign_socket_.reset(new base::CancelableSyncSocket());
  return base::CancelableSyncSocket::CreatePair(socket_.get(),
                                                foreign_socket_.get());
}

#if defined(OS_WIN)
bool AudioSyncReader::PrepareForeignSocketHandle(
    base::ProcessHandle process_handle,
    base::SyncSocket::Handle* foreign_handle) {
  ::DuplicateHandle(GetCurrentProcess(), foreign_socket_->handle(),
                    process_handle, foreign_handle,
                    0, FALSE, DUPLICATE_SAME_ACCESS);
  if (*foreign_handle != 0)
    return true;
  return false;
}
#else
bool AudioSyncReader::PrepareForeignSocketHandle(
    base::ProcessHandle process_handle,
    base::FileDescriptor* foreign_handle) {
  foreign_handle->fd = foreign_socket_->handle();
  foreign_handle->auto_close = false;
  if (foreign_handle->fd != -1)
    return true;
  return false;
}
#endif

}  // namespace content
