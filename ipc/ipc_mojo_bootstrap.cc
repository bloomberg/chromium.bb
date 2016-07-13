// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ipc/ipc_mojo_bootstrap.h"

#include <stdint.h>

#include <map>
#include <memory>
#include <utility>

#include "base/callback.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/process/process_handle.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "ipc/ipc_message_utils.h"
#include "ipc/ipc_platform_file.h"
#include "mojo/public/cpp/bindings/associated_group.h"
#include "mojo/public/cpp/bindings/associated_group_controller.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/bindings/connector.h"
#include "mojo/public/cpp/bindings/interface_endpoint_client.h"
#include "mojo/public/cpp/bindings/interface_endpoint_controller.h"
#include "mojo/public/cpp/bindings/interface_id.h"
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
  ChannelAssociatedGroupController(bool set_interface_id_namespace_bit,
                                   mojo::ScopedMessagePipeHandle handle)
      : mojo::AssociatedGroupController(base::ThreadTaskRunnerHandle::Get()),
        task_runner_(base::ThreadTaskRunnerHandle::Get()),
        id_namespace_mask_(set_interface_id_namespace_bit ?
              mojo::kInterfaceIdNamespaceMask : 0),
        associated_group_(CreateAssociatedGroup()),
        connector_(std::move(handle), mojo::Connector::SINGLE_THREADED_SEND,
                   base::ThreadTaskRunnerHandle::Get()),
        header_validator_(
            "IPC::mojom::Bootstrap [master] MessageHeaderValidator", this),
        control_message_handler_(this),
        control_message_proxy_(&connector_) {
    connector_.set_incoming_receiver(&header_validator_);
    connector_.set_connection_error_handler(
        base::Bind(&ChannelAssociatedGroupController::OnPipeError,
                   base::Unretained(this)));
    control_message_handler_.SetDescription(
        "IPC::mojom::Bootstrap [master] PipeControlMessageHandler");
  }

  mojo::AssociatedGroup* associated_group() { return associated_group_.get(); }

  void ShutDown() {
    DCHECK(thread_checker_.CalledOnValidThread());
    connector_.CloseMessagePipe();
    OnPipeError();
    associated_group_.reset();
  }

  void SetProxyTaskRunner(
      scoped_refptr<base::SingleThreadTaskRunner> proxy_task_runner) {
    proxy_task_runner_ = proxy_task_runner;
  }

  // mojo::AssociatedGroupController:
  void CreateEndpointHandlePair(
      mojo::ScopedInterfaceEndpointHandle* local_endpoint,
      mojo::ScopedInterfaceEndpointHandle* remote_endpoint) override {
    base::AutoLock locker(lock_);
    uint32_t id = 0;
    do {
      if (next_interface_id_ >= mojo::kInterfaceIdNamespaceMask)
        next_interface_id_ = 1;
      id = (next_interface_id_++) | id_namespace_mask_;
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
      connector_.RaiseError();
    } else {
      task_runner_->PostTask(
          FROM_HERE,
          base::Bind(&ChannelAssociatedGroupController::RaiseError, this));
    }
  }

 private:
  class Endpoint;
  friend class Endpoint;

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

  ~ChannelAssociatedGroupController() override {
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
      return connector_.Accept(message);
    } else {
      // We always post tasks to the master endpoint thread when called from the
      // proxy thread in order to simulate IPC::ChannelProxy::Send behavior.
      DCHECK(proxy_task_runner_ &&
             proxy_task_runner_->BelongsToCurrentThread());
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
      // Because an notification may in turn detach any endpoint, we have to
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

    if (mojo::PipeControlMessageHandler::IsPipeControlMessage(message)) {
      if (!control_message_handler_.Accept(message))
        RaiseError();
      return true;
    }

    mojo::InterfaceId id = message->interface_id();
    DCHECK(mojo::IsValidInterfaceId(id));

    base::AutoLock locker(lock_);
    bool inserted = false;
    Endpoint* endpoint = FindOrInsertEndpoint(id, &inserted);
    if (inserted) {
      MarkClosedAndMaybeRemove(endpoint);
      if (!mojo::IsMasterInterfaceId(id))
        control_message_proxy_.NotifyPeerEndpointClosed(id);
      return true;
    }

    if (endpoint->closed())
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
      CHECK(false);
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

    bool result = false;
    {
      base::AutoUnlock unlocker(lock_);
      result = client->HandleIncomingMessage(message);
    }

    if (!result)
      RaiseError();

    return true;
  }

  void AcceptOnProxyThread(std::unique_ptr<mojo::Message> message) {
    DCHECK(proxy_task_runner_->BelongsToCurrentThread());

    // TODO(rockot): Implement this.
    NOTREACHED();
  }

  // mojo::PipeControlMessageHandlerDelegate:
  bool OnPeerAssociatedEndpointClosed(mojo::InterfaceId id) override {
    DCHECK(thread_checker_.CalledOnValidThread());

    if (mojo::IsMasterInterfaceId(id))
      return false;

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
  const uint32_t id_namespace_mask_;
  std::unique_ptr<mojo::AssociatedGroup> associated_group_;
  mojo::Connector connector_;
  mojo::MessageHeaderValidator header_validator_;
  mojo::PipeControlMessageHandler control_message_handler_;

  // Guards the fields below for thread-safe access.
  base::Lock lock_;

  bool encountered_error_ = false;
  uint32_t next_interface_id_ = 1;
  std::map<uint32_t, scoped_refptr<Endpoint>> endpoints_;
  mojo::PipeControlMessageProxy control_message_proxy_;

  DISALLOW_COPY_AND_ASSIGN(ChannelAssociatedGroupController);
};

