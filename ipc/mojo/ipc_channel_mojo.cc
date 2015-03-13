// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipc/mojo/ipc_channel_mojo.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/lazy_instance.h"
#include "ipc/ipc_listener.h"
#include "ipc/ipc_logging.h"
#include "ipc/ipc_message_attachment_set.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/mojo/client_channel.mojom.h"
#include "ipc/mojo/ipc_mojo_bootstrap.h"
#include "ipc/mojo/ipc_mojo_handle_attachment.h"
#include "third_party/mojo/src/mojo/edk/embedder/embedder.h"
#include "third_party/mojo/src/mojo/public/cpp/bindings/error_handler.h"

#if defined(OS_POSIX) && !defined(OS_NACL)
#include "ipc/ipc_platform_file_attachment_posix.h"
#endif

namespace IPC {

namespace {

class MojoChannelFactory : public ChannelFactory {
 public:
  MojoChannelFactory(ChannelMojo::Delegate* delegate,
                     ChannelHandle channel_handle,
                     Channel::Mode mode)
      : delegate_(delegate), channel_handle_(channel_handle), mode_(mode) {}

  std::string GetName() const override {
    return channel_handle_.name;
  }

  scoped_ptr<Channel> BuildChannel(Listener* listener) override {
    return ChannelMojo::Create(delegate_, channel_handle_, mode_, listener);
  }

 private:
  ChannelMojo::Delegate* delegate_;
  ChannelHandle channel_handle_;
  Channel::Mode mode_;
};

//------------------------------------------------------------------------------

class ClientChannelMojo : public ChannelMojo,
                          public ClientChannel,
                          public mojo::ErrorHandler {
 public:
  ClientChannelMojo(ChannelMojo::Delegate* delegate,
                    const ChannelHandle& handle,
                    Listener* listener);
  ~ClientChannelMojo() override;
  // MojoBootstrap::Delegate implementation
  void OnPipeAvailable(mojo::embedder::ScopedPlatformHandle handle) override;
  // mojo::ErrorHandler implementation
  void OnConnectionError() override;
  // ClientChannel implementation
  void Init(
      mojo::ScopedMessagePipeHandle pipe,
      int32_t peer_pid,
      const mojo::Callback<void(int32_t)>& callback) override;

 private:
  mojo::Binding<ClientChannel> binding_;

  DISALLOW_COPY_AND_ASSIGN(ClientChannelMojo);
};

ClientChannelMojo::ClientChannelMojo(ChannelMojo::Delegate* delegate,
                                     const ChannelHandle& handle,
                                     Listener* listener)
    : ChannelMojo(delegate, handle, Channel::MODE_CLIENT, listener),
      binding_(this) {
}

ClientChannelMojo::~ClientChannelMojo() {
}

void ClientChannelMojo::OnPipeAvailable(
    mojo::embedder::ScopedPlatformHandle handle) {
  binding_.Bind(CreateMessagingPipe(handle.Pass()));
}

void ClientChannelMojo::OnConnectionError() {
  listener()->OnChannelError();
}

void ClientChannelMojo::Init(
    mojo::ScopedMessagePipeHandle pipe,
    int32_t peer_pid,
    const mojo::Callback<void(int32_t)>& callback) {
  InitMessageReader(pipe.Pass(), static_cast<base::ProcessId>(peer_pid));
  callback.Run(GetSelfPID());
}

//------------------------------------------------------------------------------

class ServerChannelMojo : public ChannelMojo, public mojo::ErrorHandler {
 public:
  ServerChannelMojo(ChannelMojo::Delegate* delegate,
                    const ChannelHandle& handle,
                    Listener* listener);
  ~ServerChannelMojo() override;

  // MojoBootstrap::Delegate implementation
  void OnPipeAvailable(mojo::embedder::ScopedPlatformHandle handle) override;
  // mojo::ErrorHandler implementation
  void OnConnectionError() override;
  // Channel override
  void Close() override;

 private:
  // ClientChannelClient implementation
  void ClientChannelWasInitialized(int32_t peer_pid);

  mojo::InterfacePtr<ClientChannel> client_channel_;
  mojo::ScopedMessagePipeHandle message_pipe_;

