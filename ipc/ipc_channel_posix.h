// Copyright (c) 2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPC_IPC_CHANNEL_POSIX_H_
#define IPC_IPC_CHANNEL_POSIX_H_
#pragma once

#include "ipc/ipc_channel.h"

#include <sys/socket.h>  // for CMSG macros

#include <queue>
#include <string>
#include <vector>

#include "base/message_loop.h"
#include "ipc/file_descriptor_set_posix.h"

#if !defined(OS_MACOSX)
// On Linux, the seccomp sandbox makes it very expensive to call
// recvmsg() and sendmsg(). The restriction on calling read() and write(), which
// are cheap, is that we can't pass file descriptors over them.
//
// As we cannot anticipate when the sender will provide us with file
// descriptors, we have to make the decision about whether we call read() or
// recvmsg() before we actually make the call. The easiest option is to
// create a dedicated socketpair() for exchanging file descriptors.
// Mac can also run in IPC_USES_READWRITE mode if necessary, but at this time
// doesn't take a performance hit from recvmsg and sendmsg, so it doesn't
// make sense to waste resources on having the separate dedicated socketpair.
// It is however useful for debugging between Linux and Mac to be able to turn
// this switch 'on' on the Mac as well.

// The HELLO message from the client to the server is always sent using
// sendmsg because it will contain the file descriptor that the server
// needs to send file descriptors in later messages.
#define IPC_USES_READWRITE 1
#endif

namespace IPC {

class Channel::ChannelImpl : public MessageLoopForIO::Watcher {
 public:
  // Mirror methods of Channel, see ipc_channel.h for description.
  ChannelImpl(const IPC::ChannelHandle& channel_handle, Mode mode,
              Listener* listener);
  ~ChannelImpl();
  bool Connect();
  void Close();
  void set_listener(Listener* listener) { listener_ = listener; }
  bool Send(Message* message);
  int GetClientFileDescriptor() const;
  bool AcceptsConnections() const;
  bool HasAcceptedConnection() const;
  void ResetToAcceptingConnectionState();

 private:
  bool CreatePipe(const IPC::ChannelHandle& channel_handle);

  bool ProcessIncomingMessages();
  bool ProcessOutgoingMessages();

  bool AcceptConnection();
  void ClosePipeOnError();
  void QueueHelloMessage();
  bool IsHelloMessage(const Message* m) const;

  // MessageLoopForIO::Watcher implementation.
  virtual void OnFileCanReadWithoutBlocking(int fd);
  virtual void OnFileCanWriteWithoutBlocking(int fd);

  Mode mode_;

  // After accepting one client connection on our server socket we want to
  // stop listening.
  MessageLoopForIO::FileDescriptorWatcher server_listen_connection_watcher_;
  MessageLoopForIO::FileDescriptorWatcher read_watcher_;
  MessageLoopForIO::FileDescriptorWatcher write_watcher_;

  // Indicates whether we're currently blocked waiting for a write to complete.
  bool is_blocked_on_write_;
  bool waiting_connect_;

  // If sending a message blocks then we use this variable
  // to keep track of where we are.
  size_t message_send_bytes_written_;

  // File descriptor we're listening on for new connections if we listen
  // for connections.
  int server_listen_pipe_;

  // The pipe used for communication.
  int pipe_;

  // For a server, the client end of our socketpair() -- the other end of our
  // pipe_ that is passed to the client.
  int client_pipe_;

#if defined(IPC_USES_READWRITE)
  // Linux/BSD use a dedicated socketpair() for passing file descriptors.
  int fd_pipe_;
  int remote_fd_pipe_;
#endif

  // The "name" of our pipe.  On Windows this is the global identifier for
  // the pipe.  On POSIX it's used as a key in a local map of file descriptors.
  std::string pipe_name_;

  Listener* listener_;

  // Messages to be sent are queued here.
  std::queue<Message*> output_queue_;

  // We read from the pipe into this buffer
  char input_buf_[Channel::kReadBufferSize];

  enum {
    // We assume a worst case: kReadBufferSize bytes of messages, where each
    // message has no payload and a full complement of descriptors.
    MAX_READ_FDS = (Channel::kReadBufferSize / sizeof(IPC::Message::Header)) *
                   FileDescriptorSet::MAX_DESCRIPTORS_PER_MESSAGE,
  };

  // This is a control message buffer large enough to hold kMaxReadFDs
#if defined(OS_MACOSX)
  // TODO(agl): OSX appears to have non-constant CMSG macros!
  char input_cmsg_buf_[1024];
#else
  char input_cmsg_buf_[CMSG_SPACE(sizeof(int) * MAX_READ_FDS)];
#endif

  // Large messages that span multiple pipe buffers, get built-up using
  // this buffer.
  std::string input_overflow_buf_;
  std::vector<int> input_overflow_fds_;

  // True if we are responsible for unlinking the unix domain socket file.
  bool must_unlink_;

  ScopedRunnableMethodFactory<ChannelImpl> factory_;

  DISALLOW_COPY_AND_ASSIGN(ChannelImpl);
};

// The maximum length of the name of a pipe for MODE_NAMED_SERVER or
// MODE_NAMED_CLIENT if you want to pass in your own socket.
// The standard size on linux is 108, mac is 104. To maintain consistency
// across platforms we standardize on the smaller value.
static const size_t kMaxPipeNameLength = 104;

}  // namespace IPC

#endif  // IPC_IPC_CHANNEL_POSIX_H_