class BootstrapMasterProxy {
 public:
  BootstrapMasterProxy() {}
  ~BootstrapMasterProxy() {
    endpoint_client_.reset();
    proxy_.reset();
    if (controller_)
      controller_->ShutDown();
  }

  void Bind(mojo::ScopedMessagePipeHandle handle) {
    DCHECK(!controller_);
    controller_ = new ChannelAssociatedGroupController(true, std::move(handle));
    endpoint_client_.reset(new mojo::InterfaceEndpointClient(
        controller_->CreateLocalEndpointHandle(mojo::kMasterInterfaceId),
        nullptr,
        base::MakeUnique<typename mojom::Bootstrap::ResponseValidator_>(),
        false, base::ThreadTaskRunnerHandle::Get()));
    proxy_.reset(new mojom::BootstrapProxy(endpoint_client_.get()));
    proxy_->serialization_context()->group_controller = controller_;
  }

  void set_connection_error_handler(const base::Closure& handler) {
    DCHECK(endpoint_client_);
    endpoint_client_->set_connection_error_handler(handler);
  }

  mojo::AssociatedGroup* associated_group() {
    DCHECK(controller_);
    return controller_->associated_group();
  }

  mojom::Bootstrap* operator->() {
    DCHECK(proxy_);
    return proxy_.get();
  }

 private:
  std::unique_ptr<mojom::BootstrapProxy> proxy_;
  scoped_refptr<ChannelAssociatedGroupController> controller_;
  std::unique_ptr<mojo::InterfaceEndpointClient> endpoint_client_;

  DISALLOW_COPY_AND_ASSIGN(BootstrapMasterProxy);
};

class BootstrapMasterBinding {
 public:
  explicit BootstrapMasterBinding(mojom::Bootstrap* impl) {
    stub_.set_sink(impl);
  }

  ~BootstrapMasterBinding() {
    endpoint_client_.reset();
    if (controller_)
      controller_->ShutDown();
  }

  void set_connection_error_handler(const base::Closure& handler) {
    DCHECK(endpoint_client_);
    endpoint_client_->set_connection_error_handler(handler);
  }

  void Bind(mojo::ScopedMessagePipeHandle handle) {
    DCHECK(!controller_);
    controller_ =
        new ChannelAssociatedGroupController(false, std::move(handle));
    stub_.serialization_context()->group_controller = controller_;

    endpoint_client_.reset(new mojo::InterfaceEndpointClient(
        controller_->CreateLocalEndpointHandle(mojo::kMasterInterfaceId),
        &stub_,
        base::MakeUnique<typename mojom::Bootstrap::RequestValidator_>(),
        false, base::ThreadTaskRunnerHandle::Get()));
  }

 private:
  mojom::BootstrapStub stub_;
  scoped_refptr<ChannelAssociatedGroupController> controller_;
  std::unique_ptr<mojo::InterfaceEndpointClient> endpoint_client_;

  DISALLOW_COPY_AND_ASSIGN(BootstrapMasterBinding);
};

// MojoBootstrap for the server process. You should create the instance
// using MojoBootstrap::Create().
class MojoServerBootstrap : public MojoBootstrap {
 public:
  MojoServerBootstrap();

 private:
  // MojoBootstrap implementation.
  void Connect() override;

  void OnInitDone(int32_t peer_pid);

  BootstrapMasterProxy bootstrap_;
  IPC::mojom::ChannelAssociatedPtrInfo send_channel_;
  IPC::mojom::ChannelAssociatedRequest receive_channel_request_;

