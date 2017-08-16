// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/edk/system/channel.h"

#include <magenta/status.h>
#include <magenta/syscalls.h>
#include <mxio/util.h>
#include <algorithm>
#include <deque>

#include "base/bind.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "base/synchronization/lock.h"
#include "base/task_runner.h"
#include "mojo/edk/embedder/platform_handle_vector.h"

namespace mojo {
namespace edk {

namespace {

const size_t kMaxBatchReadCapacity = 256 * 1024;

uint32_t UnwrapPlatformHandle(PlatformHandle* handle) {
  if (!handle->is_valid_fd())
    return 0;

  // Each MXIO file descriptor is implemented using one or more native resources
  // and can be un-wrapped into a set of |handle| and |info| pairs, with |info|
  // consisting of an MXIO-defined type & arguments (see magenta/processargs.h).
  mx_handle_t handles[3] = {};
  uint32_t info[3] = {};
  mx_status_t result = mxio_transfer_fd(handle->as_fd(), 0, handles, info);
  CHECK_EQ(result, 1) << "mxio_transfer_fd: " << mx_status_get_string(result);
  *handle = PlatformHandle::ForHandle(handles[0]);
  return info[0];
}

// A view over a Channel::Message object. The write queue uses these since
// large messages may need to be sent in chunks.
class MessageView {
 public:
  // Owns |message|. |offset| indexes the first unsent byte in the message.
  MessageView(Channel::MessagePtr message, size_t offset)
      : message_(std::move(message)),
        offset_(offset),
        handles_(message_->TakeHandlesForTransport()) {
    DCHECK_GT(message_->data_num_bytes(), offset_);
  }

  MessageView(MessageView&& other) { *this = std::move(other); }

  MessageView& operator=(MessageView&& other) {
    message_ = std::move(other.message_);
    offset_ = other.offset_;
    handles_ = std::move(other.handles_);
    return *this;
  }

  ~MessageView() {}

  const void* data() const {
    return static_cast<const char*>(message_->data()) + offset_;
  }

  size_t data_num_bytes() const { return message_->data_num_bytes() - offset_; }

  size_t data_offset() const { return offset_; }
  void advance_data_offset(size_t num_bytes) {
    DCHECK_GT(message_->data_num_bytes(), offset_ + num_bytes);
    offset_ += num_bytes;
  }

  ScopedPlatformHandleVectorPtr TakeHandles() {
    if (handles_) {
      // We can only pass Fuchsia handles via IPC, so unwrap any MXIO file-
      // descriptors in |handles_| into the underlying handles, and serialize
      // the metadata, if any, into the extra header.
      auto* handles_info = reinterpret_cast<Channel::Message::HandleInfoEntry*>(
          message_->mutable_extra_header());
      memset(handles_info, 0, message_->extra_header_size());
      for (size_t i = 0; i < handles_->size(); i++) {
        handles_info[i].info = UnwrapPlatformHandle(&(*handles_)[i]);
      }
    }
    return std::move(handles_);
  }

 private:
  Channel::MessagePtr message_;
  size_t offset_;
  ScopedPlatformHandleVectorPtr handles_;

