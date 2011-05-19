// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/curvecp/messenger.h"

#include "base/logging.h"
#include "base/message_loop.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/curvecp/protocol.h"

// Basic protocol design:
//
// OnTimeout:    Called when the timeout timer pops.
//   - call SendMessage()
//   - call RecvMessage()
//
// OnSendTimer:  Called when the send-timer pops.
//   - call SendMessage()
//   - call RecvMessage()
//
// OnMessage:    Called when a message arrived from the packet layer
//   - add the message to the receive queue
//   - call RecvMessage()
//
// Write:        Called by application to write data to remote
//   - add the data to the send_buffer
//   - call SendMessage()
//
// SendMessage:  Called to Send a message to the remote
//   - msg = first message to retransmit where retransmit timer popped
//   - if msg == NULL
//   -   msg = create a new message from the send buffer
//   - if msg != NULL
//   -   send message to the packet layer
//   -   setTimer(OnSendTimer, send_rate);
//
// RecvMessage:  Called to process a Received message from the remote
//   - calculate RTT
//   - calculate Send Rate
//   - acknowledge data from received message
//   - resetTimeout
//      - timeout = now + rtt_timeout
//      - if currrent_timeout > timeout
//         setTimer(OnTimeout, timeout)

namespace net {

// Maximum number of write blocks.
static const size_t kMaxWriteQueueMessages = 128;

// Size of the send buffer.
static const size_t kSendBufferSize = (128 * 1024);
// Size of the receive buffer.
static const size_t kReceiveBufferSize = (128 * 1024);

Messenger::Messenger(Packetizer* packetizer)
    : packetizer_(packetizer),
      send_buffer_(kSendBufferSize),
      send_complete_callback_(NULL),
      receive_complete_callback_(NULL),
      pending_receive_length_(0),
      send_message_in_progress_(false),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          send_message_callback_(this, &Messenger::OnSendMessageComplete)),
      ALLOW_THIS_IN_INITIALIZER_LIST(factory_(this)) {
}

Messenger::~Messenger() {
}

int Messenger::Read(IOBuffer* buf, int buf_len, CompletionCallback* callback) {
  DCHECK(CalledOnValidThread());
  DCHECK(!receive_complete_callback_);

  if (!received_list_.bytes_available()) {
    receive_complete_callback_ = callback;
    pending_receive_ = buf;
    pending_receive_length_ = buf_len;
    return ERR_IO_PENDING;
  }

  int bytes_read = InternalRead(buf, buf_len);
  DCHECK_LT(0, bytes_read);
  return bytes_read;
}

int Messenger::Write(IOBuffer* buf, int buf_len, CompletionCallback* callback) {
  DCHECK(CalledOnValidThread());
  DCHECK(!pending_send_.get());   // Already a write pending!
  DCHECK(!send_complete_callback_);
  DCHECK_LT(0, buf_len);

  int len = send_buffer_.write(buf->data(), buf_len);
  if (!send_timer_.IsRunning())
    send_timer_.Start(base::TimeDelta(), this, &Messenger::OnSendTimer);
  if (len)
    return len;

  // We couldn't add data to the send buffer, so block the application.
  pending_send_ = buf;
  pending_send_length_ = buf_len;
  send_complete_callback_ = callback;
  return ERR_IO_PENDING;
}

void Messenger::OnConnection(ConnectionKey key) {
  LOG(ERROR) << "Client Connect: " << key.ToString();
  key_ = key;
}

void Messenger::OnClose(Packetizer* packetizer, ConnectionKey key) {
  LOG(ERROR) << "Got Close!";
}

void Messenger::OnMessage(Packetizer* packetizer,
                          ConnectionKey key,
                          unsigned char* msg,
                          size_t length) {
  DCHECK(key == key_);

  // Do basic message sanity checking.
  if (length < sizeof(Message))
    return;
  if (length > static_cast<size_t>(kMaxMessageLength))
    return;

  // Add message to received queue.
  scoped_refptr<IOBufferWithSize> buffer(new IOBufferWithSize(length));
  memcpy(buffer->data(), msg, length);
  read_queue_.push_back(buffer);

  // Process a single received message
  RecvMessage();
}

int Messenger::InternalRead(IOBuffer* buffer, int buffer_length) {
  return received_list_.ReadBytes(buffer, buffer_length);
}

