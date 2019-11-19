// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipc/ipc_channel_mojo.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/lazy_instance.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/process/process_handle.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "ipc/ipc_listener.h"
#include "ipc/ipc_logging.h"
#include "ipc/ipc_message_attachment_set.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_mojo_bootstrap.h"
#include "ipc/ipc_mojo_handle_attachment.h"
#include "ipc/native_handle_type_converters.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/lib/message_quota_checker.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/system/platform_handle.h"

namespace IPC {

namespace {

class MojoChannelFactory : public ChannelFactory {
 public:
  MojoChannelFactory(
      mojo::ScopedMessagePipeHandle handle,
      Channel::Mode mode,
      const scoped_refptr<base::SingleThreadTaskRunner>& ipc_task_runner,
      const scoped_refptr<base::SingleThreadTaskRunner>& proxy_task_runner)
      : handle_(std::move(handle)),
        mode_(mode),
        ipc_task_runner_(ipc_task_runner),
        proxy_task_runner_(proxy_task_runner),
        quota_checker_(mojo::internal::MessageQuotaChecker::MaybeCreate()) {}

  std::unique_ptr<Channel> BuildChannel(Listener* listener) override {
    return ChannelMojo::Create(std::move(handle_), mode_, listener,
                               ipc_task_runner_, proxy_task_runner_,
                               quota_checker_);
  }

  scoped_refptr<base::SingleThreadTaskRunner> GetIPCTaskRunner() override {
    return ipc_task_runner_;
  }

  scoped_refptr<mojo::internal::MessageQuotaChecker> GetQuotaChecker()
      override {
    return quota_checker_;
  }

 private:
  mojo::ScopedMessagePipeHandle handle_;
  const Channel::Mode mode_;
  scoped_refptr<base::SingleThreadTaskRunner> ipc_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> proxy_task_runner_;
  scoped_refptr<mojo::internal::MessageQuotaChecker> quota_checker_;

  DISALLOW_COPY_AND_ASSIGN(MojoChannelFactory);
};

base::ProcessId GetSelfPID() {
#if defined(OS_LINUX)
  if (int global_pid = Channel::GetGlobalPid())
    return global_pid;
#endif  // OS_LINUX
#if defined(OS_NACL)
  return -1;
#else
  return base::GetCurrentProcId();
#endif  // defined(OS_NACL)
}

}  // namespace

//------------------------------------------------------------------------------

// static
std::unique_ptr<ChannelMojo> ChannelMojo::Create(
    mojo::ScopedMessagePipeHandle handle,
    Mode mode,
    Listener* listener,
    const scoped_refptr<base::SingleThreadTaskRunner>& ipc_task_runner,
    const scoped_refptr<base::SingleThreadTaskRunner>& proxy_task_runner,
    const scoped_refptr<mojo::internal::MessageQuotaChecker>& quota_checker) {
  return base::WrapUnique(new ChannelMojo(std::move(handle), mode, listener,
                                          ipc_task_runner, proxy_task_runner,
                                          quota_checker));
}

// static
std::unique_ptr<ChannelFactory> ChannelMojo::CreateServerFactory(
    mojo::ScopedMessagePipeHandle handle,
    const scoped_refptr<base::SingleThreadTaskRunner>& ipc_task_runner,
    const scoped_refptr<base::SingleThreadTaskRunner>& proxy_task_runner) {
  return std::make_unique<MojoChannelFactory>(
      std::move(handle), Channel::MODE_SERVER, ipc_task_runner,
      proxy_task_runner);
}

// static
std::unique_ptr<ChannelFactory> ChannelMojo::CreateClientFactory(
    mojo::ScopedMessagePipeHandle handle,
    const scoped_refptr<base::SingleThreadTaskRunner>& ipc_task_runner,
    const scoped_refptr<base::SingleThreadTaskRunner>& proxy_task_runner) {
  return std::make_unique<MojoChannelFactory>(
      std::move(handle), Channel::MODE_CLIENT, ipc_task_runner,
      proxy_task_runner);
}

ChannelMojo::ChannelMojo(
    mojo::ScopedMessagePipeHandle handle,
    Mode mode,
    Listener* listener,
    const scoped_refptr<base::SingleThreadTaskRunner>& ipc_task_runner,
    const scoped_refptr<base::SingleThreadTaskRunner>& proxy_task_runner,
    const scoped_refptr<mojo::internal::MessageQuotaChecker>& quota_checker)
    : task_runner_(ipc_task_runner), pipe_(handle.get()), listener_(listener) {
  weak_ptr_ = weak_factory_.GetWeakPtr();
  bootstrap_ = MojoBootstrap::Create(std::move(handle), mode, ipc_task_runner,
                                     proxy_task_runner, quota_checker);
}

void ChannelMojo::ForwardMessageFromThreadSafePtr(mojo::Message message) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  if (!message_reader_ || !message_reader_->sender().is_bound())
    return;
  message_reader_->sender().internal_state()->ForwardMessage(
      std::move(message));
}

