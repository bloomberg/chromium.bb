// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/audio_sync_reader.h"

#include <algorithm>

#include "base/process_util.h"
#include "base/shared_memory.h"
#include "base/threading/platform_thread.h"
#include "media/audio/audio_buffers_state.h"
#include "media/audio/audio_parameters.h"
#include "media/audio/shared_memory_util.h"

#if defined(OS_WIN)
const int kMinIntervalBetweenReadCallsInMs = 10;
#endif

AudioSyncReader::AudioSyncReader(base::SharedMemory* shared_memory,
                                 const media::AudioParameters& params)
    : shared_memory_(shared_memory) {
  packet_size_ = media::PacketSizeInBytes(shared_memory_->created_size());
  DCHECK_EQ(packet_size_, media::AudioBus::CalculateMemorySize(params));
  audio_bus_ = media::AudioBus::WrapMemory(params, shared_memory->memory());
}

AudioSyncReader::~AudioSyncReader() {
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

int AudioSyncReader::Read(media::AudioBus* audio_bus) {
#if defined(OS_WIN)
  // HACK: yield if reader is called too often.
  // Problem is lack of synchronization between host and renderer. We cannot be
  // sure if renderer already filled the buffer, and due to all the plugins we
  // cannot change the API, so we yield if previous call was too recent.
  // Optimization: if renderer is "new" one that writes length of data we can
  // stop yielding the moment length is written -- not ideal solution,
  // but better than nothing.
  while (!DataReady() &&
         ((base::Time::Now() - previous_call_time_).InMilliseconds() <
          kMinIntervalBetweenReadCallsInMs)) {
    base::PlatformThread::YieldCurrentThread();
  }
  previous_call_time_ = base::Time::Now();
#endif

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
      size / (sizeof(*audio_bus_->channel(0)) * audio_bus_->channels());
  if (frames < 0)
    frames = 0;
  else if (frames > audio_bus_->frames())
    frames = audio_bus_->frames();

  // Copy data from the shared memory into the caller's AudioBus.
  audio_bus_->CopyTo(audio_bus);

  // Zero out any unfilled frames in the destination bus.
  audio_bus->ZeroFramesPartial(frames, audio_bus->frames() - frames);

  // Zero out the entire buffer.
  memset(shared_memory_->memory(), 0, packet_size_);

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
