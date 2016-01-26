// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/edk/system/channel.h"

#include <errno.h>
#include <sys/uio.h>

#include <algorithm>
#include <deque>

#include "base/bind.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/synchronization/lock.h"
#include "base/task_runner.h"
#include "mojo/edk/embedder/platform_channel_utils_posix.h"
#include "mojo/edk/embedder/platform_handle_vector.h"

namespace mojo {
namespace edk {

namespace {

const size_t kMaxBatchReadCapacity = 256 * 1024;

// A view over a Channel::Message object. The write queue uses these since
// large messages may need to be sent in chunks.
class MessageView {
 public:
  // Owns |message|. |offset| indexes the first unsent byte in the message.
  MessageView(Channel::MessagePtr message, size_t offset)
      : message_(std::move(message)),
        offset_(offset),
        handles_(message_->TakeHandles()) {
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

  ScopedPlatformHandleVectorPtr TakeHandles() { return std::move(handles_); }
  Channel::MessagePtr TakeMessage() { return std::move(message_); }

 private:
  Channel::MessagePtr message_;
  size_t offset_;
  ScopedPlatformHandleVectorPtr handles_;

  DISALLOW_COPY_AND_ASSIGN(MessageView);
};

class ChannelPosix : public Channel,
                     public base::MessageLoop::DestructionObserver,
                     public base::MessageLoopForIO::Watcher {
 public:
  ChannelPosix(Delegate* delegate,
               ScopedPlatformHandle handle,
               scoped_refptr<base::TaskRunner> io_task_runner)
      : Channel(delegate),
        self_(this),
        handle_(std::move(handle)),
        io_task_runner_(io_task_runner) {
  }

  void Start() override {
    if (io_task_runner_->RunsTasksOnCurrentThread()) {
      StartOnIOThread();
    } else {
      io_task_runner_->PostTask(
          FROM_HERE, base::Bind(&ChannelPosix::StartOnIOThread, this));
    }
  }

  void ShutDownImpl() override {
    // Always shut down asynchronously when called through the public interface.
    io_task_runner_->PostTask(
        FROM_HERE, base::Bind(&ChannelPosix::ShutDownOnIOThread, this));
  }

  void Write(MessagePtr message) override {
    bool write_error = false;
    {
      base::AutoLock lock(write_lock_);
      if (reject_writes_)
        return;
      if (outgoing_messages_.empty()) {
        if (!WriteNoLock(MessageView(std::move(message), 0)))
          reject_writes_ = write_error = true;
      } else {
        outgoing_messages_.emplace_back(std::move(message), 0);
      }
    }
    if (write_error) {
      // Do not synchronously invoke OnError(). Write() may have been called by
      // the delegate and we don't want to re-enter it.
      io_task_runner_->PostTask(FROM_HERE,
                                base::Bind(&ChannelPosix::OnError, this));
    }
  }

  ScopedPlatformHandleVectorPtr GetReadPlatformHandles(
      size_t num_handles,
      void** payload,
      size_t* payload_size) override {
    if (incoming_platform_handles_.size() < num_handles)
      return nullptr;
    ScopedPlatformHandleVectorPtr handles(
        new PlatformHandleVector(num_handles));
    for (size_t i = 0; i < num_handles; ++i) {
      (*handles)[i] = incoming_platform_handles_.front();
      incoming_platform_handles_.pop_front();
    }
    return handles;
  }

 private:
  ~ChannelPosix() override {
    DCHECK(!read_watcher_);
    DCHECK(!write_watcher_);
    for (auto handle : incoming_platform_handles_)
      handle.CloseIfNecessary();
  }

  void StartOnIOThread() {
    DCHECK(!read_watcher_);
    DCHECK(!write_watcher_);
    read_watcher_.reset(new base::MessageLoopForIO::FileDescriptorWatcher);
    write_watcher_.reset(new base::MessageLoopForIO::FileDescriptorWatcher);
    base::MessageLoopForIO::current()->WatchFileDescriptor(
        handle_.get().handle, true /* persistent */,
        base::MessageLoopForIO::WATCH_READ, read_watcher_.get(), this);
    base::MessageLoop::current()->AddDestructionObserver(this);
  }

  void WaitForWriteOnIOThread() {
    base::AutoLock lock(write_lock_);
    WaitForWriteOnIOThreadNoLock();
  }

  void WaitForWriteOnIOThreadNoLock() {
    if (pending_write_)
      return;
    if (!write_watcher_)
      return;
    if (io_task_runner_->RunsTasksOnCurrentThread()) {
      pending_write_ = true;
      base::MessageLoopForIO::current()->WatchFileDescriptor(
          handle_.get().handle, false /* persistent */,
          base::MessageLoopForIO::WATCH_WRITE, write_watcher_.get(), this);
    } else {
      io_task_runner_->PostTask(
          FROM_HERE, base::Bind(&ChannelPosix::WaitForWriteOnIOThread, this));
    }
  }

  void ShutDownOnIOThread() {
    base::MessageLoop::current()->RemoveDestructionObserver(this);

    read_watcher_.reset();
    write_watcher_.reset();
    handle_.reset();

    // May destroy the |this| if it was the last reference.
    self_ = nullptr;
  }

  // base::MessageLoop::DestructionObserver:
  void WillDestroyCurrentMessageLoop() override {
    DCHECK(io_task_runner_->RunsTasksOnCurrentThread());
    if (self_)
      ShutDownOnIOThread();
  }

  // base::MessageLoopForIO::Watcher:
  void OnFileCanReadWithoutBlocking(int fd) override {
    CHECK_EQ(fd, handle_.get().handle);

    bool read_error = false;
    size_t next_read_size = 0;
    size_t buffer_capacity = 0;
    size_t total_bytes_read = 0;
    size_t bytes_read = 0;
    do {
      buffer_capacity = next_read_size;
      char* buffer = GetReadBuffer(&buffer_capacity);
      DCHECK_GT(buffer_capacity, 0u);

      ssize_t read_result = PlatformChannelRecvmsg(
          handle_.get(),
          buffer,
          buffer_capacity,
          &incoming_platform_handles_);

      if (read_result > 0) {
        bytes_read = static_cast<size_t>(read_result);
        total_bytes_read += bytes_read;
        if (!OnReadComplete(bytes_read, &next_read_size)) {
          read_error = true;
          break;
        }
      } else if (read_result == 0 ||
                 (errno != EAGAIN && errno != EWOULDBLOCK)) {
        read_error = true;
        break;
      }
    } while (bytes_read == buffer_capacity &&
             total_bytes_read < kMaxBatchReadCapacity &&
             next_read_size > 0);
    if (read_error) {
      // Stop receiving read notifications.
      read_watcher_.reset();

      OnError();
    }
  }

  void OnFileCanWriteWithoutBlocking(int fd) override {
    bool write_error = false;
    {
      base::AutoLock lock(write_lock_);
      pending_write_ = false;
      if (!FlushOutgoingMessagesNoLock())
        reject_writes_ = write_error = true;
    }
    if (write_error)
      OnError();
  }

  // Attempts to write a message directly to the channel. If the full message
  // cannot be written, it's queued and a wait is initiated to write the message
  // ASAP on the I/O thread.
  bool WriteNoLock(MessageView message_view) {
    size_t bytes_written = 0;
    do {
      message_view.advance_data_offset(bytes_written);

      ssize_t result;
      ScopedPlatformHandleVectorPtr handles = message_view.TakeHandles();
      if (handles && handles->size()) {
        iovec iov = {
          const_cast<void*>(message_view.data()),
          message_view.data_num_bytes()
        };
        // TODO: Handle lots of handles.
        result = PlatformChannelSendmsgWithHandles(
            handle_.get(), &iov, 1, handles->data(), handles->size());
        handles->clear();
      } else {
        result = PlatformChannelWrite(handle_.get(), message_view.data(),
                                      message_view.data_num_bytes());
      }

      if (result < 0) {
        if (errno != EAGAIN && errno != EWOULDBLOCK)
          return false;
        outgoing_messages_.emplace_back(std::move(message_view));
        WaitForWriteOnIOThreadNoLock();
        return true;
      }

      bytes_written = static_cast<size_t>(result);
    } while (bytes_written < message_view.data_num_bytes());

    return true;
  }

  bool FlushOutgoingMessagesNoLock() {
    std::deque<MessageView> messages;
    std::swap(outgoing_messages_, messages);

    while (!messages.empty()) {
      if (!WriteNoLock(std::move(messages.front())))
        return false;

      messages.pop_front();
      if (!outgoing_messages_.empty()) {
        // The message was requeued by WriteNoLock(), so we have to wait for
        // pipe to become writable again. Repopulate the message queue and exit.
        DCHECK_EQ(outgoing_messages_.size(), 1u);
        MessageView message_view = std::move(outgoing_messages_.front());
        std::swap(messages, outgoing_messages_);
        outgoing_messages_.push_front(std::move(message_view));
        return true;
      }
    }

    return true;
  }

  // Keeps the Channel alive at least until explicit shutdown on the IO thread.
  scoped_refptr<Channel> self_;

  ScopedPlatformHandle handle_;
  scoped_refptr<base::TaskRunner> io_task_runner_;

  // These watchers must only be accessed on the IO thread.
  scoped_ptr<base::MessageLoopForIO::FileDescriptorWatcher> read_watcher_;
  scoped_ptr<base::MessageLoopForIO::FileDescriptorWatcher> write_watcher_;

  std::deque<PlatformHandle> incoming_platform_handles_;

  // Protects |pending_write_| and |outgoing_messages_|.
  base::Lock write_lock_;
  bool pending_write_ = false;
  bool reject_writes_ = false;
  std::deque<MessageView> outgoing_messages_;

  DISALLOW_COPY_AND_ASSIGN(ChannelPosix);
};

}  // namespace

// static
scoped_refptr<Channel> Channel::Create(
    Delegate* delegate,
    ScopedPlatformHandle platform_handle,
    scoped_refptr<base::TaskRunner> io_task_runner) {
  return new ChannelPosix(delegate, std::move(platform_handle), io_task_runner);
}

}  // namespace edk
}  // namespace mojo
