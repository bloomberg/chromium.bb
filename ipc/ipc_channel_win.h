// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPC_IPC_CHANNEL_WIN_H_
#define IPC_IPC_CHANNEL_WIN_H_
#pragma once

#include "ipc/ipc_channel.h"

#include <queue>
#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop.h"
#include "ipc/ipc_channel_reader.h"

namespace base {
class NonThreadSafe;
}

namespace IPC {

class Channel::ChannelImpl : public internal::ChannelReader,
                             public MessageLoopForIO::IOHandler {
 public:
  // Mirror methods of Channel, see ipc_channel.h for description.
  ChannelImpl(const IPC::ChannelHandle &channel_handle, Mode mode,
              Listener* listener);
  ~ChannelImpl();
  bool Connect();
  void Close();
  bool Send(Message* message);
  static bool IsNamedServerInitialized(const std::string& channel_id);

 private:
  // ChannelReader implementation.
  virtual ReadState ReadData(char* buffer,
                             int buffer_len,
                             int* bytes_read) OVERRIDE;
  virtual bool WillDispatchInputMessage(Message* msg) OVERRIDE;
  bool DidEmptyInputBuffers() OVERRIDE;
  virtual void HandleHelloMessage(const Message& msg) OVERRIDE;

  static const std::wstring PipeName(const std::string& channel_id);
  bool CreatePipe(const IPC::ChannelHandle &channel_handle, Mode mode);

  bool ProcessConnection();
  bool ProcessOutgoingMessages(MessageLoopForIO::IOContext* context,
                               DWORD bytes_written);

  // MessageLoop::IOHandler implementation.
  virtual void OnIOCompleted(MessageLoopForIO::IOContext* context,
                             DWORD bytes_transfered, DWORD error);
 private:
  struct State {
    explicit State(ChannelImpl* channel);
    ~State();
    MessageLoopForIO::IOContext context;
    bool is_pending;
  };

  State input_state_;
  State output_state_;

  HANDLE pipe_;

  // Messages to be sent are queued here.
  std::queue<Message*> output_queue_;

  // In server-mode, we have to wait for the client to connect before we
  // can begin reading.  We make use of the input_state_ when performing
  // the connect operation in overlapped mode.
  bool waiting_connect_;

  // This flag is set when processing incoming messages.  It is used to
  // avoid recursing through ProcessIncomingMessages, which could cause
  // problems.  TODO(darin): make this unnecessary
  bool processing_incoming_;

  base::WeakPtrFactory<ChannelImpl> weak_factory_;

  scoped_ptr<base::NonThreadSafe> thread_check_;

  DISALLOW_COPY_AND_ASSIGN(ChannelImpl);
};

}  // namespace IPC

#endif  // IPC_IPC_CHANNEL_WIN_H_