  DISALLOW_COPY_AND_ASSIGN(ServerChannelMojo);
};

ServerChannelMojo::ServerChannelMojo(ChannelMojo::Delegate* delegate,
                                     const ChannelHandle& handle,
                                     Listener* listener)
    : ChannelMojo(delegate, handle, Channel::MODE_SERVER, listener) {
}

ServerChannelMojo::~ServerChannelMojo() {
  Close();
}

void ServerChannelMojo::OnPipeAvailable(
    mojo::embedder::ScopedPlatformHandle handle) {
  mojo::ScopedMessagePipeHandle peer;
  MojoResult create_result =
      mojo::CreateMessagePipe(nullptr, &message_pipe_, &peer);
  if (create_result != MOJO_RESULT_OK) {
    DLOG(WARNING) << "mojo::CreateMessagePipe failed: " << create_result;
    listener()->OnChannelError();
    return;
  }

  client_channel_.Bind(CreateMessagingPipe(handle.Pass()));
  client_channel_.set_error_handler(this);
  client_channel_->Init(
      peer.Pass(),
      static_cast<int32_t>(GetSelfPID()),
      base::Bind(&ServerChannelMojo::ClientChannelWasInitialized,
                 base::Unretained(this)));
}

void ServerChannelMojo::ClientChannelWasInitialized(int32_t peer_pid) {
  InitMessageReader(message_pipe_.Pass(), peer_pid);
}

void ServerChannelMojo::OnConnectionError() {
  listener()->OnChannelError();
}

void ServerChannelMojo::Close() {
  client_channel_.reset();
  message_pipe_.reset();
  ChannelMojo::Close();
}

#if defined(OS_POSIX) && !defined(OS_NACL)

base::ScopedFD TakeOrDupFile(internal::PlatformFileAttachment* attachment) {
  return attachment->Owns() ? base::ScopedFD(attachment->TakePlatformFile())
                            : base::ScopedFD(dup(attachment->file()));
}

#endif

} // namespace

//------------------------------------------------------------------------------

void ChannelMojo::ChannelInfoDeleter::operator()(
    mojo::embedder::ChannelInfo* ptr) const {
  mojo::embedder::DestroyChannel(ptr, base::Bind(&base::DoNothing), nullptr);
}

//------------------------------------------------------------------------------

// static
bool ChannelMojo::ShouldBeUsed() {
  // TODO(morrita): Remove this if it sticks.
  // ChannelMojo is currently disabled due to http://crbug.com/466814.
  return false;
}

// static
scoped_ptr<ChannelMojo> ChannelMojo::Create(ChannelMojo::Delegate* delegate,
                                            const ChannelHandle& channel_handle,
                                            Mode mode,
                                            Listener* listener) {
  switch (mode) {
    case Channel::MODE_CLIENT:
      return make_scoped_ptr(
          new ClientChannelMojo(delegate, channel_handle, listener));
    case Channel::MODE_SERVER:
      return make_scoped_ptr(
          new ServerChannelMojo(delegate, channel_handle, listener));
    default:
      NOTREACHED();
      return nullptr;
  }
}

// static
scoped_ptr<ChannelFactory> ChannelMojo::CreateServerFactory(
    ChannelMojo::Delegate* delegate,
    const ChannelHandle& channel_handle) {
  return make_scoped_ptr(
      new MojoChannelFactory(delegate, channel_handle, Channel::MODE_SERVER));
}

// static
scoped_ptr<ChannelFactory> ChannelMojo::CreateClientFactory(
    ChannelMojo::Delegate* delegate,
    const ChannelHandle& channel_handle) {
  return make_scoped_ptr(
      new MojoChannelFactory(delegate, channel_handle, Channel::MODE_CLIENT));
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
  ipc_support_.reset(
      new ScopedIPCSupport(base::MessageLoop::current()->task_runner()));
  delegate_ = delegate->ToWeakPtr();
  delegate_->OnChannelCreated(weak_factory_.GetWeakPtr());
}

mojo::ScopedMessagePipeHandle ChannelMojo::CreateMessagingPipe(
    mojo::embedder::ScopedPlatformHandle handle) {
  DCHECK(!channel_info_.get());
  mojo::embedder::ChannelInfo* channel_info;
  mojo::ScopedMessagePipeHandle pipe =
      mojo::embedder::CreateChannelOnIOThread(handle.Pass(), &channel_info);
  channel_info_.reset(channel_info);
  return pipe.Pass();
}

bool ChannelMojo::Connect() {
  DCHECK(!message_reader_);
  return bootstrap_->Connect();
}

void ChannelMojo::Close() {
  message_reader_.reset();
  channel_info_.reset();
  ipc_support_.reset();
}

void ChannelMojo::OnBootstrapError() {
  listener_->OnChannelError();
}

void ChannelMojo::InitMessageReader(mojo::ScopedMessagePipeHandle pipe,
                                    int32_t peer_pid) {
  message_reader_ =
      make_scoped_ptr(new internal::MessagePipeReader(pipe.Pass(), this));

  for (size_t i = 0; i < pending_messages_.size(); ++i) {
    bool sent = message_reader_->Send(make_scoped_ptr(pending_messages_[i]));
    pending_messages_[i] = nullptr;
    if (!sent) {
      pending_messages_.clear();
      listener_->OnChannelError();
      return;
    }
  }

  pending_messages_.clear();

  set_peer_pid(peer_pid);
  listener_->OnChannelConnected(static_cast<int32_t>(GetPeerPID()));
  if (message_reader_)
    message_reader_->ReadMessagesThenWait();
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

void ChannelMojo::OnClientLaunched(base::ProcessHandle handle) {
  bootstrap_->OnClientLaunched(handle);
}

void ChannelMojo::OnMessageReceived(Message& message) {
  TRACE_EVENT2("ipc,toplevel", "ChannelMojo::OnMessageReceived",
               "class", IPC_MESSAGE_ID_CLASS(message.type()),
               "line", IPC_MESSAGE_ID_LINE(message.type()));
  listener_->OnMessageReceived(message);
  if (message.dispatch_error())
    listener_->OnBadMessageReceived(message);
}

#if defined(OS_POSIX) && !defined(OS_NACL)
int ChannelMojo::GetClientFileDescriptor() const {
  return bootstrap_->GetClientFileDescriptor();
}

base::ScopedFD ChannelMojo::TakeClientFileDescriptor() {
  return bootstrap_->TakeClientFileDescriptor();
}
#endif  // defined(OS_POSIX) && !defined(OS_NACL)

// static
MojoResult ChannelMojo::ReadFromMessageAttachmentSet(
    Message* message,
    std::vector<MojoHandle>* handles) {
  // We dup() the handles in IPC::Message to transmit.
  // IPC::MessageAttachmentSet has intricate lifecycle semantics
  // of FDs, so just to dup()-and-own them is the safest option.
  if (message->HasAttachments()) {
    MessageAttachmentSet* set = message->attachment_set();
    for (unsigned i = 0; i < set->size(); ++i) {
      scoped_refptr<MessageAttachment> attachment = set->GetAttachmentAt(i);
      switch (attachment->GetType()) {
        case MessageAttachment::TYPE_PLATFORM_FILE:
#if defined(OS_POSIX) && !defined(OS_NACL)
        {
          base::ScopedFD file =
              TakeOrDupFile(static_cast<IPC::internal::PlatformFileAttachment*>(
                  attachment.get()));
          if (!file.is_valid()) {
            DPLOG(WARNING) << "Failed to dup FD to transmit.";
            set->CommitAll();
            return MOJO_RESULT_UNKNOWN;
          }

          MojoHandle wrapped_handle;
          MojoResult wrap_result = CreatePlatformHandleWrapper(
              mojo::embedder::ScopedPlatformHandle(
                  mojo::embedder::PlatformHandle(file.release())),
              &wrapped_handle);
          if (MOJO_RESULT_OK != wrap_result) {
            DLOG(WARNING) << "Pipe failed to wrap handles. Closing: "
                          << wrap_result;
            set->CommitAll();
            return wrap_result;
          }

          handles->push_back(wrapped_handle);
        }
#else
          NOTREACHED();
#endif  //  defined(OS_POSIX) && !defined(OS_NACL)
        break;
        case MessageAttachment::TYPE_MOJO_HANDLE: {
          mojo::ScopedHandle handle =
              static_cast<IPC::internal::MojoHandleAttachment*>(
                  attachment.get())->TakeHandle();
          handles->push_back(handle.release().value());
        } break;
      }
    }

    set->CommitAll();
  }

  return MOJO_RESULT_OK;
}

// static
MojoResult ChannelMojo::WriteToMessageAttachmentSet(
    const std::vector<MojoHandle>& handle_buffer,
    Message* message) {
  for (size_t i = 0; i < handle_buffer.size(); ++i) {
    bool ok = message->attachment_set()->AddAttachment(
        new IPC::internal::MojoHandleAttachment(
            mojo::MakeScopedHandle(mojo::Handle(handle_buffer[i]))));
    DCHECK(ok);
    if (!ok) {
      DLOG(ERROR) << "Failed to add new Mojo handle.";
      return MOJO_RESULT_UNKNOWN;
    }
  }

  return MOJO_RESULT_OK;
}

}  // namespace IPC
