// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/audio_input_sync_writer.h"

#include <algorithm>

#include "base/process_util.h"
#include "base/shared_memory.h"

namespace content {

AudioInputSyncWriter::AudioInputSyncWriter(base::SharedMemory* shared_memory)
    : shared_memory_(shared_memory) {
}

AudioInputSyncWriter::~AudioInputSyncWriter() {}

// TODO(henrika): Combine into one method (including Write).
void AudioInputSyncWriter::UpdateRecordedBytes(uint32 bytes) {
  socket_->Send(&bytes, sizeof(bytes));
}

uint32 AudioInputSyncWriter::Write(const void* data, uint32 size,
                                   double volume) {
  media::AudioInputBuffer* buffer =
      reinterpret_cast<media::AudioInputBuffer*>(shared_memory_->memory());
  buffer->params.volume = volume;
  buffer->params.size = size;
  memcpy(buffer->audio, data, size);

  return size;
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

#if defined(OS_WIN)

bool AudioInputSyncWriter::PrepareForeignSocketHandle(
    base::ProcessHandle process_handle,
    base::SyncSocket::Handle* foreign_handle) {
  ::DuplicateHandle(GetCurrentProcess(), foreign_socket_->handle(),
                    process_handle, foreign_handle,
                    0, FALSE, DUPLICATE_SAME_ACCESS);
  return (*foreign_handle != 0);
}

#else

bool AudioInputSyncWriter::PrepareForeignSocketHandle(
    base::ProcessHandle process_handle,
    base::FileDescriptor* foreign_handle) {
  foreign_handle->fd = foreign_socket_->handle();
  foreign_handle->auto_close = false;
  return (foreign_handle->fd != -1);
}

#endif

}  // namespace content
