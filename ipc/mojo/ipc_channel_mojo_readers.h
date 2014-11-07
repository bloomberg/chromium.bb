// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPC_MOJO_IPC_CHANNEL_MOJO_READERS_H_
#define IPC_MOJO_IPC_CHANNEL_MOJO_READERS_H_

#include <vector>

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "ipc/mojo/ipc_message_pipe_reader.h"
#include "mojo/public/cpp/system/core.h"

namespace IPC {

class ChannelMojo;
class Message;

namespace internal {

// A MessagePipeReader implementation for IPC::Message communication.
class MessageReader : public MessagePipeReader {
 public:
  MessageReader(mojo::ScopedMessagePipeHandle pipe, ChannelMojo* owner);

  bool Send(scoped_ptr<Message> message);

  // MessagePipeReader implementation
  void OnMessageReceived() override;
  void OnPipeClosed() override;
  void OnPipeError(MojoResult error) override;

 private:
  ChannelMojo* owner_;

  DISALLOW_COPY_AND_ASSIGN(MessageReader);
};

// MessagePipeReader implementation for control messages.
// Actual message handling is implemented by sublcasses.
class ControlReader : public MessagePipeReader {
 public:
  ControlReader(mojo::ScopedMessagePipeHandle pipe, ChannelMojo* owner);

  virtual bool Connect();

  // MessagePipeReader implementation
  void OnPipeClosed() override;
  void OnPipeError(MojoResult error) override;

 protected:
  ChannelMojo* owner_;

  DISALLOW_COPY_AND_ASSIGN(ControlReader);
};

// ControlReader for server-side ChannelMojo.
class ServerControlReader : public ControlReader {
 public:
  ServerControlReader(mojo::ScopedMessagePipeHandle pipe, ChannelMojo* owner);
  ~ServerControlReader() override;

  // ControlReader override
  bool Connect() override;

  // MessagePipeReader implementation
  void OnMessageReceived() override;

 private:
  MojoResult SendHelloRequest();
  MojoResult RespondHelloResponse();

  mojo::ScopedMessagePipeHandle message_pipe_;

  DISALLOW_COPY_AND_ASSIGN(ServerControlReader);
};

// ControlReader for client-side ChannelMojo.
class ClientControlReader : public ControlReader {
 public:
  ClientControlReader(mojo::ScopedMessagePipeHandle pipe, ChannelMojo* owner);

  // MessagePipeReader implementation
  void OnMessageReceived() override;

 private:
  MojoResult RespondHelloRequest(MojoHandle message_channel);

  DISALLOW_COPY_AND_ASSIGN(ClientControlReader);
};

}  // namespace internal
}  // namespace IPC

#endif  // IPC_MOJO_IPC_CHANNEL_MOJO_READERS_H_
