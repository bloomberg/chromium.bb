// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/system/raw_channel.h"

#include <string.h>

#include <algorithm>

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/stl_util.h"
#include "mojo/system/message_in_transit.h"

namespace mojo {
namespace system {

const size_t kReadSize = 4096;

RawChannel::ReadBuffer::ReadBuffer() : buffer_(kReadSize), num_valid_bytes_(0) {
}

RawChannel::ReadBuffer::~ReadBuffer() {}

void RawChannel::ReadBuffer::GetBuffer(char** addr, size_t* size) {
  DCHECK_GE(buffer_.size(), num_valid_bytes_ + kReadSize);
  *addr = &buffer_[0] + num_valid_bytes_;
  *size = kReadSize;
}

RawChannel::WriteBuffer::WriteBuffer() : offset_(0) {}

RawChannel::WriteBuffer::~WriteBuffer() {
  STLDeleteElements(&message_queue_);
}

void RawChannel::WriteBuffer::GetBuffers(std::vector<Buffer>* buffers) const {
  buffers->clear();

  size_t bytes_to_write = GetTotalBytesToWrite();
  if (bytes_to_write == 0)
    return;

  MessageInTransit* message = message_queue_.front();
  if (!message->secondary_buffer_size()) {
    // Only write from the main buffer.
    DCHECK_LT(offset_, message->main_buffer_size());
    DCHECK_LE(bytes_to_write, message->main_buffer_size());
    Buffer buffer = {
        static_cast<const char*>(message->main_buffer()) + offset_,
        bytes_to_write};
    buffers->push_back(buffer);
    return;
  }

  if (offset_ >= message->main_buffer_size()) {
    // Only write from the secondary buffer.
    DCHECK_LT(offset_ - message->main_buffer_size(),
              message->secondary_buffer_size());
    DCHECK_LE(bytes_to_write, message->secondary_buffer_size());
    Buffer buffer = {
        static_cast<const char*>(message->secondary_buffer()) +
            (offset_ - message->main_buffer_size()),
        bytes_to_write};
    buffers->push_back(buffer);
    return;
  }

  // Write from both buffers.
  DCHECK_EQ(bytes_to_write, message->main_buffer_size() - offset_ +
                                message->secondary_buffer_size());
  Buffer buffer1 = {
      static_cast<const char*>(message->main_buffer()) + offset_,
      message->main_buffer_size() - offset_};
  buffers->push_back(buffer1);
  Buffer buffer2 = {
      static_cast<const char*>(message->secondary_buffer()),
      message->secondary_buffer_size()};
  buffers->push_back(buffer2);
}

size_t RawChannel::WriteBuffer::GetTotalBytesToWrite() const {
  if (message_queue_.empty())
    return 0;

  MessageInTransit* message = message_queue_.front();
  DCHECK_LT(offset_, message->total_size());
  return message->total_size() - offset_;
}

RawChannel::RawChannel()
    : delegate_(NULL),
      message_loop_for_io_(NULL),
      read_stopped_(false),
      write_stopped_(false),
      weak_ptr_factory_(this) {
}

RawChannel::~RawChannel() {
  DCHECK(!read_buffer_);
  DCHECK(!write_buffer_);

  // No need to take the |write_lock_| here -- if there are still weak pointers
  // outstanding, then we're hosed anyway (since we wouldn't be able to
  // invalidate them cleanly, since we might not be on the I/O thread).
  DCHECK(!weak_ptr_factory_.HasWeakPtrs());
}

bool RawChannel::Init(Delegate* delegate) {
  DCHECK(delegate);

  DCHECK(!delegate_);
  delegate_ = delegate;

  CHECK_EQ(base::MessageLoop::current()->type(), base::MessageLoop::TYPE_IO);
  DCHECK(!message_loop_for_io_);
  message_loop_for_io_ =
      static_cast<base::MessageLoopForIO*>(base::MessageLoop::current());

  // No need to take the lock. No one should be using us yet.
  DCHECK(!read_buffer_);
  read_buffer_.reset(new ReadBuffer);
  DCHECK(!write_buffer_);
  write_buffer_.reset(new WriteBuffer);

  if (!OnInit())
    return false;

  return ScheduleRead() == IO_PENDING;
}

void RawChannel::Shutdown() {
  DCHECK_EQ(base::MessageLoop::current(), message_loop_for_io_);

  base::AutoLock locker(write_lock_);

  LOG_IF(WARNING, !write_buffer_->message_queue_.empty())
      << "Shutting down RawChannel with write buffer nonempty";

  weak_ptr_factory_.InvalidateWeakPtrs();

  read_stopped_ = true;
  write_stopped_ = true;

  OnShutdownNoLock(read_buffer_.Pass(), write_buffer_.Pass());
}

// Reminder: This must be thread-safe.
bool RawChannel::WriteMessage(scoped_ptr<MessageInTransit> message) {
  base::AutoLock locker(write_lock_);
  if (write_stopped_)
    return false;

  if (!write_buffer_->message_queue_.empty()) {
    write_buffer_->message_queue_.push_back(message.release());
    return true;
  }

  write_buffer_->message_queue_.push_front(message.release());
  DCHECK_EQ(write_buffer_->offset_, 0u);

  size_t bytes_written = 0;
  IOResult io_result = WriteNoLock(&bytes_written);
  if (io_result == IO_PENDING)
    return true;

  bool result = OnWriteCompletedNoLock(io_result == IO_SUCCEEDED,
                                       bytes_written);
  if (!result) {
    // Even if we're on the I/O thread, don't call |OnFatalError()| in the
    // nested context.
    message_loop_for_io_->PostTask(
        FROM_HERE,
        base::Bind(&RawChannel::CallOnFatalError,
                   weak_ptr_factory_.GetWeakPtr(),
                   Delegate::FATAL_ERROR_FAILED_WRITE));
  }

  return result;
}

// Reminder: This must be thread-safe.
bool RawChannel::IsWriteBufferEmpty() {
  base::AutoLock locker(write_lock_);
  return write_buffer_->message_queue_.empty();
}

RawChannel::ReadBuffer* RawChannel::read_buffer() {
  DCHECK_EQ(base::MessageLoop::current(), message_loop_for_io_);
  return read_buffer_.get();
}

RawChannel::WriteBuffer* RawChannel::write_buffer_no_lock() {
  write_lock_.AssertAcquired();
  return write_buffer_.get();
}

void RawChannel::OnReadCompleted(bool result, size_t bytes_read) {
  DCHECK_EQ(base::MessageLoop::current(), message_loop_for_io_);

  if (read_stopped_) {
    NOTREACHED();
    return;
  }

  IOResult io_result = result ? IO_SUCCEEDED : IO_FAILED;

  // Keep reading data in a loop, and dispatch messages if enough data is
  // received. Exit the loop if any of the following happens:
  //   - one or more messages were dispatched;
  //   - the last read failed, was a partial read or would block;
  //   - |Shutdown()| was called.
  do {
    if (io_result != IO_SUCCEEDED) {
      read_stopped_ = true;
      CallOnFatalError(Delegate::FATAL_ERROR_FAILED_READ);
      return;
    }

    read_buffer_->num_valid_bytes_ += bytes_read;

    // Dispatch all the messages that we can.
    bool did_dispatch_message = false;
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
    while (remaining_bytes > 0 &&
           MessageInTransit::GetNextMessageSize(
               &read_buffer_->buffer_[read_buffer_start], remaining_bytes,
               &message_size) &&
           remaining_bytes >= message_size) {
      MessageInTransit::View
          message_view(message_size, &read_buffer_->buffer_[read_buffer_start]);
      DCHECK_EQ(message_view.total_size(), message_size);

      // Dispatch the message.
      delegate_->OnReadMessage(message_view);
      if (read_stopped_) {
        // |Shutdown()| was called in |OnReadMessage()|.
        // TODO(vtl): Add test for this case.
        return;
      }
      did_dispatch_message = true;

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

void RawChannel::OnWriteCompleted(bool result, size_t bytes_written) {
  DCHECK_EQ(base::MessageLoop::current(), message_loop_for_io_);

  bool did_fail = false;
  {
    base::AutoLock locker(write_lock_);
    DCHECK_EQ(write_stopped_, write_buffer_->message_queue_.empty());

    if (write_stopped_) {
      NOTREACHED();
      return;
    }

    did_fail = !OnWriteCompletedNoLock(result, bytes_written);
  }

  if (did_fail)
    CallOnFatalError(Delegate::FATAL_ERROR_FAILED_WRITE);
}

void RawChannel::CallOnFatalError(Delegate::FatalError fatal_error) {
  DCHECK_EQ(base::MessageLoop::current(), message_loop_for_io_);
  // TODO(vtl): Add a "write_lock_.AssertNotAcquired()"?
  delegate_->OnFatalError(fatal_error);
}

bool RawChannel::OnWriteCompletedNoLock(bool result, size_t bytes_written) {
  write_lock_.AssertAcquired();

  DCHECK(!write_stopped_);
  DCHECK(!write_buffer_->message_queue_.empty());

  if (result) {
    if (bytes_written < write_buffer_->GetTotalBytesToWrite()) {
      // Partial (or no) write.
      write_buffer_->offset_ += bytes_written;
    } else {
      // Complete write.
      DCHECK_EQ(bytes_written, write_buffer_->GetTotalBytesToWrite());
      delete write_buffer_->message_queue_.front();
      write_buffer_->message_queue_.pop_front();
      write_buffer_->offset_ = 0;
    }

    if (write_buffer_->message_queue_.empty())
      return true;

    // Schedule the next write.
    if (ScheduleWriteNoLock() == IO_PENDING)
      return true;
  }

  write_stopped_ = true;
  STLDeleteElements(&write_buffer_->message_queue_);
  write_buffer_->offset_ = 0;
  return false;
}

}  // namespace system
}  // namespace mojo