IOBufferWithSize* Messenger::CreateBufferFromSendQueue() {
  DCHECK_LT(0, send_buffer_.length());

  int length = std::min(packetizer_->max_message_payload(),
                        send_buffer_.length());
  IOBufferWithSize* rv = new IOBufferWithSize(length);
  int bytes = send_buffer_.read(rv->data(), length);
  DCHECK_EQ(bytes, length);

  // We consumed data, check to see if someone is waiting to write more data.
  if (send_complete_callback_) {
    DCHECK(pending_send_.get());

    int len = send_buffer_.write(pending_send_->data(), pending_send_length_);
    if (len) {
      pending_send_ = NULL;
      CompletionCallback* callback = send_complete_callback_;
      send_complete_callback_ = NULL;
      callback->Run(len);
    }
  }

  return rv;
}

void Messenger::OnSendMessageComplete(int result) {
  DCHECK_NE(ERR_IO_PENDING, result);

  send_message_in_progress_ = false;

  if (result <= 0) {
    // TODO(mbelshe):  Handle error.
    NOTIMPLEMENTED();
  }

  // If the send timer fired while we were waiting for this send to complete,
  // we need to manually run the timer now.
  if (!send_timer_.IsRunning())
    OnSendTimer();

  if (!send_timeout_timer_.IsRunning()) {
    LOG(ERROR) << "RttTimeout is " << rtt_.rtt_timeout();
    base::TimeDelta delay =
        base::TimeDelta::FromMicroseconds(rtt_.rtt_timeout());
    send_timeout_timer_.Start(delay, this, &Messenger::OnTimeout);
  }
}

void Messenger::OnTimeout() {
  LOG(ERROR) << "OnTimeout fired";
  int64 position = sent_list_.FindPositionOfOldestSentBlock();
  // XXXMB - need to verify that we really need to retransmit...
  if (position >= 0) {
    rtt_.OnTimeout();  // adjust our send rate.
    LOG(ERROR) << "OnTimeout retransmitting: " << position;
    SendMessage(position);
  } else {
    DCHECK_EQ(0u, sent_list_.size());
  }
  RecvMessage();
  received_list_.LogBlockList();
}

void Messenger::OnSendTimer() {
  LOG(ERROR) << "OnSendTimer!";
  DCHECK(!send_timer_.IsRunning());

  // If the send buffer is empty, then we don't need to keep firing.
  if (!send_buffer_.length()) {
    LOG(ERROR) << "OnSendTimer: send_buffer empty";
    return;
  }

  // Set the next send timer.
  LOG(ERROR) << "SendRate is: " << rtt_.send_rate() << "us";
  send_timer_.Start(base::TimeDelta::FromMicroseconds(rtt_.send_rate()),
                    this,
                    &Messenger::OnSendTimer);

  // Create a block from the send_buffer.
  if (!sent_list_.is_full()) {
    scoped_refptr<IOBufferWithSize> buffer = CreateBufferFromSendQueue();
    int64 position = sent_list_.CreateBlock(buffer.get());
    DCHECK_LE(0, position);
    SendMessage(position);
  }

  RecvMessage();  // Try to process an incoming message
}

void Messenger::SendMessage(int64 position) {
  LOG(ERROR) << "SendMessage (position=" << position << ")";
  if (send_message_in_progress_)
    return;  // We're still waiting for the last write to complete.

  IOBufferWithSize* data = sent_list_.FindBlockByPosition(position);
  DCHECK(data != NULL);
  size_t message_size = sizeof(Message) + data->size();
  size_t padded_size = (message_size + 15) & 0xfffffff0;

  scoped_refptr<IOBufferWithSize> message(new IOBufferWithSize(padded_size));
  Message* header = reinterpret_cast<Message*>(message->data());
  memset(header, 0, sizeof(Message));
  uint64 id = sent_list_.GetNewMessageId();
  uint32_pack(header->message_id, id);
  // TODO(mbelshe): Needs to carry EOF flags
  uint16_pack(header->size.val, data->size());
  uint64_pack(header->position, position);
  // TODO(mbelshe): Fill in rest of the header fields.
  //     needs to have the block-position.  He tags each chunk with an
  //     absolute offset into the data stream.
  // Copy the contents of the message into the Message frame.
  memcpy(message->data() + sizeof(Message), data->data(), data->size());

  sent_list_.MarkBlockSent(position, id);

  int rv = packetizer_->SendMessage(key_,
                                    message->data(),
                                    padded_size,
                                    &send_message_callback_);
  if (rv == ERR_IO_PENDING) {
    send_message_in_progress_ = true;
    return;
  }

  // UDP must write all or none.
  DCHECK_EQ(padded_size, static_cast<size_t>(rv));
  OnSendMessageComplete(rv);
}