void ChannelMojo::ForwardMessageWithResponderFromThreadSafePtr(
    mojo::Message message,
    std::unique_ptr<mojo::MessageReceiver> responder) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  if (!message_reader_ || !message_reader_->sender().is_bound())
    return;
  message_reader_->sender().internal_state()->ForwardMessageWithResponder(
      std::move(message), std::move(responder));
}

ChannelMojo::~ChannelMojo() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  Close();
}

bool ChannelMojo::Connect() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  WillConnect();

  mojo::AssociatedRemote<mojom::Channel> sender;
  mojo::PendingAssociatedReceiver<mojom::Channel> receiver;
  bootstrap_->Connect(&sender, &receiver);

  DCHECK(!message_reader_);
  sender->SetPeerPid(GetSelfPID());
  message_reader_.reset(new internal::MessagePipeReader(
      pipe_, std::move(sender), std::move(receiver), this));
  return true;
}

void ChannelMojo::Pause() {
  bootstrap_->Pause();
}

void ChannelMojo::Unpause(bool flush) {
  bootstrap_->Unpause();
  if (flush)
    Flush();
}

void ChannelMojo::Flush() {
  bootstrap_->Flush();
}

void ChannelMojo::Close() {
  // NOTE: The MessagePipeReader's destructor may re-enter this function. Use
  // caution when changing this method.
  std::unique_ptr<internal::MessagePipeReader> reader =
      std::move(message_reader_);
  reader.reset();

  base::AutoLock lock(associated_interface_lock_);
  associated_interfaces_.clear();
}

void ChannelMojo::OnPipeError() {
  DCHECK(task_runner_);
  if (task_runner_->RunsTasksInCurrentSequence()) {
    listener_->OnChannelError();
  } else {
    task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&ChannelMojo::OnPipeError, weak_ptr_));
  }
}

void ChannelMojo::OnAssociatedInterfaceRequest(
    const std::string& name,
    mojo::ScopedInterfaceEndpointHandle handle) {
  GenericAssociatedInterfaceFactory factory;
  {
    base::AutoLock locker(associated_interface_lock_);
    auto iter = associated_interfaces_.find(name);
    if (iter != associated_interfaces_.end())
      factory = iter->second;
  }

  if (!factory.is_null())
    factory.Run(std::move(handle));
  else
    listener_->OnAssociatedInterfaceRequest(name, std::move(handle));
}

bool ChannelMojo::Send(Message* message) {
  DVLOG(2) << "sending message @" << message << " on channel @" << this
           << " with type " << message->type();
#if BUILDFLAG(IPC_MESSAGE_LOG_ENABLED)
  Logging::GetInstance()->OnSendMessage(message);
#endif

  std::unique_ptr<Message> scoped_message = base::WrapUnique(message);
  if (!message_reader_)
    return false;

  // Comment copied from ipc_channel_posix.cc:
  // We can't close the pipe here, because calling OnChannelError may destroy
  // this object, and that would be bad if we are called from Send(). Instead,
  // we return false and hope the caller will close the pipe. If they do not,
  // the pipe will still be closed next time OnFileCanReadWithoutBlocking is
  // called.
  //
  // With Mojo, there's no OnFileCanReadWithoutBlocking, but we expect the
  // pipe's connection error handler will be invoked in its place.
  return message_reader_->Send(std::move(scoped_message));
}

Channel::AssociatedInterfaceSupport*
ChannelMojo::GetAssociatedInterfaceSupport() { return this; }