  DISALLOW_COPY_AND_ASSIGN(MojoServerBootstrap);
};

MojoServerBootstrap::MojoServerBootstrap() = default;

void MojoServerBootstrap::Connect() {
  DCHECK_EQ(state(), STATE_INITIALIZED);

  bootstrap_.Bind(TakeHandle());
  bootstrap_.set_connection_error_handler(
      base::Bind(&MojoServerBootstrap::Fail, base::Unretained(this)));

  IPC::mojom::ChannelAssociatedRequest send_channel_request;
  IPC::mojom::ChannelAssociatedPtrInfo receive_channel;

  bootstrap_.associated_group()->CreateAssociatedInterface(
      mojo::AssociatedGroup::WILL_PASS_REQUEST, &send_channel_,
      &send_channel_request);
  bootstrap_.associated_group()->CreateAssociatedInterface(
      mojo::AssociatedGroup::WILL_PASS_PTR, &receive_channel,
      &receive_channel_request_);

  bootstrap_->Init(
      std::move(send_channel_request), std::move(receive_channel),
      GetSelfPID(),
      base::Bind(&MojoServerBootstrap::OnInitDone, base::Unretained(this)));

  set_state(STATE_WAITING_ACK);
}

void MojoServerBootstrap::OnInitDone(int32_t peer_pid) {
  if (state() != STATE_WAITING_ACK) {
    set_state(STATE_ERROR);
    LOG(ERROR) << "Got inconsistent message from client.";
    return;
  }

  set_state(STATE_READY);
  bootstrap_.set_connection_error_handler(base::Closure());
  delegate()->OnPipesAvailable(std::move(send_channel_),
                               std::move(receive_channel_request_), peer_pid);
}

// MojoBootstrap for client processes. You should create the instance
// using MojoBootstrap::Create().
class MojoClientBootstrap : public MojoBootstrap, public mojom::Bootstrap {
 public:
  MojoClientBootstrap();

 private:
  // MojoBootstrap implementation.
  void Connect() override;

  // mojom::Bootstrap implementation.
  void Init(mojom::ChannelAssociatedRequest receive_channel,
            mojom::ChannelAssociatedPtrInfo send_channel,
            int32_t peer_pid,
            const InitCallback& callback) override;

  BootstrapMasterBinding binding_;

  DISALLOW_COPY_AND_ASSIGN(MojoClientBootstrap);
};

MojoClientBootstrap::MojoClientBootstrap() : binding_(this) {}

void MojoClientBootstrap::Connect() {
  binding_.Bind(TakeHandle());
  binding_.set_connection_error_handler(
      base::Bind(&MojoClientBootstrap::Fail, base::Unretained(this)));
}

void MojoClientBootstrap::Init(mojom::ChannelAssociatedRequest receive_channel,
                               mojom::ChannelAssociatedPtrInfo send_channel,
                               int32_t peer_pid,
                               const InitCallback& callback) {
  callback.Run(GetSelfPID());
  set_state(STATE_READY);
  binding_.set_connection_error_handler(base::Closure());
  delegate()->OnPipesAvailable(std::move(send_channel),
                               std::move(receive_channel), peer_pid);
}

}  // namespace

// MojoBootstrap

// static
std::unique_ptr<MojoBootstrap> MojoBootstrap::Create(
    mojo::ScopedMessagePipeHandle handle,
    Channel::Mode mode,
    Delegate* delegate) {
  CHECK(mode == Channel::MODE_CLIENT || mode == Channel::MODE_SERVER);
  std::unique_ptr<MojoBootstrap> self =
      mode == Channel::MODE_CLIENT
          ? std::unique_ptr<MojoBootstrap>(new MojoClientBootstrap)
          : std::unique_ptr<MojoBootstrap>(new MojoServerBootstrap);

  self->Init(std::move(handle), delegate);
  return self;
}

MojoBootstrap::MojoBootstrap() : delegate_(NULL), state_(STATE_INITIALIZED) {
}

MojoBootstrap::~MojoBootstrap() {}

void MojoBootstrap::Init(mojo::ScopedMessagePipeHandle handle,
                         Delegate* delegate) {
  handle_ = std::move(handle);
  delegate_ = delegate;
}

base::ProcessId MojoBootstrap::GetSelfPID() const {
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

void MojoBootstrap::Fail() {
  set_state(STATE_ERROR);
  delegate()->OnBootstrapError();
}

bool MojoBootstrap::HasFailed() const {
  return state() == STATE_ERROR;
}

mojo::ScopedMessagePipeHandle MojoBootstrap::TakeHandle() {
  return std::move(handle_);
}

}  // namespace IPC
