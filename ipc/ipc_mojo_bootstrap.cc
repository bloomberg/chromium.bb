// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipc/ipc_mojo_bootstrap.h"

#include <stdint.h>

#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/single_thread_task_runner.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread_task_runner_handle.h"
#include "mojo/public/cpp/bindings/associated_group.h"
#include "mojo/public/cpp/bindings/associated_group_controller.h"
#include "mojo/public/cpp/bindings/connector.h"
#include "mojo/public/cpp/bindings/interface_endpoint_client.h"
#include "mojo/public/cpp/bindings/interface_endpoint_controller.h"
#include "mojo/public/cpp/bindings/interface_id.h"
#include "mojo/public/cpp/bindings/message.h"
#include "mojo/public/cpp/bindings/message_header_validator.h"
#include "mojo/public/cpp/bindings/pipe_control_message_handler.h"
#include "mojo/public/cpp/bindings/pipe_control_message_handler_delegate.h"
#include "mojo/public/cpp/bindings/pipe_control_message_proxy.h"

namespace IPC {

namespace {

class ChannelAssociatedGroupController
    : public mojo::AssociatedGroupController,
      public mojo::MessageReceiver,
      public mojo::PipeControlMessageHandlerDelegate {
 public:
  ChannelAssociatedGroupController(
      bool set_interface_id_namespace_bit,
      const scoped_refptr<base::SingleThreadTaskRunner>& ipc_task_runner)
      : task_runner_(ipc_task_runner),
        proxy_task_runner_(base::ThreadTaskRunnerHandle::Get()),
        set_interface_id_namespace_bit_(set_interface_id_namespace_bit),
        header_validator_(
            "IPC::mojom::Bootstrap [master] MessageHeaderValidator", this),
        control_message_handler_(this),
        control_message_proxy_thunk_(this),
        control_message_proxy_(&control_message_proxy_thunk_) {
    thread_checker_.DetachFromThread();
    control_message_handler_.SetDescription(
        "IPC::mojom::Bootstrap [master] PipeControlMessageHandler");
  }

  void Bind(mojo::ScopedMessagePipeHandle handle) {
    DCHECK(thread_checker_.CalledOnValidThread());
    DCHECK(task_runner_->BelongsToCurrentThread());

    connector_.reset(new mojo::Connector(
        std::move(handle), mojo::Connector::SINGLE_THREADED_SEND,
        task_runner_));
    connector_->set_incoming_receiver(&header_validator_);
    connector_->set_connection_error_handler(
        base::Bind(&ChannelAssociatedGroupController::OnPipeError,
                   base::Unretained(this)));

    std::vector<std::unique_ptr<mojo::Message>> outgoing_messages;
    std::swap(outgoing_messages, outgoing_messages_);
    for (auto& message : outgoing_messages)
      SendMessage(message.get());
  }

  void CreateChannelEndpoints(mojom::ChannelAssociatedPtr* sender,
                              mojom::ChannelAssociatedRequest* receiver) {
    mojo::InterfaceId sender_id, receiver_id;
    if (set_interface_id_namespace_bit_) {
      sender_id = 1 | mojo::kInterfaceIdNamespaceMask;
      receiver_id = 1;
    } else {
      sender_id = 1;
      receiver_id = 1 | mojo::kInterfaceIdNamespaceMask;
    }

    {
      base::AutoLock locker(lock_);
      Endpoint* sender_endpoint = new Endpoint(this, sender_id);
      Endpoint* receiver_endpoint = new Endpoint(this, receiver_id);
      endpoints_.insert({ sender_id, sender_endpoint });
      endpoints_.insert({ receiver_id, receiver_endpoint });
    }

    mojo::ScopedInterfaceEndpointHandle sender_handle =
        CreateScopedInterfaceEndpointHandle(sender_id, true);
    mojo::ScopedInterfaceEndpointHandle receiver_handle =
        CreateScopedInterfaceEndpointHandle(receiver_id, true);

    sender->Bind(mojom::ChannelAssociatedPtrInfo(std::move(sender_handle), 0));
    receiver->Bind(std::move(receiver_handle));
  }

  void ShutDown() {
    DCHECK(thread_checker_.CalledOnValidThread());
    connector_->CloseMessagePipe();
    OnPipeError();
    connector_.reset();
  }

  // mojo::AssociatedGroupController:
  void CreateEndpointHandlePair(
      mojo::ScopedInterfaceEndpointHandle* local_endpoint,
      mojo::ScopedInterfaceEndpointHandle* remote_endpoint) override {
    base::AutoLock locker(lock_);
    uint32_t id = 0;
    do {
      if (next_interface_id_ >= mojo::kInterfaceIdNamespaceMask)
        next_interface_id_ = 2;
      id = next_interface_id_++;
      if (set_interface_id_namespace_bit_)
        id |= mojo::kInterfaceIdNamespaceMask;
    } while (ContainsKey(endpoints_, id));

    Endpoint* endpoint = new Endpoint(this, id);
    if (encountered_error_)
      endpoint->set_peer_closed();
    endpoints_.insert({ id, endpoint });

    *local_endpoint = CreateScopedInterfaceEndpointHandle(id, true);
    *remote_endpoint = CreateScopedInterfaceEndpointHandle(id, false);
  }

  mojo::ScopedInterfaceEndpointHandle CreateLocalEndpointHandle(
      mojo::InterfaceId id) override {
    if (!mojo::IsValidInterfaceId(id))
      return mojo::ScopedInterfaceEndpointHandle();

    base::AutoLock locker(lock_);
    bool inserted = false;
    Endpoint* endpoint = FindOrInsertEndpoint(id, &inserted);
    if (inserted && encountered_error_)
      endpoint->set_peer_closed();

    return CreateScopedInterfaceEndpointHandle(id, true);
  }

  void CloseEndpointHandle(mojo::InterfaceId id, bool is_local) override {
    if (!mojo::IsValidInterfaceId(id))
      return;

    base::AutoLock locker(lock_);
    if (!is_local) {
      DCHECK(ContainsKey(endpoints_, id));
      DCHECK(!mojo::IsMasterInterfaceId(id));
      control_message_proxy_.NotifyEndpointClosedBeforeSent(id);
      return;
    }

    DCHECK(ContainsKey(endpoints_, id));
    Endpoint* endpoint = endpoints_[id].get();
    DCHECK(!endpoint->client());
    DCHECK(!endpoint->closed());
    MarkClosedAndMaybeRemove(endpoint);

    if (!mojo::IsMasterInterfaceId(id))
      control_message_proxy_.NotifyPeerEndpointClosed(id);
  }

  mojo::InterfaceEndpointController* AttachEndpointClient(
      const mojo::ScopedInterfaceEndpointHandle& handle,
      mojo::InterfaceEndpointClient* client,
      scoped_refptr<base::SingleThreadTaskRunner> runner) override {
    const mojo::InterfaceId id = handle.id();

    DCHECK(mojo::IsValidInterfaceId(id));
    DCHECK(client);

    base::AutoLock locker(lock_);
    DCHECK(ContainsKey(endpoints_, id));

    Endpoint* endpoint = endpoints_[id].get();
    endpoint->AttachClient(client, std::move(runner));

    if (endpoint->peer_closed())
      NotifyEndpointOfError(endpoint, true /* force_async */);

    return endpoint;
  }

  void DetachEndpointClient(
      const mojo::ScopedInterfaceEndpointHandle& handle) override {
    const mojo::InterfaceId id = handle.id();

    DCHECK(mojo::IsValidInterfaceId(id));

    base::AutoLock locker(lock_);
    DCHECK(ContainsKey(endpoints_, id));

    Endpoint* endpoint = endpoints_[id].get();
    endpoint->DetachClient();
  }

  void RaiseError() override {
    if (task_runner_->BelongsToCurrentThread()) {
      connector_->RaiseError();
    } else {
      task_runner_->PostTask(
          FROM_HERE,
          base::Bind(&ChannelAssociatedGroupController::RaiseError, this));
    }
  }

 private:
  class Endpoint;
  class ControlMessageProxyThunk;
  friend class Endpoint;
  friend class ControlMessageProxyThunk;

  class Endpoint : public base::RefCountedThreadSafe<Endpoint>,
                   public mojo::InterfaceEndpointController {
   public:
    Endpoint(ChannelAssociatedGroupController* controller, mojo::InterfaceId id)
        : controller_(controller), id_(id) {}

    mojo::InterfaceId id() const { return id_; }

    bool closed() const {
      controller_->lock_.AssertAcquired();
      return closed_;
    }

    void set_closed() {
      controller_->lock_.AssertAcquired();
      closed_ = true;
    }

    bool peer_closed() const {
      controller_->lock_.AssertAcquired();
      return peer_closed_;
    }

    void set_peer_closed() {
      controller_->lock_.AssertAcquired();
      peer_closed_ = true;
    }

    base::SingleThreadTaskRunner* task_runner() const {
      return task_runner_.get();
    }

    mojo::InterfaceEndpointClient* client() const {
      controller_->lock_.AssertAcquired();
      return client_;
    }

    void AttachClient(mojo::InterfaceEndpointClient* client,
                      scoped_refptr<base::SingleThreadTaskRunner> runner) {
      controller_->lock_.AssertAcquired();
      DCHECK(!client_);
      DCHECK(!closed_);
      DCHECK(runner->BelongsToCurrentThread());

      task_runner_ = std::move(runner);
      client_ = client;
    }

    void DetachClient() {
      controller_->lock_.AssertAcquired();
      DCHECK(client_);
      DCHECK(task_runner_->BelongsToCurrentThread());
      DCHECK(!closed_);

      task_runner_ = nullptr;
      client_ = nullptr;
    }

    // mojo::InterfaceEndpointController:
    bool SendMessage(mojo::Message* message) override {
      DCHECK(task_runner_->BelongsToCurrentThread());
      message->set_interface_id(id_);
      return controller_->SendMessage(message);
    }

    void AllowWokenUpBySyncWatchOnSameThread() override {
      DCHECK(task_runner_->BelongsToCurrentThread());

      // TODO(rockot): Implement sync waiting.
      NOTREACHED();
    }

    bool SyncWatch(const bool* should_stop) override {
      DCHECK(task_runner_->BelongsToCurrentThread());

      // It's not legal to make sync calls from the master endpoint's thread,
      // and in fact they must only happen from the proxy task runner.
      DCHECK(!controller_->task_runner_->BelongsToCurrentThread());
      DCHECK(controller_->proxy_task_runner_->BelongsToCurrentThread());

      // TODO(rockot): Implement sync waiting.
      NOTREACHED();
      return false;
    }

   private:
    friend class base::RefCountedThreadSafe<Endpoint>;

    ~Endpoint() override {}

    ChannelAssociatedGroupController* const controller_;
    const mojo::InterfaceId id_;

    bool closed_ = false;
    bool peer_closed_ = false;
    mojo::InterfaceEndpointClient* client_ = nullptr;
    scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

    DISALLOW_COPY_AND_ASSIGN(Endpoint);
  };

  class ControlMessageProxyThunk : public MessageReceiver {
   public:
    explicit ControlMessageProxyThunk(
        ChannelAssociatedGroupController* controller)
        : controller_(controller) {}

   private:
    // MessageReceiver:
    bool Accept(mojo::Message* message) override {
      return controller_->SendMessage(message);
    }

    ChannelAssociatedGroupController* controller_;

    DISALLOW_COPY_AND_ASSIGN(ControlMessageProxyThunk);
  };

  ~ChannelAssociatedGroupController() override {
    DCHECK(!connector_);

    base::AutoLock locker(lock_);
    for (auto iter = endpoints_.begin(); iter != endpoints_.end();) {
      Endpoint* endpoint = iter->second.get();
      ++iter;

      DCHECK(endpoint->closed());
      MarkPeerClosedAndMaybeRemove(endpoint);
    }

    DCHECK(endpoints_.empty());
  }

  bool SendMessage(mojo::Message* message) {
    if (task_runner_->BelongsToCurrentThread()) {
      DCHECK(thread_checker_.CalledOnValidThread());
      if (!connector_) {
        // Pipe may not be bound yet, so we queue the message.
        std::unique_ptr<mojo::Message> queued_message(new mojo::Message);
        message->MoveTo(queued_message.get());
        outgoing_messages_.emplace_back(std::move(queued_message));
        return true;
      }
      return connector_->Accept(message);
    } else {
      // We always post tasks to the master endpoint thread when called from the
      // proxy thread in order to simulate IPC::ChannelProxy::Send behavior.
      DCHECK(proxy_task_runner_->BelongsToCurrentThread());
      std::unique_ptr<mojo::Message> passed_message(new mojo::Message);
      message->MoveTo(passed_message.get());
      task_runner_->PostTask(
          FROM_HERE,
          base::Bind(
              &ChannelAssociatedGroupController::SendMessageOnMasterThread,
              this, base::Passed(&passed_message)));
      return true;
    }
  }

  void SendMessageOnMasterThread(std::unique_ptr<mojo::Message> message) {
    DCHECK(thread_checker_.CalledOnValidThread());
    if (!SendMessage(message.get()))
      RaiseError();
  }

  void OnPipeError() {
    DCHECK(thread_checker_.CalledOnValidThread());

    // We keep |this| alive here because it's possible for the notifications
    // below to release all other references.
    scoped_refptr<ChannelAssociatedGroupController> keepalive(this);

    base::AutoLock locker(lock_);
    encountered_error_ = true;

    std::vector<scoped_refptr<Endpoint>> endpoints_to_notify;
    for (auto iter = endpoints_.begin(); iter != endpoints_.end();) {
      Endpoint* endpoint = iter->second.get();
      ++iter;

      if (endpoint->client())
        endpoints_to_notify.push_back(endpoint);

      MarkPeerClosedAndMaybeRemove(endpoint);
    }

    for (auto& endpoint : endpoints_to_notify) {
      // Because a notification may in turn detach any endpoint, we have to
      // check each client again here.
      if (endpoint->client())
        NotifyEndpointOfError(endpoint.get(), false /* force_async */);
    }
  }

  void NotifyEndpointOfError(Endpoint* endpoint, bool force_async) {
    lock_.AssertAcquired();
    DCHECK(endpoint->task_runner() && endpoint->client());
    if (endpoint->task_runner()->BelongsToCurrentThread() && !force_async) {
      mojo::InterfaceEndpointClient* client = endpoint->client();

      base::AutoUnlock unlocker(lock_);
      client->NotifyError();
    } else {
      endpoint->task_runner()->PostTask(
          FROM_HERE,
          base::Bind(&ChannelAssociatedGroupController
                ::NotifyEndpointOfErrorOnEndpointThread, this,
                make_scoped_refptr(endpoint)));
    }
  }

  void NotifyEndpointOfErrorOnEndpointThread(scoped_refptr<Endpoint> endpoint) {
    base::AutoLock locker(lock_);
    if (!endpoint->client())
      return;
    DCHECK(endpoint->task_runner()->BelongsToCurrentThread());
    NotifyEndpointOfError(endpoint.get(), false /* force_async */);
  }

  void MarkClosedAndMaybeRemove(Endpoint* endpoint) {
    lock_.AssertAcquired();
    endpoint->set_closed();
    if (endpoint->closed() && endpoint->peer_closed())
      endpoints_.erase(endpoint->id());
  }

  void MarkPeerClosedAndMaybeRemove(Endpoint* endpoint) {
    lock_.AssertAcquired();
    endpoint->set_peer_closed();
    if (endpoint->closed() && endpoint->peer_closed())
      endpoints_.erase(endpoint->id());
  }

  Endpoint* FindOrInsertEndpoint(mojo::InterfaceId id, bool* inserted) {
    lock_.AssertAcquired();
    DCHECK(!inserted || !*inserted);

    auto iter = endpoints_.find(id);
    if (iter != endpoints_.end())
      return iter->second.get();

    Endpoint* endpoint = new Endpoint(this, id);
    endpoints_.insert({ id, endpoint });
    if (inserted)
      *inserted = true;
    return endpoint;
  }

  // mojo::MessageReceiver:
  bool Accept(mojo::Message* message) override {
    DCHECK(thread_checker_.CalledOnValidThread());

    if (mojo::PipeControlMessageHandler::IsPipeControlMessage(message))
      return control_message_handler_.Accept(message);

    mojo::InterfaceId id = message->interface_id();
    DCHECK(mojo::IsValidInterfaceId(id));

    base::AutoLock locker(lock_);
    Endpoint* endpoint = GetEndpointForDispatch(id);
    if (!endpoint)
      return true;

    mojo::InterfaceEndpointClient* client = endpoint->client();
    if (!client || !endpoint->task_runner()->BelongsToCurrentThread()) {
      // No client has been bound yet or the client runs tasks on another
      // thread. We assume the other thread must always be the one on which
      // |proxy_task_runner_| runs tasks, since that's the only valid scenario.
      //
      // If the client is not yet bound, it must be bound by the time this task
      // runs or else it's programmer error.
      DCHECK(proxy_task_runner_);
      std::unique_ptr<mojo::Message> passed_message(new mojo::Message);
      message->MoveTo(passed_message.get());
      proxy_task_runner_->PostTask(
          FROM_HERE,
          base::Bind(&ChannelAssociatedGroupController::AcceptOnProxyThread,
                     this, base::Passed(&passed_message)));
      return true;
    }

    // We do not expect to receive sync responses on the master endpoint thread.
    // If it's happening, it's a bug.
    DCHECK(!message->has_flag(mojo::Message::kFlagIsSync));

    base::AutoUnlock unlocker(lock_);
    return client->HandleIncomingMessage(message);
  }

  void AcceptOnProxyThread(std::unique_ptr<mojo::Message> message) {
    DCHECK(proxy_task_runner_->BelongsToCurrentThread());

    mojo::InterfaceId id = message->interface_id();
    DCHECK(mojo::IsValidInterfaceId(id) && !mojo::IsMasterInterfaceId(id));

    base::AutoLock locker(lock_);
    Endpoint* endpoint = GetEndpointForDispatch(id);
    if (!endpoint)
      return;

    mojo::InterfaceEndpointClient* client = endpoint->client();
    if (!client)
      return;

    DCHECK(endpoint->task_runner()->BelongsToCurrentThread());

    // TODO(rockot): Implement sync dispatch. For now, sync messages are
    // unsupported here.
    DCHECK(!message->has_flag(mojo::Message::kFlagIsSync));

    bool result = false;
    {
      base::AutoUnlock unlocker(lock_);
      result = client->HandleIncomingMessage(message.get());
    }

    if (!result)
      RaiseError();
  }

  Endpoint* GetEndpointForDispatch(mojo::InterfaceId id) {
    lock_.AssertAcquired();
    bool inserted = false;
    Endpoint* endpoint = FindOrInsertEndpoint(id, &inserted);
    if (inserted) {
      MarkClosedAndMaybeRemove(endpoint);
      if (!mojo::IsMasterInterfaceId(id))
        control_message_proxy_.NotifyPeerEndpointClosed(id);
      return nullptr;
    }

    if (endpoint->closed())
      return nullptr;

    return endpoint;
  }

  // mojo::PipeControlMessageHandlerDelegate:
  bool OnPeerAssociatedEndpointClosed(mojo::InterfaceId id) override {
    DCHECK(thread_checker_.CalledOnValidThread());

    if (mojo::IsMasterInterfaceId(id))
      return false;

    scoped_refptr<ChannelAssociatedGroupController> keepalive(this);
    base::AutoLock locker(lock_);
    scoped_refptr<Endpoint> endpoint = FindOrInsertEndpoint(id, nullptr);
    if (!endpoint->peer_closed()) {
      if (endpoint->client())
        NotifyEndpointOfError(endpoint.get(), false /* force_async */);
      MarkPeerClosedAndMaybeRemove(endpoint.get());
    }

    return true;
  }

  bool OnAssociatedEndpointClosedBeforeSent(mojo::InterfaceId id) override {
    DCHECK(thread_checker_.CalledOnValidThread());

    if (mojo::IsMasterInterfaceId(id))
      return false;

    base::AutoLock locker(lock_);
    Endpoint* endpoint = FindOrInsertEndpoint(id, nullptr);
    DCHECK(!endpoint->closed());
    MarkClosedAndMaybeRemove(endpoint);
    control_message_proxy_.NotifyPeerEndpointClosed(id);
    return true;
  }

  // Checked in places which must be run on the master endpoint's thread.
  base::ThreadChecker thread_checker_;

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  scoped_refptr<base::SingleThreadTaskRunner> proxy_task_runner_;
  const bool set_interface_id_namespace_bit_;
  std::unique_ptr<mojo::Connector> connector_;
  mojo::MessageHeaderValidator header_validator_;
  mojo::PipeControlMessageHandler control_message_handler_;
  ControlMessageProxyThunk control_message_proxy_thunk_;
  mojo::PipeControlMessageProxy control_message_proxy_;

  // Outgoing messages that were sent before this controller was bound to a
  // real message pipe.
  std::vector<std::unique_ptr<mojo::Message>> outgoing_messages_;

  // Guards the fields below for thread-safe access.
  base::Lock lock_;

  bool encountered_error_ = false;

  // ID #1 is reserved for the mojom::Channel interface.
  uint32_t next_interface_id_ = 2;

  std::map<uint32_t, scoped_refptr<Endpoint>> endpoints_;

  DISALLOW_COPY_AND_ASSIGN(ChannelAssociatedGroupController);
};

class MojoBootstrapImpl : public MojoBootstrap {
 public:
  MojoBootstrapImpl(
      mojo::ScopedMessagePipeHandle handle,
      Delegate* delegate,
      const scoped_refptr<ChannelAssociatedGroupController> controller)
            : controller_(controller),
        handle_(std::move(handle)),
        delegate_(delegate) {
    associated_group_ = controller_->CreateAssociatedGroup();
  }

