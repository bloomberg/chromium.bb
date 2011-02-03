// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPC_IPC_CHANNEL_H_
#define IPC_IPC_CHANNEL_H_
#pragma once

#include "base/compiler_specific.h"
#include "ipc/ipc_channel_handle.h"
#include "ipc/ipc_message.h"

namespace IPC {

//------------------------------------------------------------------------------
// See
// http://www.chromium.org/developers/design-documents/inter-process-communication
// for overview of IPC in Chromium.

// Channels are implemented using named pipes on Windows, and
// socket pairs (or in some special cases unix domain sockets) on POSIX.
// On Windows we access pipes in various processes by name.
// On POSIX we pass file descriptors to child processes and assign names to them
// in a lookup table.
// In general on POSIX we do not use unix domain sockets due to security
// concerns and the fact that they can leave garbage around the file system
// (MacOS does not support abstract named unix domain sockets).
// You can use unix domain sockets if you like on POSIX by constructing the
// the channel with the mode set to one of the NAMED modes. NAMED modes are
// currently used by automation and service processes.

class Channel : public Message::Sender {
  // Security tests need access to the pipe handle.
  friend class ChannelTest;

 public:
  // Implemented by consumers of a Channel to receive messages.
  class Listener {
   public:
    virtual ~Listener() {}

    // Called when a message is received.  Returns true iff the message was
    // handled.
    virtual bool OnMessageReceived(const Message& message) = 0;

    // Called when the channel is connected and we have received the internal
    // Hello message from the peer.
    virtual void OnChannelConnected(int32 peer_pid) {}

    // Called when an error is detected that causes the channel to close.
    // This method is not called when a channel is closed normally.
    virtual void OnChannelError() {}

#if defined(OS_POSIX)
    // Called on the server side when a channel that listens for connections
    // denies an attempt to connect.
    virtual void OnChannelDenied() {}

    // Called on the server side when a channel that listens for connections
    // has an error that causes the listening channel to close.
    virtual void OnChannelListenError() {}
#endif  // OS_POSIX
  };

  // Flags to test modes
  enum ModeFlags {
    MODE_NO_FLAG = 0x0,
    MODE_SERVER_FLAG = 0x1,
    MODE_CLIENT_FLAG = 0x2,
    MODE_NAMED_FLAG = 0x4
  };

  // Some Standard Modes
  enum Mode {
    MODE_NONE = MODE_NO_FLAG,
    MODE_SERVER = MODE_SERVER_FLAG,
    MODE_CLIENT = MODE_CLIENT_FLAG,
    // Channels on Windows are named by default and accessible from other
    // processes. On POSIX channels are anonymous by default and not accessible
    // from other processes. Named channels work via named unix domain sockets.
    // On Windows MODE_NAMED_SERVER is equivalent to MODE_SERVER and
    // MODE_NAMED_CLIENT is equivalent to MODE_CLIENT.
    MODE_NAMED_SERVER = MODE_SERVER_FLAG | MODE_NAMED_FLAG,
    MODE_NAMED_CLIENT = MODE_CLIENT_FLAG | MODE_NAMED_FLAG,
  };

  enum {
    // The maximum message size in bytes. Attempting to receive a
    // message of this size or bigger results in a channel error.
    kMaximumMessageSize = 128 * 1024 * 1024,

    // Ammount of data to read at once from the pipe.
    kReadBufferSize = 4 * 1024
  };

  // Initialize a Channel.
  //
  // |channel_handle| identifies the communication Channel. For POSIX, if
  // the file descriptor in the channel handle is != -1, the channel takes
  // ownership of the file descriptor and will close it appropriately, otherwise
  // it will create a new descriptor internally.
  // |mode| specifies whether this Channel is to operate in server mode or
  // client mode.  In server mode, the Channel is responsible for setting up the
  // IPC object, whereas in client mode, the Channel merely connects to the
  // already established IPC object.
  // |listener| receives a callback on the current thread for each newly
  // received message.
  //
  Channel(const IPC::ChannelHandle &channel_handle, Mode mode,
          Listener* listener);

  ~Channel();

  // Connect the pipe.  On the server side, this will initiate
  // waiting for connections.  On the client, it attempts to
  // connect to a pre-existing pipe.  Note, calling Connect()
  // will not block the calling thread and may complete
  // asynchronously.
  bool Connect() WARN_UNUSED_RESULT;

  // Close this Channel explicitly.  May be called multiple times.
  // On POSIX calling close on an IPC channel that listens for connections will
  // cause it to close any accepted connections, and it will stop listening for
  // new connections. If you just want to close the currently accepted
  // connection and listen for new ones, use ResetToAcceptingConnectionState.
  void Close();

  // Modify the Channel's listener.
  void set_listener(Listener* listener);

  // Send a message over the Channel to the listener on the other end.
  //
  // |message| must be allocated using operator new.  This object will be
  // deleted once the contents of the Message have been sent.
  virtual bool Send(Message* message);

#if defined(OS_POSIX) && !defined(OS_NACL)
  // On POSIX an IPC::Channel wraps a socketpair(), this method returns the
  // FD # for the client end of the socket.
  // This method may only be called on the server side of a channel.
  int GetClientFileDescriptor() const;

  // On POSIX an IPC::Channel can either wrap an established socket, or it
  // can wrap a socket that is listening for connections. Currently an
  // IPC::Channel that listens for connections can only accept one connection
  // at a time.

  // Returns true if the channel supports listening for connections.
  bool AcceptsConnections() const;

  // Returns true if the channel supports listening for connections and is
  // currently connected.
  bool HasAcceptedConnection() const;

  // Closes any currently connected socket, and returns to a listening state
  // for more connections.
  void ResetToAcceptingConnectionState();
#endif  // defined(OS_POSIX)

 protected:
  // Used in Chrome by the TestSink to provide a dummy channel implementation
  // for testing. TestSink overrides the "interesting" functions in Channel so
  // no actual implementation is needed. This will cause un-overridden calls to
  // segfault. Do not use outside of test code!
  Channel() : channel_impl_(0) { }

 private:
  // PIMPL to which all channel calls are delegated.
  class ChannelImpl;
  ChannelImpl *channel_impl_;

  // The Hello message is internal to the Channel class.  It is sent
  // by the peer when the channel is connected.  The message contains
  // just the process id (pid).  The message has a special routing_id
  // (MSG_ROUTING_NONE) and type (HELLO_MESSAGE_TYPE).
  enum {
    HELLO_MESSAGE_TYPE = kuint16max  // Maximum value of message type (uint16),
                                     // to avoid conflicting with normal
                                     // message types, which are enumeration
                                     // constants starting from 0.
  };
};

}  // namespace IPC

#endif  // IPC_IPC_CHANNEL_H_
