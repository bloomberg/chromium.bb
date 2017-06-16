// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/profiling/memlog_sender_pipe_win.h"

#include "base/logging.h"
#include "chrome/common/profiling/memlog_stream.h"

namespace profiling {

MemlogSenderPipe::MemlogSenderPipe(const base::string16& pipe_id)
    : pipe_id_(pipe_id), handle_(INVALID_HANDLE_VALUE) {}

MemlogSenderPipe::~MemlogSenderPipe() {
  if (handle_ != INVALID_HANDLE_VALUE)
    ::CloseHandle(handle_);
}

bool MemlogSenderPipe::Connect() {
  DCHECK(handle_ == INVALID_HANDLE_VALUE);

  base::string16 pipe_name = kWindowsPipePrefix;
  pipe_name.append(pipe_id_);

  const int kMaxWaitMS = 30000;
  ULONGLONG timeout_ticks = ::GetTickCount64() + kMaxWaitMS;
  do {
    if (!::WaitNamedPipe(pipe_name.c_str(), kMaxWaitMS)) {
      // Since it will wait "forever", the only time WaitNamedPipe should fail
      // is if the pipe doesn't exist.
      return false;
    }
    // This is a single-direction pipe so we don't specify GENERIC_READ, but
    // MSDN says we need FILE_READ_ATTRIBUTES.
    handle_ = ::CreateFile(pipe_name.c_str(),
                           FILE_READ_ATTRIBUTES | GENERIC_WRITE, 0,
                           NULL, OPEN_EXISTING, 0, NULL);
    // Need to loop since there is a race condition waiting for a connection to
    // be available and actually connecting to it.
  } while (handle_ == INVALID_HANDLE_VALUE &&
           ::GetTickCount64() < timeout_ticks);

  return handle_ != INVALID_HANDLE_VALUE;
}

bool MemlogSenderPipe::Send(const void* data, size_t sz) {
  // The pipe uses a blocking wait mode (it doesn't specify PIPE_NOWAIT) so
  // WriteFile should do only complete writes.
  //
  // Note: don't use logging here (CHECK, DCHECK) because they will allocate,
  // and this function is called from within a malloc hook.
  DWORD bytes_written = 0;
  if (!::WriteFile(handle_, data, static_cast<DWORD>(sz), &bytes_written, NULL))
    return false;
  return true;
}

}  // namespace profiling
