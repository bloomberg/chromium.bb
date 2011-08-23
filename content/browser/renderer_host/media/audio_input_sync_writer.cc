// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/audio_input_sync_writer.h"

#include <algorithm>

#include "base/process_util.h"
#include "base/shared_memory.h"

AudioInputSyncWriter::AudioInputSyncWriter(base::SharedMemory* shared_memory)
    : shared_memory_(shared_memory) {
}

AudioInputSyncWriter::~AudioInputSyncWriter() {}

void AudioInputSyncWriter::UpdateRecordedBytes(uint32 bytes) {
  socket_->Send(&bytes, sizeof(bytes));
}

uint32 AudioInputSyncWriter::Write(const void* data, uint32 size) {
  uint32 write_size = std::min(size, shared_memory_->created_size());
  // Copy audio input samples from recorded data to shared memory.
  memcpy(shared_memory_->memory(), data, write_size);
  return write_size;
}

void AudioInputSyncWriter::Close() {
  socket_->Close();
}

bool AudioInputSyncWriter::Init() {
  base::SyncSocket* sockets[2] = {0};
  if (base::SyncSocket::CreatePair(sockets)) {
    socket_.reset(sockets[0]);
    foreign_socket_.reset(sockets[1]);
    return true;
  }
  return false;
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