  DISALLOW_COPY_AND_ASSIGN(MessageView);
};

class ChannelFuchsia : public Channel,
                       public base::MessageLoop::DestructionObserver,
                       public base::MessageLoopForIO::MxHandleWatcher {
 public:
  ChannelFuchsia(Delegate* delegate,
                 ConnectionParams connection_params,
                 scoped_refptr<base::TaskRunner> io_task_runner)
      : Channel(delegate),
        self_(this),
        handle_(connection_params.TakeChannelHandle()),
        io_task_runner_(io_task_runner) {
    CHECK(handle_.is_valid());
  }

  void Start() override {
    if (io_task_runner_->RunsTasksInCurrentSequence()) {
      StartOnIOThread();
    } else {
      io_task_runner_->PostTask(
          FROM_HERE, base::Bind(&ChannelFuchsia::StartOnIOThread, this));
    }
  }

  void ShutDownImpl() override {
    // Always shut down asynchronously when called through the public interface.
    io_task_runner_->PostTask(
        FROM_HERE, base::Bind(&ChannelFuchsia::ShutDownOnIOThread, this));
  }

  void Write(MessagePtr message) override {
    bool write_error = false;
    {
      base::AutoLock lock(write_lock_);
      if (reject_writes_)
        return;
      if (!WriteNoLock(MessageView(std::move(message), 0)))
        reject_writes_ = write_error = true;
    }
    if (write_error) {
      // Do not synchronously invoke OnError(). Write() may have been called by
      // the delegate and we don't want to re-enter it.
      io_task_runner_->PostTask(
          FROM_HERE,
          base::Bind(&ChannelFuchsia::OnError, this, Error::kDisconnected));
    }
  }

  void LeakHandle() override {
    DCHECK(io_task_runner_->RunsTasksInCurrentSequence());
    leak_handle_ = true;
  }

  bool GetReadPlatformHandles(size_t num_handles,
                              const void* extra_header,
                              size_t extra_header_size,
                              ScopedPlatformHandleVectorPtr* handles) override {
    DCHECK(io_task_runner_->RunsTasksInCurrentSequence());
    if (num_handles > std::numeric_limits<uint16_t>::max())
      return false;

    // Locate the handle info and verify there is enough of it.
    if (!extra_header)
      return false;
    const auto* handles_info =
        reinterpret_cast<const Channel::Message::HandleInfoEntry*>(
            extra_header);
    size_t handles_info_size = sizeof(handles_info[0]) * num_handles;
    if (handles_info_size > extra_header_size)
      return false;

    // Verify there are as many handles as expected.
    if (incoming_handles_.size() < num_handles)
      return true;

    handles->reset(new PlatformHandleVector(num_handles));
    for (size_t i = 0; i < num_handles; ++i) {
      mx_handle_t handle = incoming_handles_.front().release();
      if (handles_info[i].info) {
        int out_fd = -1;
        mx_status_t result = mxio_create_fd(
            &handle, const_cast<uint32_t*>(&handles_info[i].info), 1, &out_fd);
        if (result != MX_OK)
          return false;
        (*handles)->at(i) = PlatformHandle::ForFd(out_fd);
      } else {
        (*handles)->at(i) = PlatformHandle::ForHandle(handle);
      }
      incoming_handles_.pop_front();
    }

    return true;
  }

 private:
  ~ChannelFuchsia() override {
    DCHECK(!read_watch_);
  }

  void StartOnIOThread() {
    DCHECK(!read_watch_);

    base::MessageLoop::current()->AddDestructionObserver(this);

    read_watch_.reset(
        new base::MessageLoopForIO::MxHandleWatchController(FROM_HERE));
    base::MessageLoopForIO::current()->WatchMxHandle(
        handle_.get().as_handle(), true /* persistent */,
        MX_CHANNEL_READABLE | MX_CHANNEL_PEER_CLOSED, read_watch_.get(), this);
  }

  void ShutDownOnIOThread() {
    base::MessageLoop::current()->RemoveDestructionObserver(this);

    read_watch_.reset();
    if (leak_handle_)
      ignore_result(handle_.release());
    handle_.reset();

    // May destroy the |this| if it was the last reference.
    self_ = nullptr;
  }

  // base::MessageLoop::DestructionObserver:
  void WillDestroyCurrentMessageLoop() override {
    DCHECK(io_task_runner_->RunsTasksInCurrentSequence());
    if (self_)
      ShutDownOnIOThread();
  }

  // base::MessageLoopForIO::MxHandleWatcher:
  void OnMxHandleSignalled(mx_handle_t handle, mx_signals_t signals) override {
    DCHECK(io_task_runner_->RunsTasksInCurrentSequence());
    CHECK_EQ(handle, handle_.get().as_handle());
    DCHECK((MX_CHANNEL_READABLE | MX_CHANNEL_PEER_CLOSED) & signals);

    // We always try to read message(s), even if MX_CHANNEL_PEER_CLOSED, since
    // the peer may have closed while messages were still unread, in the pipe.

    bool validation_error = false;
    bool read_error = false;
    size_t next_read_size = 0;
    size_t buffer_capacity = 0;
    size_t total_bytes_read = 0;
    do {
      buffer_capacity = next_read_size;
      char* buffer = GetReadBuffer(&buffer_capacity);
      DCHECK_GT(buffer_capacity, 0u);

      uint32_t bytes_read = 0;
      uint32_t handles_read = 0;
      mx_handle_t handles[MX_CHANNEL_MAX_MSG_HANDLES] = {};

      mx_status_t read_result = mx_channel_read(
          handle_.get().as_handle(), 0, buffer, handles, buffer_capacity,
          arraysize(handles), &bytes_read, &handles_read);
      if (read_result == MX_OK) {
        for (size_t i = 0; i < handles_read; ++i) {
          incoming_handles_.push_back(base::ScopedMxHandle(handles[i]));
        }
        total_bytes_read += bytes_read;
        if (!OnReadComplete(bytes_read, &next_read_size)) {
          read_error = true;
          validation_error = true;
          break;
        }
      } else if (read_result == MX_ERR_BUFFER_TOO_SMALL) {
        DCHECK_LE(handles_read, arraysize(handles));
        next_read_size = bytes_read;
      } else if (read_result == MX_ERR_SHOULD_WAIT) {
        break;
      } else {
        DLOG_IF(ERROR, read_result != MX_ERR_PEER_CLOSED)
            << "mx_channel_read: " << mx_status_get_string(read_result);
        read_error = true;
        break;
      }
    } while (total_bytes_read < kMaxBatchReadCapacity && next_read_size > 0);
    if (read_error) {
      // Stop receiving read notifications.
      read_watch_.reset();
      if (validation_error)
        OnError(Error::kReceivedMalformedData);
      else
        OnError(Error::kDisconnected);
    }
  }

  // Attempts to write a message directly to the channel. If the full message
  // cannot be written, it's queued and a wait is initiated to write the message
  // ASAP on the I/O thread.
  bool WriteNoLock(MessageView message_view) {
    uint32_t write_bytes = 0;
    do {
      message_view.advance_data_offset(write_bytes);

      ScopedPlatformHandleVectorPtr outgoing_handles =
          message_view.TakeHandles();
      size_t handles_count = 0;
      mx_handle_t handles[MX_CHANNEL_MAX_MSG_HANDLES] = {};
      if (outgoing_handles) {
        DCHECK_LE(outgoing_handles->size(), arraysize(handles));

        handles_count = outgoing_handles->size();
        for (size_t i = 0; i < handles_count; ++i) {
          DCHECK(outgoing_handles->data()[i].is_valid_handle());
          handles[i] = outgoing_handles->data()[i].as_handle();
        }
      }

      write_bytes = std::min(message_view.data_num_bytes(),
                             static_cast<size_t>(MX_CHANNEL_MAX_MSG_BYTES));
      mx_status_t result =
          mx_channel_write(handle_.get().as_handle(), 0, message_view.data(),
                           write_bytes, handles, handles_count);
      if (result != MX_OK) {
        // TODO(fuchsia): Handle MX_ERR_SHOULD_WAIT flow-control errors, once
        // the platform starts generating them. See crbug.com/754084.
        DLOG_IF(ERROR, result != MX_ERR_PEER_CLOSED)
            << "WriteNoLock(mx_channel_write): "
            << mx_status_get_string(result);
        return false;
      }

      if (outgoing_handles) {
        // |handles| have been transferred to the peer process, so clear them to
        // avoid them being double-closed.
        for (auto& outgoing_handle : *outgoing_handles) {
          outgoing_handle = PlatformHandle();
        }
      }
    } while (write_bytes < message_view.data_num_bytes());

    return true;
  }

  // Keeps the Channel alive at least until explicit shutdown on the IO thread.
  scoped_refptr<Channel> self_;

  ScopedPlatformHandle handle_;
  scoped_refptr<base::TaskRunner> io_task_runner_;

  // These members are only used on the IO thread.
  std::unique_ptr<base::MessageLoopForIO::MxHandleWatchController> read_watch_;
  std::deque<base::ScopedMxHandle> incoming_handles_;
  bool leak_handle_ = false;

  base::Lock write_lock_;
  bool reject_writes_ = false;

  DISALLOW_COPY_AND_ASSIGN(ChannelFuchsia);
};

}  // namespace

// static
scoped_refptr<Channel> Channel::Create(
    Delegate* delegate,
    ConnectionParams connection_params,
    scoped_refptr<base::TaskRunner> io_task_runner) {
  return new ChannelFuchsia(delegate, std::move(connection_params),
                            std::move(io_task_runner));
}

}  // namespace edk
}  // namespace mojo
