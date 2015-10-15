// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/edk/system/raw_channel.h"

#include <string.h>

#include <algorithm>

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "mojo/edk/embedder/embedder_internal.h"
#include "mojo/edk/system/configuration.h"
#include "mojo/edk/system/message_in_transit.h"
#include "mojo/edk/system/transport_data.h"

namespace mojo {
namespace edk {

const size_t kReadSize = 4096;

// RawChannel::ReadBuffer ------------------------------------------------------

RawChannel::ReadBuffer::ReadBuffer() : buffer_(kReadSize), num_valid_bytes_(0) {
}

RawChannel::ReadBuffer::~ReadBuffer() {
}

void RawChannel::ReadBuffer::GetBuffer(char** addr, size_t* size) {
  DCHECK_GE(buffer_.size(), num_valid_bytes_ + kReadSize);
  *addr = &buffer_[0] + num_valid_bytes_;
  *size = kReadSize;
}

// RawChannel::WriteBuffer -----------------------------------------------------

RawChannel::WriteBuffer::WriteBuffer()
    : serialized_platform_handle_size_(0),
      platform_handles_offset_(0),
      data_offset_(0) {
}

RawChannel::WriteBuffer::~WriteBuffer() {
  message_queue_.Clear();
}

bool RawChannel::WriteBuffer::HavePlatformHandlesToSend() const {
  if (message_queue_.IsEmpty())
    return false;

  const TransportData* transport_data =
      message_queue_.PeekMessage()->transport_data();
  if (!transport_data)
    return false;

  const PlatformHandleVector* all_platform_handles =
      transport_data->platform_handles();
  if (!all_platform_handles) {
    DCHECK_EQ(platform_handles_offset_, 0u);
    return false;
  }
  if (platform_handles_offset_ >= all_platform_handles->size()) {
    DCHECK_EQ(platform_handles_offset_, all_platform_handles->size());
    return false;
  }

  return true;
}

void RawChannel::WriteBuffer::GetPlatformHandlesToSend(
    size_t* num_platform_handles,
    PlatformHandle** platform_handles,
    void** serialization_data) {
  DCHECK(HavePlatformHandlesToSend());

  MessageInTransit* message = message_queue_.PeekMessage();
  TransportData* transport_data = message->transport_data();
  PlatformHandleVector* all_platform_handles =
      transport_data->platform_handles();
  *num_platform_handles =
      all_platform_handles->size() - platform_handles_offset_;
  *platform_handles = &(*all_platform_handles)[platform_handles_offset_];

  if (serialized_platform_handle_size_ > 0) {
    size_t serialization_data_offset =
        transport_data->platform_handle_table_offset();
    serialization_data_offset +=
        platform_handles_offset_ * serialized_platform_handle_size_;
    *serialization_data = static_cast<char*>(transport_data->buffer()) +
                          serialization_data_offset;
  } else {
    *serialization_data = nullptr;
  }
}

void RawChannel::WriteBuffer::GetBuffers(std::vector<Buffer>* buffers) {
  buffers->clear();

  if (message_queue_.IsEmpty())
    return;

  const MessageInTransit* message = message_queue_.PeekMessage();
  if (message->type() == MessageInTransit::Type::RAW_MESSAGE) {
    // These are already-serialized messages so we don't want to write another
    // header as they include that.
    if (data_offset_ == 0) {
      size_t header_size = message->total_size() - message->num_bytes();
      data_offset_ = header_size;
    }
  }

  DCHECK_LT(data_offset_, message->total_size());
  size_t bytes_to_write = message->total_size() - data_offset_;

  size_t transport_data_buffer_size =
      message->transport_data() ? message->transport_data()->buffer_size() : 0;

  if (!transport_data_buffer_size) {
    // Only write from the main buffer.
    DCHECK_LT(data_offset_, message->main_buffer_size());
    DCHECK_LE(bytes_to_write, message->main_buffer_size());
    Buffer buffer = {
        static_cast<const char*>(message->main_buffer()) + data_offset_,
        bytes_to_write};

    buffers->push_back(buffer);
    return;
  }

  if (data_offset_ >= message->main_buffer_size()) {
    // Only write from the transport data buffer.
    DCHECK_LT(data_offset_ - message->main_buffer_size(),
              transport_data_buffer_size);
    DCHECK_LE(bytes_to_write, transport_data_buffer_size);
    Buffer buffer = {
        static_cast<const char*>(message->transport_data()->buffer()) +
            (data_offset_ - message->main_buffer_size()),
        bytes_to_write};

    buffers->push_back(buffer);
    return;
  }

  // TODO(vtl): We could actually send out buffers from multiple messages, with
  // the "stopping" condition being reaching a message with platform handles
  // attached.

  // Write from both buffers.
  DCHECK_EQ(bytes_to_write, message->main_buffer_size() - data_offset_ +
                                transport_data_buffer_size);
  Buffer buffer1 = {
      static_cast<const char*>(message->main_buffer()) + data_offset_,
      message->main_buffer_size() - data_offset_};
  buffers->push_back(buffer1);
  Buffer buffer2 = {
      static_cast<const char*>(message->transport_data()->buffer()),
      transport_data_buffer_size};
  buffers->push_back(buffer2);
}

// RawChannel ------------------------------------------------------------------

RawChannel::RawChannel()
    : message_loop_for_io_(nullptr),
      delegate_(nullptr),
      write_ready_(false),
      write_stopped_(false),
      error_occurred_(false),
      pending_error_(false),
      weak_ptr_factory_(this) {
  read_buffer_.reset(new ReadBuffer);
  write_buffer_.reset(new WriteBuffer());
}

RawChannel::~RawChannel() {
  DCHECK(!read_buffer_);
  DCHECK(!write_buffer_);

  // Only want to decrement counter if Init was called.
  if (message_loop_for_io_) {
    // No need to take the |write_lock_| here -- if there are still weak
    // pointers outstanding, then we're hosed anyway (since we wouldn't be able
    // to invalidate them cleanly, since we might not be on the I/O thread).
  //  DCHECK(!weak_ptr_factory_.HasWeakPtrs());
    internal::ChannelShutdown();
  }
}

void RawChannel::Init(Delegate* delegate) {
  internal::ChannelStarted();
  DCHECK(delegate);

  base::AutoLock read_locker(read_lock_);
  // solves race where initialiing on io thread while main thread is serializing
  // this channel and releases handle.
  base::AutoLock locker(write_lock_);

  DCHECK(!delegate_);
  delegate_ = delegate;

  DCHECK(!message_loop_for_io_);
  message_loop_for_io_ =
      static_cast<base::MessageLoopForIO*>(base::MessageLoop::current());

  OnInit();

  if (read_buffer_->num_valid_bytes_) {
    // We had serialized read buffer data through SetSerializedData call.
    // Make sure we read messages out of it now, otherwise the delegate won't
    // get notified if no other data gets written to the pipe.
    // Although this means that we can call back synchronously into the caller,
    // that's easier than posting a task to do this. That is because if we post
    // a task, a pending read could have started and we wouldn't be able to move
    // the read buffer since it can be in use by the OS in an async operation.
    bool did_dispatch_message = false;
    bool stop_dispatching = false;
    DispatchMessages(&did_dispatch_message, &stop_dispatching);
  }

  IOResult io_result = ScheduleRead();
  if (io_result != IO_PENDING) {
    // This will notify the delegate about the read failure. Although we're on
    // the I/O thread, don't call it in the nested context.
    message_loop_for_io_->PostTask(
        FROM_HERE, base::Bind(&RawChannel::CallOnReadCompleted,
                              weak_ptr_factory_.GetWeakPtr(), io_result, 0));
  }
  // Note: |ScheduleRead()| failure is treated as a read failure (by notifying
  // the delegate), not an initialization failure.

  write_ready_ = true;
  write_buffer_->serialized_platform_handle_size_ =
      GetSerializedPlatformHandleSize();
  if (!write_buffer_->message_queue_.IsEmpty())
    SendQueuedMessagesNoLock();
}

void RawChannel::Shutdown() {
  weak_ptr_factory_.InvalidateWeakPtrs();

  // Normally, we want to flush any pending writes before shutting down. This
  // doesn't apply when 1) we don't have a handle (for obvious reasons) or
  // 2) when the other side already quit and asked us to close the handle to
  // ensure that we read everything out of the pipe first.
  if (!HandleForDebuggingNoLock().is_valid() || error_occurred_) {
    {
      base::AutoLock read_locker(read_lock_);
      base::AutoLock locker(write_lock_);
      OnShutdownNoLock(read_buffer_.Pass(), write_buffer_.Pass());
    }
    delete this;
    return;
  }

  base::AutoLock read_locker(read_lock_);
  base::AutoLock locker(write_lock_);
  DCHECK(read_buffer_->IsEmpty()) <<
      "RawChannel::Shutdown called but there is pending data to be read";

  // happens on shutdown if didn't call init when doing createduplicate
  if (message_loop_for_io()) {
    DCHECK_EQ(base::MessageLoop::current(), message_loop_for_io_);
  }

  // Reset the delegate so that it won't receive further calls.
  delegate_ = nullptr;

  bool empty = write_buffer_->message_queue_.IsEmpty();

  // We may have no messages to write. However just because our end of the pipe
  // wrote everything doesn't mean that the other end read it. We don't want to
  // call FlushFileBuffers since a) that only works for server end of the pipe,
  // and b) it pauses this thread (which can block a process on another, or
  // worse hang if both pipes are in the same process).
  scoped_ptr<MessageInTransit> quit_message(new MessageInTransit(
      MessageInTransit::Type::RAW_CHANNEL_QUIT, 0, nullptr));
  EnqueueMessageNoLock(quit_message.Pass());
  write_stopped_ = true;

  if (empty)
    SendQueuedMessagesNoLock();
}

ScopedPlatformHandle RawChannel::ReleaseHandle(
    std::vector<char>* serialized_read_buffer,
    std::vector<char>* serialized_write_buffer) {
  ScopedPlatformHandle rv;
  {
    base::AutoLock read_locker(read_lock_);
    base::AutoLock locker(write_lock_);
    rv = ReleaseHandleNoLock(serialized_read_buffer, serialized_write_buffer);

    delegate_ = nullptr;

    // The Unretained is safe because above cancelled IO so we shouldn't get any
    // channel errors.
    // |message_loop_for_io_| might not be set yet
    internal::g_io_thread_task_runner->PostTask(
        FROM_HERE,
        base::Bind(&RawChannel::Shutdown, base::Unretained(this)));
  }

  return rv;
}

// Reminder: This must be thread-safe.
bool RawChannel::WriteMessage(scoped_ptr<MessageInTransit> message) {
  DCHECK(message);
  base::AutoLock locker(write_lock_);
  if (write_stopped_)
    return false;

  bool queue_was_empty = write_buffer_->message_queue_.IsEmpty();
  EnqueueMessageNoLock(message.Pass());
  if (queue_was_empty && write_ready_)
    return SendQueuedMessagesNoLock();

  return true;
}

bool RawChannel::SendQueuedMessagesNoLock() {
  DCHECK_EQ(write_buffer_->data_offset_, 0u);

  size_t platform_handles_written = 0;
  size_t bytes_written = 0;
  IOResult io_result = WriteNoLock(&platform_handles_written, &bytes_written);
  if (io_result == IO_PENDING)
    return true;

  bool result = OnWriteCompletedInternalNoLock(
      io_result, platform_handles_written, bytes_written);
  if (!result) {
    // Even if we're on the I/O thread, don't call |OnError()| in the nested
    // context.
    pending_error_ = true;
    message_loop_for_io_->PostTask(
        FROM_HERE,
        base::Bind(&RawChannel::LockAndCallOnError,
                   weak_ptr_factory_.GetWeakPtr(),
                   Delegate::ERROR_WRITE));
  }

  return result;
}

void RawChannel::SetSerializedData(
    char* serialized_read_buffer, size_t serialized_read_buffer_size,
    char* serialized_write_buffer, size_t serialized_write_buffer_size) {
  base::AutoLock locker(read_lock_);

  if (serialized_read_buffer_size) {
    // TODO(jam): copy power of 2 algorithm below? or share.
    read_buffer_->buffer_.resize(serialized_read_buffer_size + kReadSize);
    memcpy(&read_buffer_->buffer_[0], serialized_read_buffer,
           serialized_read_buffer_size);
    read_buffer_->num_valid_bytes_ = serialized_read_buffer_size;
  }

  if (serialized_write_buffer_size) {
    size_t max_message_num_bytes = GetConfiguration().max_message_num_bytes;

    uint32_t offset = 0;
    while (offset < serialized_write_buffer_size) {
      uint32_t message_num_bytes =
          std::min(static_cast<uint32_t>(max_message_num_bytes),
                   static_cast<uint32_t>(serialized_write_buffer_size) -
                       offset);
      scoped_ptr<MessageInTransit> message(new MessageInTransit(
          MessageInTransit::Type::RAW_MESSAGE, message_num_bytes,
          static_cast<const char*>(serialized_write_buffer) + offset));
      write_buffer_->message_queue_.AddMessage(message.Pass());
      offset += message_num_bytes;
    }
  }
}

void RawChannel::OnReadCompletedNoLock(IOResult io_result, size_t bytes_read) {
  DCHECK_EQ(base::MessageLoop::current(), message_loop_for_io_);
  read_lock_.AssertAcquired();
  // Keep reading data in a loop, and dispatch messages if enough data is
  // received. Exit the loop if any of the following happens:
  //   - one or more messages were dispatched;
  //   - the last read failed, was a partial read or would block;
  //   - |Shutdown()| was called.
  do {
    switch (io_result) {
      case IO_SUCCEEDED:
        break;
      case IO_FAILED_SHUTDOWN:
      case IO_FAILED_BROKEN:
      case IO_FAILED_UNKNOWN:
        CallOnError(ReadIOResultToError(io_result));
        return;  // |this| may have been destroyed in |CallOnError()|.
      case IO_PENDING:
        NOTREACHED();
        return;
    }

    read_buffer_->num_valid_bytes_ += bytes_read;

    // Dispatch all the messages that we can.
    bool did_dispatch_message = false;
    bool stop_dispatching = false;
    DispatchMessages(&did_dispatch_message, &stop_dispatching);
    if (stop_dispatching)
      return;

    if (read_buffer_->buffer_.size() - read_buffer_->num_valid_bytes_ <
        kReadSize) {
      // Use power-of-2 buffer sizes.
      // TODO(vtl): Make sure the buffer doesn't get too large (and enforce the
      // maximum message size to whatever extent necessary).
      // TODO(vtl): We may often be able to peek at the header and get the real
      // required extra space (which may be much bigger than |kReadSize|).
      size_t new_size = std::max(read_buffer_->buffer_.size(), kReadSize);
      while (new_size < read_buffer_->num_valid_bytes_ + kReadSize)
        new_size *= 2;

      // TODO(vtl): It's suboptimal to zero out the fresh memory.
      read_buffer_->buffer_.resize(new_size, 0);
    }

    // (1) If we dispatched any messages, stop reading for now (and let the
    // message loop do its thing for another round).
    // TODO(vtl): Is this the behavior we want? (Alternatives: i. Dispatch only
    // a single message. Risks: slower, more complex if we want to avoid lots of
    // copying. ii. Keep reading until there's no more data and dispatch all the
    // messages we can. Risks: starvation of other users of the message loop.)
    // (2) If we didn't max out |kReadSize|, stop reading for now.
    bool schedule_for_later = did_dispatch_message || bytes_read < kReadSize;
    bytes_read = 0;
    io_result = schedule_for_later ? ScheduleRead() : Read(&bytes_read);
  } while (io_result != IO_PENDING);
}

void RawChannel::OnWriteCompletedNoLock(IOResult io_result,
                                        size_t platform_handles_written,
                                        size_t bytes_written) {
  DCHECK_EQ(base::MessageLoop::current(), message_loop_for_io_);
  write_lock_.AssertAcquired();
  DCHECK_NE(io_result, IO_PENDING);

  bool did_fail = !OnWriteCompletedInternalNoLock(
      io_result, platform_handles_written, bytes_written);
  if (did_fail)
    LockAndCallOnError(Delegate::ERROR_WRITE);
}

void RawChannel::SerializeReadBuffer(size_t additional_bytes_read,
                                     std::vector<char>* buffer) {
  read_buffer_->num_valid_bytes_ += additional_bytes_read;
  read_buffer_->buffer_.resize(read_buffer_->num_valid_bytes_);
  read_buffer_->buffer_.swap(*buffer);
  read_buffer_->num_valid_bytes_ = 0;
}

void RawChannel::SerializeWriteBuffer(
    size_t additional_bytes_written,
    size_t additional_platform_handles_written,
    std::vector<char>* buffer) {
  if (write_buffer_->IsEmpty()) {
    DCHECK_EQ(0u, additional_bytes_written);
    DCHECK_EQ(0u, additional_platform_handles_written);
    return;
  }

  UpdateWriteBuffer(
      additional_platform_handles_written, additional_bytes_written);
  while (!write_buffer_->message_queue_.IsEmpty()) {
    SerializePlatformHandles();
    std::vector<WriteBuffer::Buffer> buffers;
    write_buffer_no_lock()->GetBuffers(&buffers);
    for (size_t i = 0; i < buffers.size(); ++i) {
      buffer->insert(buffer->end(), buffers[i].addr,
                     buffers[i].addr + buffers[i].size);
    }
    write_buffer_->message_queue_.DiscardMessage();
  }
}

void RawChannel::EnqueueMessageNoLock(scoped_ptr<MessageInTransit> message) {
  write_lock_.AssertAcquired();
  DCHECK(HandleForDebuggingNoLock().is_valid());
  write_buffer_->message_queue_.AddMessage(message.Pass());
}

bool RawChannel::OnReadMessageForRawChannel(
    const MessageInTransit::View& message_view) {
  if (message_view.type() == MessageInTransit::Type::RAW_CHANNEL_QUIT) {
    pending_error_ = true;
    message_loop_for_io_->PostTask(
        FROM_HERE, base::Bind(&RawChannel::LockAndCallOnError,
                              weak_ptr_factory_.GetWeakPtr(),
                              Delegate::ERROR_READ_SHUTDOWN));
    return true;
  }

  // No non-implementation specific |RawChannel| control messages.
  LOG(ERROR) << "Invalid control message (type " << message_view.type()
             << ")";
  return false;
}

RawChannel::Delegate::Error RawChannel::ReadIOResultToError(
    IOResult io_result) {
  switch (io_result) {
    case IO_FAILED_SHUTDOWN:
      return Delegate::ERROR_READ_SHUTDOWN;
    case IO_FAILED_BROKEN:
      return Delegate::ERROR_READ_BROKEN;
    case IO_FAILED_UNKNOWN:
      return Delegate::ERROR_READ_UNKNOWN;
    case IO_SUCCEEDED:
    case IO_PENDING:
      NOTREACHED();
      break;
  }
  return Delegate::ERROR_READ_UNKNOWN;
}

void RawChannel::CallOnError(Delegate::Error error) {
  DCHECK_EQ(base::MessageLoop::current(), message_loop_for_io_);
  read_lock_.AssertAcquired();
  error_occurred_ = true;
  if (delegate_) {
    delegate_->OnError(error);
  } else {
    // We depend on delegate to delete since it could be waiting to call
    // ReleaseHandle.
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&RawChannel::Shutdown, weak_ptr_factory_.GetWeakPtr()));
  }
}

