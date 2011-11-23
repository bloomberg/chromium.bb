// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_CURVECP_MESSENGER_H_
#define NET_CURVECP_MESSENGER_H_
#pragma once

#include <deque>
#include <list>

#include "base/basictypes.h"
#include "base/task.h"
#include "base/threading/non_thread_safe.h"
#include "base/timer.h"
#include "net/base/completion_callback.h"
#include "net/curvecp/circular_buffer.h"
#include "net/curvecp/packetizer.h"
#include "net/curvecp/received_block_list.h"
#include "net/curvecp/rtt_and_send_rate_calculator.h"
#include "net/curvecp/sent_block_list.h"
#include "net/socket/socket.h"

namespace net {

class IOBufferWithSize;
class Packetizer;

// The messenger provides the reliable CurveCP transport.
class Messenger : public base::NonThreadSafe,
                  public Packetizer::Listener {
 public:
  explicit Messenger(Packetizer* packetizer);
  virtual ~Messenger();

  int Read(IOBuffer* buf, int buf_len, OldCompletionCallback* callback);
  int Write(IOBuffer* buf, int buf_len, OldCompletionCallback* callback);

  // Packetizer::Listener methods:
  virtual void OnConnection(ConnectionKey key) OVERRIDE;
  virtual void OnClose(Packetizer* packetizer, ConnectionKey key);
  virtual void OnMessage(Packetizer* packetizer,
                         ConnectionKey key,
                         unsigned char* msg,
                         size_t length) OVERRIDE;

 protected:
  ConnectionKey key_;

 private:
  // Handle reading data from the read queue.
  int InternalRead(IOBuffer* buffer, int buffer_length);

  // Extracts data from send queue to create a new buffer of data to send.
  IOBufferWithSize* CreateBufferFromSendQueue();

  // Continuation function after a SendMessage() call was blocked.
  void OnSendMessageComplete(int result);

  // Protocol handling routines
  void OnTimeout();
  void OnSendTimer();
  void SendMessage(int64 position);
  void RecvMessage();
  void SendAck(uint32 last_message_received);

  RttAndSendRateCalculator rtt_;
  Packetizer* packetizer_;

  // The send_buffer is a list of pending data to pack into messages and send
  // to the remote.
  CircularBuffer send_buffer_;
  OldCompletionCallback* send_complete_callback_;
  scoped_refptr<IOBuffer> pending_send_;
  int pending_send_length_;

  // The read_buffer is a list of pending data which has been unpacked from
  // messages and is awaiting delivery to the application.
  OldCompletionCallback* receive_complete_callback_;
  scoped_refptr<IOBuffer> pending_receive_;
  int pending_receive_length_;

  // The list of received but unprocessed messages.
  std::list<scoped_refptr<IOBufferWithSize> > read_queue_;

  ReceivedBlockList received_list_;
  SentBlockList sent_list_;
  bool send_message_in_progress_;

  // A timer to fire when a timeout has occurred.
  base::OneShotTimer<Messenger> send_timeout_timer_;
  // A timer to fire when we can send data.
  base::OneShotTimer<Messenger> send_timer_;

  OldCompletionCallbackImpl<Messenger> send_message_callback_;

  ScopedRunnableMethodFactory<Messenger> factory_;
  DISALLOW_COPY_AND_ASSIGN(Messenger);
};

}  // namespace net

#endif  // NET_CURVECP_MESSENGER_H_
