// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/net/blimp_message_output_buffer.h"

#include <algorithm>

#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "blimp/common/proto/blimp_message.pb.h"
#include "net/base/net_errors.h"

namespace blimp {

BlimpMessageOutputBuffer::BlimpMessageOutputBuffer(int max_buffer_size_bytes)
    : max_buffer_size_bytes_(max_buffer_size_bytes) {}

BlimpMessageOutputBuffer::~BlimpMessageOutputBuffer() {}

void BlimpMessageOutputBuffer::SetOutputProcessor(
    BlimpMessageProcessor* processor) {
  // Check that we are setting or removing the processor, not replacing it.
  if (processor) {
    DCHECK(!output_processor_);
    output_processor_ = processor;
    write_complete_cb_.Reset(base::Bind(
        &BlimpMessageOutputBuffer::OnWriteComplete, base::Unretained(this)));
    WriteNextMessageIfReady();
  } else {
    DCHECK(output_processor_);
    output_processor_ = nullptr;
    write_complete_cb_.Cancel();
  }
}

void BlimpMessageOutputBuffer::RetransmitBufferedMessages() {
  DCHECK(output_processor_);

  // Prepend the entirety of |ack_buffer_| to |write_buffer_|.
  write_buffer_.insert(write_buffer_.begin(),
                       std::make_move_iterator(ack_buffer_.begin()),
                       std::make_move_iterator(ack_buffer_.end()));
  ack_buffer_.clear();

  WriteNextMessageIfReady();
}

int BlimpMessageOutputBuffer::GetBufferByteSizeForTest() const {
  return write_buffer_.size() + ack_buffer_.size();
}

int BlimpMessageOutputBuffer::GetUnacknowledgedMessageCountForTest() const {
  return ack_buffer_.size();
}

void BlimpMessageOutputBuffer::ProcessMessage(
    scoped_ptr<BlimpMessage> message,
    const net::CompletionCallback& callback) {
  VLOG(2) << "ProcessMessage (id=" << message->message_id()
          << ", type=" << message->type() << ")";

  message->set_message_id(++prev_message_id_);

  current_buffer_size_bytes_ += message->ByteSize();
  DCHECK_GE(max_buffer_size_bytes_, current_buffer_size_bytes_);

  write_buffer_.push_back(
      make_scoped_ptr(new BufferEntry(message.Pass(), callback)));

  // Write the message
  if (write_buffer_.size() == 1 && output_processor_) {
    WriteNextMessageIfReady();
  }
}

// Flushes acknowledged messages from the buffer and invokes their
// |callbacks|, if any.
void BlimpMessageOutputBuffer::OnMessageCheckpoint(int64 message_id) {
  VLOG(2) << "OnMessageCheckpoint (message_id=" << message_id << ")";
  if (ack_buffer_.empty()) {
    LOG(WARNING) << "Checkpoint called while buffer is empty.";
    return;
  }
  if (message_id > prev_message_id_) {
    LOG(WARNING) << "Illegal checkpoint response: " << message_id;
    return;
  }

  // Remove all acknowledged messages through |message_id| and invoke their
  // write callbacks, if set.
  while (!ack_buffer_.empty() &&
         ack_buffer_.front()->message->message_id() <= message_id) {
    const BufferEntry& ack_entry = *ack_buffer_.front();
    current_buffer_size_bytes_ -= ack_entry.message->GetCachedSize();
    DCHECK_GE(current_buffer_size_bytes_, 0);
    VLOG(3) << "Buffer size: " << current_buffer_size_bytes_
            << " (max=" << current_buffer_size_bytes_ << ")";

    if (!ack_entry.callback.is_null()) {
      base::MessageLoop::current()->PostTask(
          FROM_HERE, base::Bind(ack_entry.callback, net::OK));
    }

    ack_buffer_.pop_front();
  }

  // An empty buffer should have a zero-byte footprint.
  DCHECK(current_buffer_size_bytes_ > 0 ||
         (ack_buffer_.empty() && write_buffer_.empty()))
      << "Expected zero-length buffer size, was " << current_buffer_size_bytes_
      << " bytes instead.";
}

BlimpMessageOutputBuffer::BufferEntry::BufferEntry(
    scoped_ptr<BlimpMessage> message,
    net::CompletionCallback callback)
    : message(message.Pass()), callback(callback) {}

BlimpMessageOutputBuffer::BufferEntry::~BufferEntry() {}

void BlimpMessageOutputBuffer::WriteNextMessageIfReady() {
  if (write_buffer_.empty()) {
    VLOG(2) << "Nothing to write.";
    return;
  }

  scoped_ptr<BlimpMessage> message_to_write(
      new BlimpMessage(*write_buffer_.front()->message));
  VLOG(3) << "Writing message (id="
          << write_buffer_.front()->message->message_id()
          << ", type=" << message_to_write->type() << ")";

  output_processor_->ProcessMessage(message_to_write.Pass(),
                                    write_complete_cb_.callback());
  VLOG(3) << "Queue size: " << write_buffer_.size();
}

void BlimpMessageOutputBuffer::OnWriteComplete(int result) {
  DCHECK_LE(result, net::OK);
  VLOG(2) << "Write complete, result=" << result;

  if (result == net::OK) {
    ack_buffer_.push_back(std::move(write_buffer_.front()));
    write_buffer_.pop_front();
    WriteNextMessageIfReady();
  } else {
    // An error occurred while writing to the network connection.
    // Stop writing more messages until a new connection is established.
    DLOG(WARNING) << "Write error (result=" << result << ")";
  }
}

}  // namespace blimp
