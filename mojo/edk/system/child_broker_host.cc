// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/edk/system/child_broker_host.h"

#include "base/bind.h"
#include "base/lazy_instance.h"
#include "mojo/edk/embedder/platform_channel_pair.h"
#include "mojo/edk/system/broker_messages.h"
#include "mojo/edk/system/broker_state.h"
#include "mojo/edk/system/configuration.h"

namespace mojo {
namespace edk {

namespace {
static const int kDefaultReadBufferSize = 256;
}

ChildBrokerHost::ChildBrokerHost(base::ProcessHandle child_process,
                                 ScopedPlatformHandle pipe)
    : child_process_(child_process),
      pipe_(pipe.Pass()),
      num_bytes_read_(0) {
#if defined(OS_WIN)
  memset(&read_context_.overlapped, 0, sizeof(read_context_.overlapped));
  read_context_.handler = this;
  memset(&write_context_.overlapped, 0, sizeof(write_context_.overlapped));
  write_context_.handler = this;
#else
  // TODO(jam)
  (void)child_process_; // Suppress -Wunused-private-field.
  (void)num_bytes_read_; // Suppress -Wunused-private-field.
#endif

  read_data_.resize(kDefaultReadBufferSize);
  BrokerState::GetInstance()->broker_thread()->PostTask(
      FROM_HERE,
      base::Bind(&ChildBrokerHost::RegisterIOHandler, base::Unretained(this)));
}

ChildBrokerHost::~ChildBrokerHost() {
}

void ChildBrokerHost::RegisterIOHandler() {
#if defined(OS_WIN)
  base::MessageLoopForIO::current()->RegisterIOHandler(
      pipe_.get().handle, this);
  BeginRead();
#elif defined(OS_POSIX)
  // TOOD(jam): setup
#endif
}

void ChildBrokerHost::BeginRead() {
#if defined(OS_WIN)
  BOOL rv = ReadFile(pipe_.get().handle, &read_data_[num_bytes_read_],
                     static_cast<int>(read_data_.size() - num_bytes_read_),
                     nullptr, &read_context_.overlapped);
  if (rv || GetLastError() == ERROR_IO_PENDING)
    return;

  if (rv == ERROR_BROKEN_PIPE) {
    delete this;
    return;
  }

  NOTREACHED() << "Unknown error in ChildBrokerHost " << rv;
#endif
}

#if defined(OS_WIN)
void ChildBrokerHost::OnIOCompleted(base::MessageLoopForIO::IOContext* context,
                                    DWORD bytes_transferred,
                                    DWORD error) {
  if (context != &read_context_)
    return;

  if (error == ERROR_BROKEN_PIPE) {
    delete this;
    return;  // Child process exited or crashed.
  }

  if (error != ERROR_SUCCESS) {
    NOTREACHED() << "Error " << error << " in ChildBrokerHost.";
    delete this;
    return;
  }

  num_bytes_read_ += bytes_transferred;
  CHECK_GE(num_bytes_read_, sizeof(uint32_t));
  BrokerMessage* message = reinterpret_cast<BrokerMessage*>(&read_data_[0]);
  if (num_bytes_read_ < message->size) {
    read_data_.resize(message->size);
    BeginRead();
    return;
  }

  if (message->id == CREATE_PLATFORM_CHANNEL_PAIR) {
    PlatformChannelPair channel_pair;
    uint32_t response_size = 2 * sizeof(HANDLE);
    write_data_.resize(response_size);
    HANDLE* handles = reinterpret_cast<HANDLE*>(&write_data_[0]);
    handles[0] = DuplicateToChild(
        channel_pair.PassServerHandle().release().handle);
    handles[1] = DuplicateToChild(
        channel_pair.PassClientHandle().release().handle);
  } else if (message->id == HANDLE_TO_TOKEN) {
    uint32_t count =
        (message->size - kBrokerMessageHeaderSize) / sizeof(HANDLE);
    if (count > GetConfiguration().max_message_num_handles) {
      NOTREACHED() << "Too many handles from child process. Closing channel.";
      delete this;
      return;
    }
    uint32_t response_size = count * sizeof(uint64_t);
    write_data_.resize(response_size);
    uint64_t* tokens = reinterpret_cast<uint64_t*>(&write_data_[0]);
    std::vector<PlatformHandle> duplicated_handles(count);
    for (uint32_t i = 0; i < count; ++i) {
      duplicated_handles[i] =
          PlatformHandle(DuplicateFromChild(message->handles[i]));
    }
    BrokerState::GetInstance()->HandleToToken(
        &duplicated_handles[0], count, tokens);
  } else if (message->id == TOKEN_TO_HANDLE) {
    uint32_t count =
        (message->size - kBrokerMessageHeaderSize) / sizeof(uint64_t);
    if (count > GetConfiguration().max_message_num_handles) {
      NOTREACHED() << "Too many tokens from child process. Closing channel.";
      delete this;
      return;
    }
    uint32_t response_size = count * sizeof(HANDLE);
    write_data_.resize(response_size);
    HANDLE* handles = reinterpret_cast<HANDLE*>(&write_data_[0]);
    std::vector<PlatformHandle> temp_handles(count);
    BrokerState::GetInstance()->TokenToHandle(
        &message->tokens[0], count, &temp_handles[0]);
    for (uint32_t i = 0; i < count; ++i) {
      if (temp_handles[i].is_valid()) {
        handles[i] = DuplicateToChild(temp_handles[i].handle);
      } else {
        NOTREACHED() << "Unknown token";
        handles[i] = INVALID_HANDLE_VALUE;
      }
    }
  } else {
    NOTREACHED() << "Unknown command. Stopping reading.";
    delete this;
    return;
  }

  BOOL rv = WriteFile(pipe_.get().handle, &write_data_[0],
                      static_cast<int>(write_data_.size()), NULL,
                      &write_context_.overlapped);
  DCHECK(rv || GetLastError() == ERROR_IO_PENDING);

  // Start reading again.
  num_bytes_read_ = 0;
  BeginRead();
}

HANDLE ChildBrokerHost::DuplicateToChild(HANDLE handle) {
  HANDLE rv = INVALID_HANDLE_VALUE;
  BOOL result = DuplicateHandle(base::GetCurrentProcessHandle(), handle,
                                child_process_, &rv, 0, FALSE,
                                DUPLICATE_SAME_ACCESS | DUPLICATE_CLOSE_SOURCE);
  DCHECK(result);
  return rv;
}

HANDLE ChildBrokerHost::DuplicateFromChild(HANDLE handle) {
  HANDLE rv = INVALID_HANDLE_VALUE;
  BOOL result = DuplicateHandle(child_process_, handle,
                                base::GetCurrentProcessHandle(), &rv, 0, FALSE,
                                DUPLICATE_SAME_ACCESS | DUPLICATE_CLOSE_SOURCE);
  DCHECK(result);
  return rv;
}
#endif

}  // namespace edk
}  // namespace mojo
