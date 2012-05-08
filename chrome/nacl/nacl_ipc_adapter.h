// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_NACL_NACL_IPC_ADAPTER_H_
#define CHROME_NACL_NACL_IPC_ADAPTER_H_
#pragma once

#include <queue>
#include <string>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/synchronization/condition_variable.h"
#include "base/synchronization/lock.h"
#include "base/task_runner.h"
#include "ipc/ipc_channel.h"

// Adapts a Chrome IPC channel to an IPC channel that we expose to Native
// Client. This provides a mapping in both directions, so when IPC messages
// come in from another process, we rewrite them and allow them to be received
// via a recvmsg-like interface in the NaCl code. When NaCl code calls sendmsg,
// we implement that as sending IPC messages on the channel.
//
// This object also provides the necessary logic for rewriting IPC messages.
// NaCl code is platform-independent and runs in a Posix-like enviroment, but
// some formatting in the message and the way handles are transferred varies
// by platform. This class bridges that gap to provide what looks like a
// normal platform-specific IPC implementation to Chrome, and a Posix-like
// version on every platform to NaCl.
//
// This object must be threadsafe since the nacl environment determines which
// thread every function is called on.
class NaClIPCAdapter : public base::RefCountedThreadSafe<NaClIPCAdapter>,
                       public IPC::Channel::Listener {
 public:
  // Chrome's IPC message format varies by platform, NaCl's does not. In
  // particular, the header has some extra fields on Posix platforms. Since
  // NaCl is a Posix environment, it gets that version of the header. This
  // header is duplicated here so we have a cross-platform definition of the
  // header we're exposing to NaCl.
#pragma pack(push, 4)
  struct NaClMessageHeader : public Pickle::Header {
    int32 routing;
    uint32 type;
    uint32 flags;
    uint16 num_fds;
    uint16 pad;
  };
#pragma pack(pop)

  // Creates an adapter, using the thread associated with the given task
  // runner for posting messages. In normal use, the task runner will post to
  // the I/O thread of the process.
  NaClIPCAdapter(const IPC::ChannelHandle& handle, base::TaskRunner* runner);

  // Initializes with a given channel that's already created for testing
  // purposes. This function will take ownership of the given channel.
  NaClIPCAdapter(scoped_ptr<IPC::Channel> channel, base::TaskRunner* runner);

  // Implementation of sendmsg. Returns the number of bytes written or -1 on
  // failure.
  int Send(const char* input_data, size_t input_data_len);

  // Implementation of recvmsg. Returns the number of bytes read or -1 on
  // failure. This will block until there's an error or there is data to
  // read.
  int BlockingReceive(char* output_buffer, int output_buffer_size);

  // Closes the IPC channel.
  void CloseChannel();

  // Listener implementation.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;
  virtual void OnChannelConnected(int32 peer_pid) OVERRIDE;
  virtual void OnChannelError() OVERRIDE;

 private:
  friend class base::RefCountedThreadSafe<NaClIPCAdapter>;

  class RewrittenMessage;

  // This is the data that must only be accessed inside the lock. This struct
  // just separates it so it's easier to see.
  struct LockedData {
    LockedData();
    ~LockedData();

    // Messages that we have read off of the Chrome IPC channel that are waiting
    // to be received by the plugin.
    std::queue< scoped_refptr<RewrittenMessage> > to_be_received_;

    // Data that we've queued from the plugin to send, but doesn't consist of a
    // full message yet. The calling code can break apart the message into
    // smaller pieces, and we need to send the message to the other process in
    // one chunk.
    //
    // The IPC channel always starts a new send() at the beginning of each
    // message, so we don't need to worry about arbitrary message boundaries.
    std::string to_be_sent_;

    bool channel_closed_;
  };

  // This is the data that must only be accessed on the I/O thread (as defined
  // by TaskRunner). This struct just separates it so it's easier to see.
  struct IOThreadData {
    IOThreadData();
    ~IOThreadData();

    scoped_ptr<IPC::Channel> channel_;
  };

  virtual ~NaClIPCAdapter();

  // Reads up to the given amount of data. Returns 0 if nothing is waiting.
  int LockedReceive(char* output_buffer, int output_buffer_size);

  // Sends a message that we know has been completed to the Chrome process.
  bool SendCompleteMessage(const char* buffer, size_t buffer_len);

  // Clears the LockedData.to_be_sent_ structure in a way to make sure that
  // the memory is deleted. std::string can sometimes hold onto the buffer
  // for future use which we don't want.
  void ClearToBeSent();

  void CreateChannelOnIOThread(const IPC::ChannelHandle& handle);
  void CloseChannelOnIOThread();
  void SendMessageOnIOThread(scoped_ptr<IPC::Message> message);

  base::Lock lock_;
  base::ConditionVariable cond_var_;

  scoped_refptr<base::TaskRunner> task_runner_;

  // To be accessed inside of lock_ only.
  LockedData locked_data_;

  // To be accessed on the I/O thread (via task runner) only.
  IOThreadData io_thread_data_;

  DISALLOW_COPY_AND_ASSIGN(NaClIPCAdapter);
};

#endif  // CHROME_NACL_NACL_IPC_ADAPTER_H_