void Messenger::RecvMessage() {
  if (!read_queue_.size())
    return;

  scoped_refptr<IOBufferWithSize> message(read_queue_.front());
  read_queue_.pop_front();

  Message* header = reinterpret_cast<Message*>(message->data());
  uint16 body_length = uint16_unpack(header->size.val);
  if (body_length > kMaxMessageLength)
    return;
  if (body_length > message->size())
    return;

  uint32 message_id = uint32_unpack(header->message_id);
  if (message_id) {
    LOG(ERROR) << "RecvMessage Message id: " << message_id
               << ", " << body_length << " bytes";
  } else {
    LOG(ERROR) << "RecvMessage ACK ";
  }

  // See if this message has information for recomputing RTT.
  uint32 response_to_msg = uint32_unpack(header->last_message_received);
  base::TimeTicks last_sent_time = sent_list_.FindLastSendTime(response_to_msg);
  if (!last_sent_time.is_null()) {
    int rtt = (base::TimeTicks::Now() - last_sent_time).InMicroseconds();
    DCHECK_LE(0, rtt);
    LOG(ERROR) << "RTT was: " << rtt << "us";
    rtt_.OnMessage(rtt);
  }

  // Handle acknowledgements
  uint64 start_byte = 0;
  uint64 stop_byte = uint64_unpack(header->acknowledge_1);
  sent_list_.AcknowledgeBlocks(start_byte, stop_byte);

  start_byte = stop_byte + uint16_unpack(header->gap_1);
  stop_byte = start_byte + uint16_unpack(header->acknowledge_2);
  sent_list_.AcknowledgeBlocks(start_byte, stop_byte);

  start_byte = stop_byte + uint16_unpack(header->gap_2);
  stop_byte = start_byte + uint16_unpack(header->acknowledge_3);
  sent_list_.AcknowledgeBlocks(start_byte, stop_byte);

  start_byte = stop_byte + uint16_unpack(header->gap_3);
  stop_byte = start_byte + uint16_unpack(header->acknowledge_4);
  sent_list_.AcknowledgeBlocks(start_byte, stop_byte);

  start_byte = stop_byte + uint16_unpack(header->gap_4);
  stop_byte = start_byte + uint16_unpack(header->acknowledge_5);
  sent_list_.AcknowledgeBlocks(start_byte, stop_byte);

  start_byte = stop_byte + uint16_unpack(header->gap_5);
  stop_byte = start_byte + uint16_unpack(header->acknowledge_6);
  sent_list_.AcknowledgeBlocks(start_byte, stop_byte);

  if (!header->is_ack()) {
    // Add to our received block list
    uint64 position = uint64_unpack(header->position);
    scoped_refptr<IOBuffer> buffer(new IOBuffer(body_length));
    memcpy(buffer->data(), message->data() + sizeof(Message), body_length);
    received_list_.AddBlock(position, buffer, body_length);

    SendAck(message_id);
  }

  // If we have data available, and a read is pending, notify the callback.
  if (received_list_.bytes_available() && receive_complete_callback_) {
    // Pass the data up to the caller.
    int bytes_read = InternalRead(pending_receive_, pending_receive_length_);
    CompletionCallback* callback = receive_complete_callback_;
    receive_complete_callback_ = NULL;
    callback->Run(bytes_read);
  }
}

void Messenger::SendAck(uint32 last_message_received) {
  scoped_refptr<IOBuffer> buffer(new IOBuffer(sizeof(Message)));
  memset(buffer->data(), 0, sizeof(Message));
  Message* message = reinterpret_cast<Message*>(buffer->data());
  uint32_pack(message->last_message_received, last_message_received);
  uint64_pack(message->acknowledge_1, received_list_.bytes_received());
  LOG(ERROR) << "SendAck " << received_list_.bytes_received();
  // TODO(mbelshe): fill in remainder of selective acknowledgements

  // TODO(mbelshe): Fix this - it is totally possible to have a send message
  //                in progress here...
  DCHECK(!send_message_in_progress_);

  int rv = packetizer_->SendMessage(key_,
                                    buffer->data(),
                                    sizeof(Message),
                                    &send_message_callback_);
  // TODO(mbelshe): Fix me!  Deal with the error cases
  DCHECK(rv == sizeof(Message));
}

}  // namespace net
