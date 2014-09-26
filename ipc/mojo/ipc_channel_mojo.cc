// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipc/mojo/ipc_channel_mojo.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/lazy_instance.h"
#include "ipc/ipc_listener.h"
#include "ipc/mojo/ipc_channel_mojo_readers.h"
#include "ipc/mojo/ipc_mojo_bootstrap.h"
#include "mojo/embedder/embedder.h"

#if defined(OS_POSIX) && !defined(OS_NACL)
#include "ipc/file_descriptor_set_posix.h"
#endif

namespace IPC {

namespace {

class MojoChannelFactory : public ChannelFactory {
 public:
  MojoChannelFactory(ChannelMojo::Delegate* delegate,
                     ChannelHandle channel_handle,
                     Channel::Mode mode)
      : delegate_(delegate), channel_handle_(channel_handle), mode_(mode) {}

  virtual std::string GetName() const OVERRIDE {
    return channel_handle_.name;
  }

  virtual scoped_ptr<Channel> BuildChannel(Listener* listener) OVERRIDE {
    return ChannelMojo::Create(delegate_, channel_handle_, mode_, listener)
        .PassAs<Channel>();
  }

 private:
  ChannelMojo::Delegate* delegate_;
  ChannelHandle channel_handle_;
  Channel::Mode mode_;
};

} // namespace

//------------------------------------------------------------------------------

void ChannelMojo::ChannelInfoDeleter::operator()(
    mojo::embedder::ChannelInfo* ptr) const {
  mojo::embedder::DestroyChannelOnIOThread(ptr);
}

//------------------------------------------------------------------------------

// static
scoped_ptr<ChannelMojo> ChannelMojo::Create(ChannelMojo::Delegate* delegate,
                                            const ChannelHandle& channel_handle,
                                            Mode mode,
                                            Listener* listener) {
  return make_scoped_ptr(
      new ChannelMojo(delegate, channel_handle, mode, listener));
}

// static
scoped_ptr<ChannelFactory> ChannelMojo::CreateServerFactory(
    ChannelMojo::Delegate* delegate,
    const ChannelHandle& channel_handle) {
  return make_scoped_ptr(new MojoChannelFactory(
                             delegate, channel_handle, Channel::MODE_SERVER))
      .PassAs<ChannelFactory>();
}

// static
scoped_ptr<ChannelFactory> ChannelMojo::CreateClientFactory(
    const ChannelHandle& channel_handle) {
  return make_scoped_ptr(
             new MojoChannelFactory(NULL, channel_handle, Channel::MODE_CLIENT))
      .PassAs<ChannelFactory>();
}

ChannelMojo::ChannelMojo(ChannelMojo::Delegate* delegate,
                         const ChannelHandle& handle,
                         Mode mode,
                         Listener* listener)
    : mode_(mode),
      listener_(listener),
      peer_pid_(base::kNullProcessId),
      weak_factory_(this) {
  // Create MojoBootstrap after all members are set as it touches
  // ChannelMojo from a different thread.
  bootstrap_ = MojoBootstrap::Create(handle, mode, this);
  if (delegate) {
    if (delegate->GetIOTaskRunner() ==
        base::MessageLoop::current()->message_loop_proxy()) {
      InitDelegate(delegate);
    } else {
      delegate->GetIOTaskRunner()->PostTask(
          FROM_HERE,
          base::Bind(
              &ChannelMojo::InitDelegate, base::Unretained(this), delegate));
    }
  }
}

ChannelMojo::~ChannelMojo() {
  Close();
}

void ChannelMojo::InitDelegate(ChannelMojo::Delegate* delegate) {
  delegate_ = delegate->ToWeakPtr();
  delegate_->OnChannelCreated(weak_factory_.GetWeakPtr());
}

void ChannelMojo::InitControlReader(
    mojo::embedder::ScopedPlatformHandle handle) {
  DCHECK(base::MessageLoopForIO::IsCurrent());
  mojo::embedder::ChannelInfo* channel_info;
  mojo::ScopedMessagePipeHandle control_pipe =
      mojo::embedder::CreateChannelOnIOThread(handle.Pass(), &channel_info);
  channel_info_.reset(channel_info);

  switch (mode_) {
    case MODE_SERVER:
      control_reader_.reset(
          new internal::ServerControlReader(control_pipe.Pass(), this));
      break;
    case MODE_CLIENT:
      control_reader_.reset(
          new internal::ClientControlReader(control_pipe.Pass(), this));
      break;
    default:
      NOTREACHED();
      break;
  }
}

bool ChannelMojo::Connect() {
  DCHECK(!message_reader_);
  DCHECK(!control_reader_);
  return bootstrap_->Connect();
}

void ChannelMojo::Close() {
  control_reader_.reset();
  message_reader_.reset();
  channel_info_.reset();
}

void ChannelMojo::OnPipeAvailable(mojo::embedder::ScopedPlatformHandle handle) {
  InitControlReader(handle.Pass());
  control_reader_->Connect();
}

void ChannelMojo::OnBootstrapError() {
  listener_->OnChannelError();
}

void ChannelMojo::OnConnected(mojo::ScopedMessagePipeHandle pipe) {
  message_reader_ =
      make_scoped_ptr(new internal::MessageReader(pipe.Pass(), this));

  for (size_t i = 0; i < pending_messages_.size(); ++i) {
    bool sent = message_reader_->Send(make_scoped_ptr(pending_messages_[i]));
    pending_messages_[i] = NULL;
    if (!sent) {
      pending_messages_.clear();
      listener_->OnChannelError();
      return;
    }
  }

  pending_messages_.clear();

  listener_->OnChannelConnected(GetPeerPID());
}

void ChannelMojo::OnPipeClosed(internal::MessagePipeReader* reader) {
  Close();
}

void ChannelMojo::OnPipeError(internal::MessagePipeReader* reader) {
  listener_->OnChannelError();
}


bool ChannelMojo::Send(Message* message) {
  if (!message_reader_) {
    pending_messages_.push_back(message);
    return true;
  }

  return message_reader_->Send(make_scoped_ptr(message));
}

base::ProcessId ChannelMojo::GetPeerPID() const {
  return peer_pid_;
}

base::ProcessId ChannelMojo::GetSelfPID() const {
  return base::GetCurrentProcId();
}

void ChannelMojo::OnClientLaunched(base::ProcessHandle handle) {
  bootstrap_->OnClientLaunched(handle);
}

void ChannelMojo::OnMessageReceived(Message& message) {
  listener_->OnMessageReceived(message);
  if (message.dispatch_error())
    listener_->OnBadMessageReceived(message);
}

#if defined(OS_POSIX) && !defined(OS_NACL)
int ChannelMojo::GetClientFileDescriptor() const {
  return bootstrap_->GetClientFileDescriptor();
}

int ChannelMojo::TakeClientFileDescriptor() {
  return bootstrap_->TakeClientFileDescriptor();
}

// static
MojoResult ChannelMojo::WriteToFileDescriptorSet(
    const std::vector<MojoHandle>& handle_buffer,
    Message* message) {
  for (size_t i = 0; i < handle_buffer.size(); ++i) {
    mojo::embedder::ScopedPlatformHandle platform_handle;
    MojoResult unwrap_result = mojo::embedder::PassWrappedPlatformHandle(
        handle_buffer[i], &platform_handle);
    if (unwrap_result != MOJO_RESULT_OK) {
      DLOG(WARNING) << "Pipe failed to covert handles. Closing: "
                    << unwrap_result;
      return unwrap_result;
    }

    bool ok = message->file_descriptor_set()->AddToOwn(
        base::ScopedFD(platform_handle.release().fd));
    DCHECK(ok);
  }

  return MOJO_RESULT_OK;
}

// static
MojoResult ChannelMojo::ReadFromFileDescriptorSet(
    Message* message,
    std::vector<MojoHandle>* handles) {
  // We dup() the handles in IPC::Message to transmit.
  // IPC::FileDescriptorSet has intricate lifecycle semantics
  // of FDs, so just to dup()-and-own them is the safest option.
  if (message->HasFileDescriptors()) {
    FileDescriptorSet* fdset = message->file_descriptor_set();
    std::vector<base::PlatformFile> fds_to_send(fdset->size());
    fdset->PeekDescriptors(&fds_to_send[0]);
    for (size_t i = 0; i < fds_to_send.size(); ++i) {
      int fd_to_send = dup(fds_to_send[i]);
      if (-1 == fd_to_send) {
        DPLOG(WARNING) << "Failed to dup FD to transmit.";
        fdset->CommitAll();
        return MOJO_RESULT_UNKNOWN;
      }

      MojoHandle wrapped_handle;
      MojoResult wrap_result = CreatePlatformHandleWrapper(
          mojo::embedder::ScopedPlatformHandle(
              mojo::embedder::PlatformHandle(fd_to_send)),
          &wrapped_handle);
      if (MOJO_RESULT_OK != wrap_result) {
        DLOG(WARNING) << "Pipe failed to wrap handles. Closing: "
                      << wrap_result;
        fdset->CommitAll();
        return wrap_result;
      }

      handles->push_back(wrapped_handle);
    }

    fdset->CommitAll();
  }

  return MOJO_RESULT_OK;
}

#endif  // defined(OS_POSIX) && !defined(OS_NACL)

}  // namespace IPC
