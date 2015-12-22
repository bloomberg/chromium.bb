// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPC_IPC_CHANNEL_MOJO_H_
#define IPC_IPC_CHANNEL_MOJO_H_

#include <stdint.h>

#include <vector>

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/memory/weak_ptr.h"
#include "base/synchronization/lock.h"
#include "build/build_config.h"
#include "ipc/ipc_channel.h"
#include "ipc/ipc_channel_factory.h"
#include "ipc/ipc_export.h"
#include "ipc/mojo/ipc_message_pipe_reader.h"
#include "ipc/mojo/ipc_mojo_bootstrap.h"
#include "ipc/mojo/scoped_ipc_support.h"
#include "mojo/public/cpp/system/core.h"
#include "third_party/mojo/src/mojo/edk/embedder/channel_info_forward.h"

namespace IPC {

// Mojo-based IPC::Channel implementation over a platform handle.
//
// ChannelMojo builds Mojo MessagePipe using underlying pipe given by
// "bootstrap" IPC::Channel which creates and owns platform pipe like
// named socket. The bootstrap Channel is used only for establishing
// the underlying connection. ChannelMojo takes its handle over once
// the it is made and puts MessagePipe on it.
//
// ChannelMojo has a couple of MessagePipes:
//
// * The first MessagePipe, which is built on top of bootstrap handle,
//   is the "control" pipe. It is used to communicate out-of-band
//   control messages that aren't visible from IPC::Listener.
//
// * The second MessagePipe, which is created by the server channel
//   and sent to client Channel over the control pipe, is used
//   to send IPC::Messages as an IPC::Sender.
//
// TODO(morrita): Extract handle creation part of IPC::Channel into
//                separate class to clarify what ChannelMojo relies
//                on.
// TODO(morrita): Add APIs to create extra MessagePipes to let
//                Mojo-based objects talk over this Channel.
//
class IPC_MOJO_EXPORT ChannelMojo
    : public Channel,
      public MojoBootstrap::Delegate,
      public NON_EXPORTED_BASE(internal::MessagePipeReader::Delegate) {
 public:
  using CreateMessagingPipeCallback =
      base::Callback<void(mojo::ScopedMessagePipeHandle)>;
  using CreateMessagingPipeOnIOThreadCallback =
      base::Callback<void(mojo::ScopedMessagePipeHandle,
                          mojo::embedder::ChannelInfo*)>;

  // True if ChannelMojo should be used regardless of the flag.
  static bool ShouldBeUsed();

  // Create ChannelMojo. A bootstrap channel is created as well.
  static scoped_ptr<ChannelMojo> Create(
      scoped_refptr<base::TaskRunner> io_runner,
      const ChannelHandle& channel_handle,
      Mode mode,
      Listener* listener);

  // Create a factory object for ChannelMojo.
  // The factory is used to create Mojo-based ChannelProxy family.
  // |host| must not be null.
  static scoped_ptr<ChannelFactory> CreateServerFactory(
      scoped_refptr<base::TaskRunner> io_runner,
      const ChannelHandle& channel_handle);

  static scoped_ptr<ChannelFactory> CreateClientFactory(
      scoped_refptr<base::TaskRunner> io_runner,
      const ChannelHandle& channel_handle);

  ~ChannelMojo() override;

  // Channel implementation
  bool Connect() override;
  void Close() override;
  bool Send(Message* message) override;
  bool IsSendThreadSafe() const override;
  base::ProcessId GetPeerPID() const override;
  base::ProcessId GetSelfPID() const override;

#if defined(OS_POSIX) && !defined(OS_NACL)
  int GetClientFileDescriptor() const override;
  base::ScopedFD TakeClientFileDescriptor() override;
#endif  // defined(OS_POSIX) && !defined(OS_NACL)

  // These access protected API of IPC::Message, which has ChannelMojo
  // as a friend class.
  static MojoResult WriteToMessageAttachmentSet(
      const std::vector<MojoHandle>& handle_buffer,
      Message* message);
  static MojoResult ReadFromMessageAttachmentSet(
      Message* message,
      std::vector<MojoHandle>* handles);

  // MojoBootstrapDelegate implementation
  void OnBootstrapError() override;

  // MessagePipeReader::Delegate
  void OnMessageReceived(Message& message) override;
  void OnPipeClosed(internal::MessagePipeReader* reader) override;
  void OnPipeError(internal::MessagePipeReader* reader) override;

 protected:
  ChannelMojo(scoped_refptr<base::TaskRunner> io_runner,
              const ChannelHandle& channel_handle,
              Mode mode,
              Listener* listener);

  void CreateMessagingPipe(mojo::embedder::ScopedPlatformHandle handle,
                           const CreateMessagingPipeCallback& callback);
  void InitMessageReader(mojo::ScopedMessagePipeHandle pipe, int32_t peer_pid);

  Listener* listener() const { return listener_; }
  void set_peer_pid(base::ProcessId pid) { peer_pid_ = pid; }

 private:
  struct ChannelInfoDeleter {
    explicit ChannelInfoDeleter(scoped_refptr<base::TaskRunner> io_runner);
    ~ChannelInfoDeleter();

    void operator()(mojo::embedder::ChannelInfo* ptr) const;

    scoped_refptr<base::TaskRunner> io_runner;
  };

  // ChannelMojo needs to kill its MessagePipeReader in delayed manner
  // because the channel wants to kill these readers during the
  // notifications invoked by them.
  typedef internal::MessagePipeReader::DelayedDeleter ReaderDeleter;

  void InitOnIOThread();

  static void CreateMessagingPipeOnIOThread(
      mojo::embedder::ScopedPlatformHandle handle,
      scoped_refptr<base::TaskRunner> callback_runner,
      const CreateMessagingPipeOnIOThreadCallback& callback);
  void OnMessagingPipeCreated(const CreateMessagingPipeCallback& callback,
                              mojo::ScopedMessagePipeHandle handle,
                              mojo::embedder::ChannelInfo* channel_info);

  scoped_ptr<MojoBootstrap> bootstrap_;
  Listener* listener_;
  base::ProcessId peer_pid_;
  scoped_refptr<base::TaskRunner> io_runner_;
  scoped_ptr<mojo::embedder::ChannelInfo,
             ChannelInfoDeleter> channel_info_;

  // Guards |message_reader_|, |waiting_connect_| and |pending_messages_|
  //
  // * The contents of |pending_messages_| can be modified from any thread.
  // * |message_reader_| is modified only from the IO thread,
  //   but they can be referenced from other threads.
  base::Lock lock_;
  scoped_ptr<internal::MessagePipeReader, ReaderDeleter> message_reader_;
  ScopedVector<Message> pending_messages_;
  bool waiting_connect_;

  scoped_ptr<ScopedIPCSupport> ipc_support_;

  base::WeakPtrFactory<ChannelMojo> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ChannelMojo);
};

}  // namespace IPC

#endif  // IPC_IPC_CHANNEL_MOJO_H_
