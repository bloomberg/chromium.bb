// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPC_IPC_CHANNEL_NACL_H_
#define IPC_IPC_CHANNEL_NACL_H_
#pragma once

#include "ipc/ipc_channel.h"
#include "ipc/ipc_channel_reader.h"

namespace IPC {

// Similar to the Posix version of ChannelImpl but for Native Client code.
// This is somewhat different because NaCl's send/recvmsg is slightly different
// and we don't need to worry about complicated set up and READWRITE mode for
// sharing handles.
class Channel::ChannelImpl : public internal::ChannelReader {
 public:
  ChannelImpl(const IPC::ChannelHandle& channel_handle,
              Mode mode,
              Listener* listener);
  virtual ~ChannelImpl();

  // Channel implementation.
  bool Connect();
  void Close();
  bool Send(Message* message);
  int GetClientFileDescriptor() const;
  int TakeClientFileDescriptor();
  bool AcceptsConnections() const;
  bool HasAcceptedConnection() const;
  bool GetClientEuid(uid_t* client_euid) const;
  void ResetToAcceptingConnectionState();
  static bool IsNamedServerInitialized(const std::string& channel_id);

  virtual ReadState ReadData(char* buffer,
                             int buffer_len,
                             int* bytes_read) OVERRIDE;
  virtual bool WillDispatchInputMessage(Message* msg) OVERRIDE;
  virtual bool DidEmptyInputBuffers() OVERRIDE;
  virtual void HandleHelloMessage(const Message& msg) OVERRIDE;

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(ChannelImpl);
};

}  // namespace IPC

#endif  // IPC_IPC_CHANNEL_NACL_H_