void RawChannel::LockAndCallOnError(Delegate::Error error) {
  base::AutoLock locker(read_lock_);
  CallOnError(error);
}

bool RawChannel::OnWriteCompletedInternalNoLock(IOResult io_result,
                                                size_t platform_handles_written,
                                                size_t bytes_written) {
  write_lock_.AssertAcquired();

  DCHECK(!write_buffer_->message_queue_.IsEmpty());

  if (io_result == IO_SUCCEEDED) {
    UpdateWriteBuffer(platform_handles_written, bytes_written);
    if (write_buffer_->message_queue_.IsEmpty())
      return true;

    // Schedule the next write.
    io_result = ScheduleWriteNoLock();
    if (io_result == IO_PENDING)
      return true;
    DCHECK_NE(io_result, IO_SUCCEEDED);
  }

  write_stopped_ = true;
  write_buffer_->message_queue_.Clear();
  write_buffer_->platform_handles_offset_ = 0;
  write_buffer_->data_offset_ = 0;
  return false;
}

void RawChannel::DispatchMessages(bool* did_dispatch_message,
                                  bool* stop_dispatching) {
  *did_dispatch_message = false;
  *stop_dispatching = false;
  // Tracks the offset of the first undispatched message in |read_buffer_|.
  // Currently, we copy data to ensure that this is zero at the beginning.
  size_t read_buffer_start = 0;
  size_t remaining_bytes = read_buffer_->num_valid_bytes_;
  size_t message_size;
  // Note that we rely on short-circuit evaluation here:
  //   - |read_buffer_start| may be an invalid index into
  //     |read_buffer_->buffer_| if |remaining_bytes| is zero.
  //   - |message_size| is only valid if |GetNextMessageSize()| returns true.
  // TODO(vtl): Use |message_size| more intelligently (e.g., to request the
  // next read).
  // TODO(vtl): Validate that |message_size| is sane.
  while (remaining_bytes > 0 && MessageInTransit::GetNextMessageSize(
                                    &read_buffer_->buffer_[read_buffer_start],
                                    remaining_bytes, &message_size) &&
          remaining_bytes >= message_size) {
    MessageInTransit::View message_view(
        message_size, &read_buffer_->buffer_[read_buffer_start]);
    DCHECK_EQ(message_view.total_size(), message_size);

    const char* error_message = nullptr;
    if (!message_view.IsValid(GetSerializedPlatformHandleSize(),
                              &error_message)) {
      DCHECK(error_message);
      LOG(ERROR) << "Received invalid message: " << error_message;
      CallOnError(Delegate::ERROR_READ_BAD_MESSAGE);
      *stop_dispatching = true;
      return;  // |this| may have been destroyed in |CallOnError()|.
    }

    if (message_view.type() != MessageInTransit::Type::MESSAGE) {
      if (!OnReadMessageForRawChannel(message_view)) {
        CallOnError(Delegate::ERROR_READ_BAD_MESSAGE);
        *stop_dispatching = true;
        return;  // |this| may have been destroyed in |CallOnError()|.
      }
    } else {
      ScopedPlatformHandleVectorPtr platform_handles;
      if (message_view.transport_data_buffer()) {
        size_t num_platform_handles;
        const void* platform_handle_table;
        TransportData::GetPlatformHandleTable(
            message_view.transport_data_buffer(), &num_platform_handles,
            &platform_handle_table);

        if (num_platform_handles > 0) {
          platform_handles =
              GetReadPlatformHandles(num_platform_handles,
                                      platform_handle_table).Pass();
          if (!platform_handles) {
            LOG(ERROR) << "Invalid number of platform handles received";
            CallOnError(Delegate::ERROR_READ_BAD_MESSAGE);
            *stop_dispatching = true;
            return;  // |this| may have been destroyed in |CallOnError()|.
          }
        }
      }

      // TODO(vtl): In the case that we aren't expecting any platform handles,
      // for the POSIX implementation, we should confirm that none are stored.

      // Dispatch the message.
      // Note: it's valid to get here without a delegate. i.e. after Shutdown
      // is called, if this object still has a valid handle we keep it alive
      // until the other side closes it in response to the RAW_CHANNEL_QUIT
      // message. In the meantime the sender could have sent us a message.
      if (delegate_)
        delegate_->OnReadMessage(message_view, platform_handles.Pass());
    }

    *did_dispatch_message = true;

    // Update our state.
    read_buffer_start += message_size;
    remaining_bytes -= message_size;
  }

  if (read_buffer_start > 0) {
    // Move data back to start.
    read_buffer_->num_valid_bytes_ = remaining_bytes;
    if (read_buffer_->num_valid_bytes_ > 0) {
      memmove(&read_buffer_->buffer_[0],
              &read_buffer_->buffer_[read_buffer_start], remaining_bytes);
    }
    read_buffer_start = 0;
  }
}

void RawChannel::UpdateWriteBuffer(size_t platform_handles_written,
                                   size_t bytes_written) {
  write_buffer_->platform_handles_offset_ += platform_handles_written;
  write_buffer_->data_offset_ += bytes_written;

  MessageInTransit* message = write_buffer_->message_queue_.PeekMessage();
  if (write_buffer_->data_offset_ >= message->total_size()) {
    // Complete write.
    CHECK_EQ(write_buffer_->data_offset_, message->total_size());
    write_buffer_->message_queue_.DiscardMessage();
    write_buffer_->platform_handles_offset_ = 0;
    write_buffer_->data_offset_ = 0;
  }
}

void RawChannel::CallOnReadCompleted(IOResult io_result, size_t bytes_read) {
  base::AutoLock locker(read_lock());
  OnReadCompletedNoLock(io_result, bytes_read);
}

}  // namespace edk
}  // namespace mojo
