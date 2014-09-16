// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipc/mojo/ipc_channel_mojo.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/lazy_instance.h"
#include "ipc/ipc_listener.h"
#include "ipc/mojo/ipc_channel_mojo_readers.h"
#include "mojo/embedder/embedder.h"

#if defined(OS_POSIX) && !defined(OS_NACL)
#include "ipc/file_descriptor_set_posix.h"
#endif

namespace IPC {

namespace {

// IPC::Listener for bootstrap channels.
// It should never receive any message.
class NullListener : public Listener {
 public:
  virtual bool OnMessageReceived(const Message&) OVERRIDE {
    NOTREACHED();
    return false;
  }

  virtual void OnChannelConnected(int32 peer_pid) OVERRIDE {
    NOTREACHED();
  }

  virtual void OnChannelError() OVERRIDE {
    NOTREACHED();
  }

  virtual void OnBadMessageReceived(const Message& message) OVERRIDE {
    NOTREACHED();
  }
};

base::LazyInstance<NullListener> g_null_listener = LAZY_INSTANCE_INITIALIZER;

class MojoChannelFactory : public ChannelFactory {
 public:
  MojoChannelFactory(
      ChannelHandle channel_handle,
      Channel::Mode mode,
      scoped_refptr<base::TaskRunner> io_thread_task_runner)
      : channel_handle_(channel_handle),
        mode_(mode),
        io_thread_task_runner_(io_thread_task_runner) {
  }

  virtual std::string GetName() const OVERRIDE {
    return channel_handle_.name;
  }

  virtual scoped_ptr<Channel> BuildChannel(Listener* listener) OVERRIDE {
    return ChannelMojo::Create(
        channel_handle_,
        mode_,
        listener,
        io_thread_task_runner_).PassAs<Channel>();
  }

 private:
  ChannelHandle channel_handle_;
  Channel::Mode mode_;
  scoped_refptr<base::TaskRunner> io_thread_task_runner_;
};

mojo::embedder::PlatformHandle ToPlatformHandle(
    const ChannelHandle& handle) {
#if defined(OS_POSIX) && !defined(OS_NACL)
  return mojo::embedder::PlatformHandle(handle.socket.fd);
#elif defined(OS_WIN)
  return mojo::embedder::PlatformHandle(handle.pipe.handle);
#else
#error "Unsupported Platform!"
#endif
}

} // namespace

//------------------------------------------------------------------------------

void ChannelMojo::ChannelInfoDeleter::operator()(
    mojo::embedder::ChannelInfo* ptr) const {
  mojo::embedder::DestroyChannelOnIOThread(ptr);
}

//------------------------------------------------------------------------------

// static
scoped_ptr<ChannelMojo> ChannelMojo::Create(
    const ChannelHandle &channel_handle, Mode mode, Listener* listener,
    scoped_refptr<base::TaskRunner> io_thread_task_runner) {
  return make_scoped_ptr(
      new ChannelMojo(channel_handle, mode, listener, io_thread_task_runner));
}

// static
scoped_ptr<ChannelFactory> ChannelMojo::CreateFactory(
    const ChannelHandle &channel_handle, Mode mode,
    scoped_refptr<base::TaskRunner> io_thread_task_runner) {
  return make_scoped_ptr(
      new MojoChannelFactory(
          channel_handle, mode,
          io_thread_task_runner)).PassAs<ChannelFactory>();
}

ChannelMojo::ChannelMojo(const ChannelHandle& channel_handle,
                         Mode mode,
                         Listener* listener,
                         scoped_refptr<base::TaskRunner> io_thread_task_runner)
    : bootstrap_(
          Channel::Create(channel_handle, mode, g_null_listener.Pointer())),
      mode_(mode),
      listener_(listener),
      peer_pid_(base::kNullProcessId),
      weak_factory_(this) {
  if (base::MessageLoopProxy::current() == io_thread_task_runner.get()) {
    InitOnIOThread();
  } else {
    io_thread_task_runner->PostTask(FROM_HERE,
                                    base::Bind(&ChannelMojo::InitOnIOThread,
                                               weak_factory_.GetWeakPtr()));
  }
}

ChannelMojo::~ChannelMojo() {
  Close();
}

void ChannelMojo::InitOnIOThread() {
  mojo::embedder::ChannelInfo* channel_info;
  mojo::ScopedMessagePipeHandle control_pipe =
      mojo::embedder::CreateChannelOnIOThread(
          mojo::embedder::ScopedPlatformHandle(
              ToPlatformHandle(bootstrap_->TakePipeHandle())),
          &channel_info);
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
  return control_reader_->Connect();
}

void ChannelMojo::Close() {
  control_reader_.reset();
  message_reader_.reset();
  channel_info_.reset();
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
  return bootstrap_->GetSelfPID();
}

ChannelHandle ChannelMojo::TakePipeHandle() {
  return bootstrap_->TakePipeHandle();
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

    bool ok = message->file_descriptor_set()->Add(platform_handle.release().fd);
    DCHECK(ok);
  }

  return MOJO_RESULT_OK;
}

// static
MojoResult ChannelMojo::ReadFromFileDescriptorSet(
    const Message& message,
    std::vector<MojoHandle>* handles) {
  // We dup() the handles in IPC::Message to transmit.
  // IPC::FileDescriptorSet has intricate lifecycle semantics
  // of FDs, so just to dup()-and-own them is the safest option.
  if (message.HasFileDescriptors()) {
    const FileDescriptorSet* fdset = message.file_descriptor_set();
    for (size_t i = 0; i < fdset->size(); ++i) {
      int fd_to_send = dup(fdset->GetDescriptorAt(i));
      if (-1 == fd_to_send) {
        DPLOG(WARNING) << "Failed to dup FD to transmit.";
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
        return wrap_result;
      }

      handles->push_back(wrapped_handle);
    }
  }

  return MOJO_RESULT_OK;
}

#endif  // defined(OS_POSIX) && !defined(OS_NACL)

}  // namespace IPC
