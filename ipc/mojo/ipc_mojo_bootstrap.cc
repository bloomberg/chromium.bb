// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipc/mojo/ipc_mojo_bootstrap.h"

#include "base/logging.h"
#include "base/process/process_handle.h"
#include "ipc/ipc_message_utils.h"
#include "ipc/ipc_platform_file.h"
#include "mojo/embedder/platform_channel_pair.h"

namespace IPC {

namespace {

// MojoBootstrap for the server process. You should create the instance
// using MojoBootstrap::Create().
class IPC_MOJO_EXPORT MojoServerBootstrap : public MojoBootstrap {
 public:
  MojoServerBootstrap();

  virtual void OnClientLaunched(base::ProcessHandle process) OVERRIDE;

 private:
  void SendClientPipe();
  void SendClientPipeIfReady();

  // Listener implementations
  virtual bool OnMessageReceived(const Message& message) OVERRIDE;
  virtual void OnChannelConnected(int32 peer_pid) OVERRIDE;

  mojo::embedder::ScopedPlatformHandle server_pipe_;
  base::ProcessHandle client_process_;
  bool connected_;

  DISALLOW_COPY_AND_ASSIGN(MojoServerBootstrap);
};

MojoServerBootstrap::MojoServerBootstrap()
    : client_process_(base::kNullProcessHandle), connected_(false) {
}

void MojoServerBootstrap::SendClientPipe() {
  DCHECK_EQ(state(), STATE_INITIALIZED);
  DCHECK_NE(client_process_, base::kNullProcessHandle);
  DCHECK(connected_);

  mojo::embedder::PlatformChannelPair channel_pair;
  server_pipe_ = channel_pair.PassServerHandle();
  PlatformFileForTransit client_pipe = GetFileHandleForProcess(
#if defined(OS_POSIX)
      channel_pair.PassClientHandle().release().fd,
#else
      channel_pair.PassClientHandle().release().handle,
#endif
      client_process_,
      true);
  CHECK(client_pipe != IPC::InvalidPlatformFileForTransit());
  scoped_ptr<Message> message(new Message());
  ParamTraits<PlatformFileForTransit>::Write(message.get(), client_pipe);
  Send(message.release());

  set_state(STATE_WAITING_ACK);
}

void MojoServerBootstrap::SendClientPipeIfReady() {
  // Is the client launched?
  if (client_process_ == base::kNullProcessHandle)
    return;
  // Has the bootstrap channel been made?
  if (!connected_)
    return;
  SendClientPipe();
}

void MojoServerBootstrap::OnClientLaunched(base::ProcessHandle process) {
  DCHECK_EQ(state(), STATE_INITIALIZED);
  DCHECK_NE(process, base::kNullProcessHandle);
  client_process_ = process;
  SendClientPipeIfReady();
}

void MojoServerBootstrap::OnChannelConnected(int32 peer_pid) {
  DCHECK_EQ(state(), STATE_INITIALIZED);
  connected_ = true;
  SendClientPipeIfReady();
}

bool MojoServerBootstrap::OnMessageReceived(const Message&) {
  DCHECK_EQ(state(), STATE_WAITING_ACK);
  set_state(STATE_READY);

  delegate()->OnPipeAvailable(
      mojo::embedder::ScopedPlatformHandle(server_pipe_.release()));

  return true;
}

// MojoBootstrap for client processes. You should create the instance
// using MojoBootstrap::Create().
class IPC_MOJO_EXPORT MojoClientBootstrap : public MojoBootstrap {
 public:
  MojoClientBootstrap();

  virtual void OnClientLaunched(base::ProcessHandle process) OVERRIDE;

 private:
  // Listener implementations
  virtual bool OnMessageReceived(const Message& message) OVERRIDE;
  virtual void OnChannelConnected(int32 peer_pid) OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(MojoClientBootstrap);
};

MojoClientBootstrap::MojoClientBootstrap() {
}

bool MojoClientBootstrap::OnMessageReceived(const Message& message) {
  PlatformFileForTransit pipe;
  PickleIterator iter(message);
  if (!ParamTraits<PlatformFileForTransit>::Read(&message, &iter, &pipe)) {
    DLOG(WARNING) << "Failed to read a file handle from bootstrap channel.";
    message.set_dispatch_error();
    return false;
  }

  // Sends ACK back.
  Send(new Message());
  set_state(STATE_READY);
  delegate()->OnPipeAvailable(
      mojo::embedder::ScopedPlatformHandle(mojo::embedder::PlatformHandle(
          PlatformFileForTransitToPlatformFile(pipe))));

  return true;
}

void MojoClientBootstrap::OnClientLaunched(base::ProcessHandle process) {
  // This notification should happen only on server processes.
  NOTREACHED();
}

void MojoClientBootstrap::OnChannelConnected(int32 peer_pid) {
}

}  // namespace

// MojoBootstrap

// static
scoped_ptr<MojoBootstrap> MojoBootstrap::Create(ChannelHandle handle,
                                                Channel::Mode mode,
                                                Delegate* delegate) {
  CHECK(mode == Channel::MODE_CLIENT || mode == Channel::MODE_SERVER);
  scoped_ptr<MojoBootstrap> self =
      mode == Channel::MODE_CLIENT
          ? scoped_ptr<MojoBootstrap>(new MojoClientBootstrap())
          : scoped_ptr<MojoBootstrap>(new MojoServerBootstrap());
  scoped_ptr<Channel> bootstrap_channel =
      Channel::Create(handle, mode, self.get());
  self->Init(bootstrap_channel.Pass(), delegate);
  return self.Pass();
}

MojoBootstrap::MojoBootstrap() : delegate_(NULL), state_(STATE_INITIALIZED) {
}

MojoBootstrap::~MojoBootstrap() {
}

void MojoBootstrap::Init(scoped_ptr<Channel> channel, Delegate* delegate) {
  channel_ = channel.Pass();
  delegate_ = delegate;
}

bool MojoBootstrap::Connect() {
  return channel_->Connect();
}

void MojoBootstrap::OnBadMessageReceived(const Message& message) {
  delegate_->OnBootstrapError();
}

void MojoBootstrap::OnChannelError() {
  if (state_ == STATE_READY)
    return;
  DLOG(WARNING) << "Detected error on Mojo bootstrap channel.";
  delegate()->OnBootstrapError();
}

bool MojoBootstrap::Send(Message* message) {
  return channel_->Send(message);
}

#if defined(OS_POSIX) && !defined(OS_NACL)
int MojoBootstrap::GetClientFileDescriptor() const {
  return channel_->GetClientFileDescriptor();
}

int MojoBootstrap::TakeClientFileDescriptor() {
  return channel_->TakeClientFileDescriptor();
}
#endif  // defined(OS_POSIX) && !defined(OS_NACL)

}  // namespace IPC