std::unique_ptr<mojo::ThreadSafeForwarder<mojom::Channel>>
ChannelMojo::CreateThreadSafeChannel() {
  return std::make_unique<mojo::ThreadSafeForwarder<mojom::Channel>>(
      task_runner_,
      base::BindRepeating(&ChannelMojo::ForwardMessageFromThreadSafePtr,
                          weak_ptr_),
      base::BindRepeating(
          &ChannelMojo::ForwardMessageWithResponderFromThreadSafePtr,
          weak_ptr_),
      *bootstrap_->GetAssociatedGroup());
}

void ChannelMojo::OnPeerPidReceived(int32_t peer_pid) {
  listener_->OnChannelConnected(peer_pid);
}

void ChannelMojo::OnMessageReceived(const Message& message) {
  TRACE_EVENT2("ipc,toplevel", "ChannelMojo::OnMessageReceived",
               "class", IPC_MESSAGE_ID_CLASS(message.type()),
               "line", IPC_MESSAGE_ID_LINE(message.type()));
  listener_->OnMessageReceived(message);
  if (message.dispatch_error())
    listener_->OnBadMessageReceived(message);
}

void ChannelMojo::OnBrokenDataReceived() {
  listener_->OnBadMessageReceived(Message());
}

// static
MojoResult ChannelMojo::ReadFromMessageAttachmentSet(
    Message* message,
    base::Optional<std::vector<mojo::native::SerializedHandlePtr>>* handles) {
  DCHECK(!*handles);

  MojoResult result = MOJO_RESULT_OK;
  if (!message->HasAttachments())
    return result;

  std::vector<mojo::native::SerializedHandlePtr> output_handles;
  MessageAttachmentSet* set = message->attachment_set();

  for (unsigned i = 0; result == MOJO_RESULT_OK && i < set->size(); ++i) {
    auto attachment = set->GetAttachmentAt(i);
    auto serialized_handle = mojo::native::SerializedHandle::New();
    serialized_handle->the_handle = attachment->TakeMojoHandle();
    serialized_handle->type =
        mojo::ConvertTo<mojo::native::SerializedHandleType>(
            attachment->GetType());
    output_handles.emplace_back(std::move(serialized_handle));
  }
  set->CommitAllDescriptors();

  if (!output_handles.empty())
    *handles = std::move(output_handles);

  return result;
}

// static
MojoResult ChannelMojo::WriteToMessageAttachmentSet(
    base::Optional<std::vector<mojo::native::SerializedHandlePtr>> handles,
    Message* message) {
  if (!handles)
    return MOJO_RESULT_OK;
  for (size_t i = 0; i < handles->size(); ++i) {
    auto& handle = handles->at(i);
    scoped_refptr<MessageAttachment> unwrapped_attachment =
        MessageAttachment::CreateFromMojoHandle(
            std::move(handle->the_handle),
            mojo::ConvertTo<MessageAttachment::Type>(handle->type));
    if (!unwrapped_attachment) {
      DLOG(WARNING) << "Pipe failed to unwrap handles.";
      return MOJO_RESULT_UNKNOWN;
    }

    bool ok = message->attachment_set()->AddAttachment(
        std::move(unwrapped_attachment));
    DCHECK(ok);
    if (!ok) {
      LOG(ERROR) << "Failed to add new Mojo handle.";
      return MOJO_RESULT_UNKNOWN;
    }
  }
  return MOJO_RESULT_OK;
}

void ChannelMojo::AddGenericAssociatedInterface(
    const std::string& name,
    const GenericAssociatedInterfaceFactory& factory) {
  base::AutoLock locker(associated_interface_lock_);
  auto result = associated_interfaces_.insert({ name, factory });
  DCHECK(result.second);
}

void ChannelMojo::GetGenericRemoteAssociatedInterface(
    const std::string& name,
    mojo::ScopedInterfaceEndpointHandle handle) {
  if (message_reader_) {
    message_reader_->GetRemoteInterface(name, std::move(handle));
  } else {
    // Attach the associated interface to a disconnected pipe, so that the
    // associated interface pointer can be used to make calls (which are
    // dropped).
    mojo::AssociateWithDisconnectedPipe(std::move(handle));
  }
}

}  // namespace IPC
