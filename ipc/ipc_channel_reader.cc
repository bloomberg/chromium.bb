// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipc/ipc_channel_reader.h"

namespace IPC {
namespace internal {

ChannelReader::ChannelReader(Listener* listener) : listener_(listener) {
  memset(input_buf_, 0, sizeof(input_buf_));
}

ChannelReader::~ChannelReader() {
}

bool ChannelReader::ProcessIncomingMessages() {
  while (true) {
    int bytes_read = 0;
    ReadState read_state = ReadData(input_buf_, Channel::kReadBufferSize,
                                    &bytes_read);
    if (read_state == READ_FAILED)
      return false;
    if (read_state == READ_PENDING)
      return true;

    DCHECK(bytes_read > 0);
    if (!DispatchInputData(input_buf_, bytes_read))
      return false;
  }
}

bool ChannelReader::AsyncReadComplete(int bytes_read) {
  return DispatchInputData(input_buf_, bytes_read);
}

bool ChannelReader::IsHelloMessage(const Message& m) const {
  return m.routing_id() == MSG_ROUTING_NONE &&
         m.type() == Channel::HELLO_MESSAGE_TYPE;
}

bool ChannelReader::DispatchInputData(const char* input_data,
                                      int input_data_len) {
  const char* p;
  const char* end;

  // Possibly combine with the overflow buffer to make a larger buffer.
  if (input_overflow_buf_.empty()) {
    p = input_data;
    end = input_data + input_data_len;
  } else {
    if (input_overflow_buf_.size() >
        Channel::kMaximumMessageSize - input_data_len) {
      input_overflow_buf_.clear();
      LOG(ERROR) << "IPC message is too big";
      return false;
    }
    input_overflow_buf_.append(input_data, input_data_len);
    p = input_overflow_buf_.data();
    end = p + input_overflow_buf_.size();
  }

  // Dispatch all complete messages in the data buffer.
  while (p < end) {
    const char* message_tail = Message::FindNext(p, end);
    if (message_tail) {
      int len = static_cast<int>(message_tail - p);
      Message m(p, len);
      if (!WillDispatchInputMessage(&m))
        return false;

      m.TraceMessageStep();
      if (IsHelloMessage(m))
        HandleHelloMessage(m);
      else
        listener_->OnMessageReceived(m);
      p = message_tail;
    } else {
      // Last message is partial.
      break;
    }
  }

  // Save any partial data in the overflow buffer.
  input_overflow_buf_.assign(p, end - p);

  if (input_overflow_buf_.empty() && !DidEmptyInputBuffers())
    return false;
  return true;
}


}  // namespace internal
}  // namespace IPC
