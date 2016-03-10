// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipc/mojo/ipc_channel_mojo.h"

#include <stddef.h>
#include <stdint.h>
#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/lazy_instance.h"
#include "base/macros.h"
#include "base/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "ipc/ipc_listener.h"
#include "ipc/ipc_logging.h"
#include "ipc/ipc_message_attachment_set.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/mojo/ipc_mojo_bootstrap.h"
#include "ipc/mojo/ipc_mojo_handle_attachment.h"
#include "mojo/edk/embedder/embedder.h"
#include "mojo/public/cpp/bindings/binding.h"

#if defined(OS_POSIX) && !defined(OS_NACL)
#include "ipc/ipc_platform_file_attachment_posix.h"
#endif

namespace IPC {

namespace {

class MojoChannelFactory : public ChannelFactory {
 public:
  MojoChannelFactory(const std::string& token, Channel::Mode mode)
      : token_(token), mode_(mode) {}

  std::string GetName() const override {
    return token_;
  }

  scoped_ptr<Channel> BuildChannel(Listener* listener) override {
    return ChannelMojo::Create(token_, mode_, listener);
  }

 private:
  const std::string token_;
  const Channel::Mode mode_;

  DISALLOW_COPY_AND_ASSIGN(MojoChannelFactory);
};

#if defined(OS_POSIX) && !defined(OS_NACL)

base::ScopedFD TakeOrDupFile(internal::PlatformFileAttachment* attachment) {
  return attachment->Owns() ? base::ScopedFD(attachment->TakePlatformFile())
                            : base::ScopedFD(dup(attachment->file()));
}

#endif

}  // namespace

//------------------------------------------------------------------------------

// static
bool ChannelMojo::ShouldBeUsed() {
  // TODO(rockot): Investigate performance bottlenecks and hopefully reenable
  // this at some point. http://crbug.com/500019
  return false;
}

// static
scoped_ptr<ChannelMojo> ChannelMojo::Create(const std::string& token,
                                            Mode mode,
                                            Listener* listener) {
  return make_scoped_ptr(
      new ChannelMojo(token, mode, listener));
}

// static
scoped_ptr<ChannelFactory> ChannelMojo::CreateServerFactory(
    const std::string& token) {
  return make_scoped_ptr(
      new MojoChannelFactory(token, Channel::MODE_SERVER));
}

// static
scoped_ptr<ChannelFactory> ChannelMojo::CreateClientFactory(
    const std::string& token) {
  return make_scoped_ptr(
      new MojoChannelFactory(token, Channel::MODE_CLIENT));
}

ChannelMojo::ChannelMojo(const std::string& token,
                         Mode mode,
                         Listener* listener)
    : listener_(listener),
      peer_pid_(base::kNullProcessId),
      waiting_connect_(true),
      weak_factory_(this) {
  // Create MojoBootstrap after all members are set as it touches
  // ChannelMojo from a different thread.
  bootstrap_ = MojoBootstrap::Create(token, mode, this);
}

ChannelMojo::~ChannelMojo() {
  Close();
}

scoped_ptr<internal::MessagePipeReader, ChannelMojo::ReaderDeleter>
ChannelMojo::CreateMessageReader(mojom::ChannelAssociatedPtrInfo sender,
                                 mojom::ChannelAssociatedRequest receiver) {
  mojom::ChannelAssociatedPtr sender_ptr;
  sender_ptr.Bind(std::move(sender));
  return scoped_ptr<internal::MessagePipeReader, ChannelMojo::ReaderDeleter>(
      new internal::MessagePipeReader(std::move(sender_ptr),
                                      std::move(receiver), this));
}

bool ChannelMojo::Connect() {
  DCHECK(!message_reader_);
  bootstrap_->Connect();
  return true;
}

void ChannelMojo::Close() {
  message_reader_.reset();
  // We might Close() before we Connect().
  waiting_connect_ = false;
}

// MojoBootstrap::Delegate implementation
void ChannelMojo::OnPipesAvailable(
    mojom::ChannelAssociatedPtrInfo send_channel,
    mojom::ChannelAssociatedRequest receive_channel,
    int32_t peer_pid) {
  set_peer_pid(peer_pid);
  InitMessageReader(std::move(send_channel), std::move(receive_channel));
}

void ChannelMojo::OnBootstrapError() {
  listener_->OnChannelError();
}

void ChannelMojo::InitMessageReader(mojom::ChannelAssociatedPtrInfo sender,
                                    mojom::ChannelAssociatedRequest receiver) {
  scoped_ptr<internal::MessagePipeReader, ChannelMojo::ReaderDeleter> reader =
      CreateMessageReader(std::move(sender), std::move(receiver));

  for (size_t i = 0; i < pending_messages_.size(); ++i) {
    bool sent = reader->Send(std::move(pending_messages_[i]));
    if (!sent) {
      // OnChannelError() is notified by OnPipeError().
      pending_messages_.clear();
      LOG(ERROR) << "Failed to flush pending messages";
      return;
    }
  }

  // We set |message_reader_| here and won't get any |pending_messages_|
  // hereafter. Although we might have some if there is an error, we don't
  // care. They cannot be sent anyway.
  message_reader_ = std::move(reader);
  pending_messages_.clear();
  waiting_connect_ = false;

  listener_->OnChannelConnected(static_cast<int32_t>(GetPeerPID()));
}

void ChannelMojo::OnPipeClosed(internal::MessagePipeReader* reader) {
  Close();
}

void ChannelMojo::OnPipeError(internal::MessagePipeReader* reader) {
  listener_->OnChannelError();
}


bool ChannelMojo::Send(Message* message) {
  if (!message_reader_) {
    pending_messages_.push_back(make_scoped_ptr(message));
    // Counts as OK before the connection is established, but it's an
    // error otherwise.
    return waiting_connect_;
  }

  return message_reader_->Send(make_scoped_ptr(message));
}

base::ProcessId ChannelMojo::GetPeerPID() const {
  return peer_pid_;
}

base::ProcessId ChannelMojo::GetSelfPID() const {
  return bootstrap_->GetSelfPID();
}

void ChannelMojo::OnMessageReceived(const Message& message) {
  TRACE_EVENT2("ipc,toplevel", "ChannelMojo::OnMessageReceived",
               "class", IPC_MESSAGE_ID_CLASS(message.type()),
               "line", IPC_MESSAGE_ID_LINE(message.type()));
  listener_->OnMessageReceived(message);
  if (message.dispatch_error())
    listener_->OnBadMessageReceived(message);
}

#if defined(OS_POSIX) && !defined(OS_NACL)
int ChannelMojo::GetClientFileDescriptor() const {
  return -1;
}

base::ScopedFD ChannelMojo::TakeClientFileDescriptor() {
  return base::ScopedFD(GetClientFileDescriptor());
}
#endif  // defined(OS_POSIX) && !defined(OS_NACL)

// static
MojoResult ChannelMojo::ReadFromMessageAttachmentSet(
    Message* message,
    mojo::Array<mojo::ScopedHandle>* handles) {
  // We dup() the handles in IPC::Message to transmit.
  // IPC::MessageAttachmentSet has intricate lifecycle semantics
  // of FDs, so just to dup()-and-own them is the safest option.
  if (message->HasAttachments()) {
    MessageAttachmentSet* set = message->attachment_set();
    for (unsigned i = 0; i < set->num_non_brokerable_attachments(); ++i) {
      scoped_refptr<MessageAttachment> attachment =
          set->GetNonBrokerableAttachmentAt(i);
      switch (attachment->GetType()) {
        case MessageAttachment::TYPE_PLATFORM_FILE:
#if defined(OS_POSIX) && !defined(OS_NACL)
        {
          base::ScopedFD file =
              TakeOrDupFile(static_cast<IPC::internal::PlatformFileAttachment*>(
                  attachment.get()));
          if (!file.is_valid()) {
            DPLOG(WARNING) << "Failed to dup FD to transmit.";
            set->CommitAllDescriptors();
            return MOJO_RESULT_UNKNOWN;
          }

          MojoHandle wrapped_handle;
          MojoResult wrap_result = mojo::edk::CreatePlatformHandleWrapper(
              mojo::edk::ScopedPlatformHandle(
                  mojo::edk::PlatformHandle(file.release())),
              &wrapped_handle);
          if (MOJO_RESULT_OK != wrap_result) {
            LOG(WARNING) << "Pipe failed to wrap handles. Closing: "
                         << wrap_result;
            set->CommitAllDescriptors();
            return wrap_result;
          }

          handles->push_back(
              mojo::MakeScopedHandle(mojo::Handle(wrapped_handle)));
        }
#else
          NOTREACHED();
#endif  //  defined(OS_POSIX) && !defined(OS_NACL)
        break;
        case MessageAttachment::TYPE_MOJO_HANDLE: {
          mojo::ScopedHandle handle =
              static_cast<IPC::internal::MojoHandleAttachment*>(
                  attachment.get())->TakeHandle();
          handles->push_back(std::move(handle));
        } break;
        case MessageAttachment::TYPE_BROKERABLE_ATTACHMENT:
          // Brokerable attachments are handled by the AttachmentBroker so
          // there's no need to do anything here.
          NOTREACHED();
          break;
      }
    }

    set->CommitAllDescriptors();
  }

  return MOJO_RESULT_OK;
}

// static
MojoResult ChannelMojo::WriteToMessageAttachmentSet(
    mojo::Array<mojo::ScopedHandle> handle_buffer,
    Message* message) {
  for (size_t i = 0; i < handle_buffer.size(); ++i) {
    bool ok = message->attachment_set()->AddAttachment(
        new IPC::internal::MojoHandleAttachment(std::move(handle_buffer[i])));
    DCHECK(ok);
    if (!ok) {
      LOG(ERROR) << "Failed to add new Mojo handle.";
      return MOJO_RESULT_UNKNOWN;
    }
  }

  return MOJO_RESULT_OK;
}

}  // namespace IPC