  ~MojoBootstrapImpl() override {
    controller_->ShutDown();
  }

 private:
  // MojoBootstrap:
  void Connect() override {
    controller_->Bind(std::move(handle_));

    IPC::mojom::ChannelAssociatedPtr sender;
    IPC::mojom::ChannelAssociatedRequest receiver;
    controller_->CreateChannelEndpoints(&sender, &receiver);

    delegate_->OnPipesAvailable(std::move(sender), std::move(receiver));
  }

  mojo::AssociatedGroup* GetAssociatedGroup() override {
    return associated_group_.get();
  }

  scoped_refptr<ChannelAssociatedGroupController> controller_;

  mojo::ScopedMessagePipeHandle handle_;
  Delegate* delegate_;
  std::unique_ptr<mojo::AssociatedGroup> associated_group_;

  DISALLOW_COPY_AND_ASSIGN(MojoBootstrapImpl);
};

}  // namespace

// static
std::unique_ptr<MojoBootstrap> MojoBootstrap::Create(
    mojo::ScopedMessagePipeHandle handle,
    Channel::Mode mode,
    Delegate* delegate,
    const scoped_refptr<base::SingleThreadTaskRunner>& ipc_task_runner) {
  return base::MakeUnique<MojoBootstrapImpl>(
      std::move(handle), delegate,
      new ChannelAssociatedGroupController(mode == Channel::MODE_SERVER,
                                           ipc_task_runner));
}

}  // namespace IPC
