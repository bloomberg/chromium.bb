// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/lib/multiplex_router.h"

#include <stdint.h>

#include <utility>

#include "base/bind.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/stl_util.h"
#include "mojo/public/cpp/bindings/associated_group.h"
#include "mojo/public/cpp/bindings/lib/interface_endpoint_client.h"

namespace mojo {
namespace internal {

// InterfaceEndpoint stores the information of an interface endpoint registered
// with the router. Always accessed under the router's lock.
// No one other than the router's |endpoints_| and |tasks_| should hold refs to
// this object.
class MultiplexRouter::InterfaceEndpoint
    : public base::RefCounted<InterfaceEndpoint> {
 public:
  InterfaceEndpoint(MultiplexRouter* router, InterfaceId id)
      : router_lock_(&router->lock_),
        id_(id),
        closed_(false),
        peer_closed_(false),
        client_(nullptr) {
    router_lock_->AssertAcquired();
  }

  InterfaceId id() const { return id_; }

  bool closed() const { return closed_; }
  void set_closed() {
    router_lock_->AssertAcquired();
    closed_ = true;
  }

  bool peer_closed() const { return peer_closed_; }
  void set_peer_closed() {
    router_lock_->AssertAcquired();
    peer_closed_ = true;
  }

  const scoped_refptr<base::SingleThreadTaskRunner> task_runner() const {
    return task_runner_;
  }
  void set_task_runner(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
    router_lock_->AssertAcquired();
    task_runner_ = std::move(task_runner);
  }

  InterfaceEndpointClient* client() const { return client_; }
  void set_client(InterfaceEndpointClient* client) {
    router_lock_->AssertAcquired();
    client_ = client;
  }

 private:
  friend class base::RefCounted<InterfaceEndpoint>;

  ~InterfaceEndpoint() {
    router_lock_->AssertAcquired();

    DCHECK(!client_);
    DCHECK(closed_);
    DCHECK(peer_closed_);
  }

  base::Lock* const router_lock_;
  const InterfaceId id_;

  // Whether the endpoint has been closed.
  bool closed_;
  // Whether the peer endpoint has been closed.
  bool peer_closed_;

  // The task runner on which |client_| can be accessed.
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  // Not owned. It is null if no client is attached to this endpoint.
  InterfaceEndpointClient* client_;

  DISALLOW_COPY_AND_ASSIGN(InterfaceEndpoint);
};

struct MultiplexRouter::Task {
 public:
  // Doesn't take ownership of |message| but takes its contents.
  static scoped_ptr<Task> CreateIncomingMessageTask(Message* message) {
    Task* task = new Task();
    task->message.reset(new Message);
    message->MoveTo(task->message.get());
    return make_scoped_ptr(task);
  }
  static scoped_ptr<Task> CreateNotifyErrorTask(InterfaceEndpoint* endpoint) {
    Task* task = new Task();
    task->endpoint_to_notify = endpoint;
    return make_scoped_ptr(task);
  }

  ~Task() {}

  bool IsIncomingMessageTask() const { return !!message; }
  bool IsNotifyErrorTask() const { return !!endpoint_to_notify; }

  scoped_ptr<Message> message;
  scoped_refptr<InterfaceEndpoint> endpoint_to_notify;

 private:
  Task() {}
};

MultiplexRouter::MultiplexRouter(bool set_interface_id_namesapce_bit,
                                 ScopedMessagePipeHandle message_pipe,
                                 const MojoAsyncWaiter* waiter)
    : RefCountedDeleteOnMessageLoop(base::MessageLoop::current()
                                        ->task_runner()),
      set_interface_id_namespace_bit_(set_interface_id_namesapce_bit),
      header_validator_(this),
      connector_(std::move(message_pipe),
                 Connector::MULTI_THREADED_SEND,
                 waiter),
      encountered_error_(false),
      control_message_handler_(this),
      control_message_proxy_(&connector_),
      next_interface_id_value_(1),
      testing_mode_(false) {
  connector_.set_incoming_receiver(&header_validator_);
  connector_.set_connection_error_handler(
      [this]() { OnPipeConnectionError(); });
}

MultiplexRouter::~MultiplexRouter() {
  base::AutoLock locker(lock_);

  tasks_.clear();

  for (auto iter = endpoints_.begin(); iter != endpoints_.end();) {
    InterfaceEndpoint* endpoint = iter->second.get();
    // Increment the iterator before calling UpdateEndpointStateMayRemove()
    // because it may remove the corresponding value from the map.
    ++iter;

    DCHECK(endpoint->closed());
    UpdateEndpointStateMayRemove(endpoint, PEER_ENDPOINT_CLOSED);
  }

  DCHECK(endpoints_.empty());
}

void MultiplexRouter::CreateEndpointHandlePair(
    ScopedInterfaceEndpointHandle* local_endpoint,
    ScopedInterfaceEndpointHandle* remote_endpoint) {
  base::AutoLock locker(lock_);
  uint32_t id = 0;
  do {
    if (next_interface_id_value_ >= kInterfaceIdNamespaceMask)
      next_interface_id_value_ = 1;
    id = next_interface_id_value_++;
    if (set_interface_id_namespace_bit_)
      id |= kInterfaceIdNamespaceMask;
  } while (ContainsKey(endpoints_, id));

  InterfaceEndpoint* endpoint = new InterfaceEndpoint(this, id);
  endpoints_[id] = endpoint;
  if (encountered_error_)
    UpdateEndpointStateMayRemove(endpoint, PEER_ENDPOINT_CLOSED);

  *local_endpoint = ScopedInterfaceEndpointHandle(id, true, this);
  *remote_endpoint = ScopedInterfaceEndpointHandle(id, false, this);
}

ScopedInterfaceEndpointHandle MultiplexRouter::CreateLocalEndpointHandle(
    InterfaceId id) {
  if (!IsValidInterfaceId(id))
    return ScopedInterfaceEndpointHandle();

  base::AutoLock locker(lock_);
  if (ContainsKey(endpoints_, id)) {
    // If the endpoint already exist, it is because we have received a
    // notification that the peer endpoint has closed.
    InterfaceEndpoint* endpoint = endpoints_[id].get();
    CHECK(!endpoint->closed());
    CHECK(endpoint->peer_closed());
  } else {
    InterfaceEndpoint* endpoint = new InterfaceEndpoint(this, id);
    endpoints_[id] = endpoint;
    if (encountered_error_)
      UpdateEndpointStateMayRemove(endpoint, PEER_ENDPOINT_CLOSED);
  }
  return ScopedInterfaceEndpointHandle(id, true, this);
}

void MultiplexRouter::CloseEndpointHandle(InterfaceId id, bool is_local) {
  if (!IsValidInterfaceId(id))
    return;

  base::AutoLock locker(lock_);

  if (!is_local) {
    DCHECK(ContainsKey(endpoints_, id));
    DCHECK(!IsMasterInterfaceId(id));

    // We will receive a NotifyPeerEndpointClosed message from the other side.
    control_message_proxy_.NotifyEndpointClosedBeforeSent(id);

    return;
  }

  DCHECK(ContainsKey(endpoints_, id));
  InterfaceEndpoint* endpoint = endpoints_[id].get();
  DCHECK(!endpoint->client());
  DCHECK(!endpoint->closed());
  UpdateEndpointStateMayRemove(endpoint, ENDPOINT_CLOSED);

  if (!IsMasterInterfaceId(id))
    control_message_proxy_.NotifyPeerEndpointClosed(id);

  ProcessTasks(true);
}

void MultiplexRouter::AttachEndpointClient(
    const ScopedInterfaceEndpointHandle& handle,
    InterfaceEndpointClient* client) {
  const InterfaceId id = handle.id();

  DCHECK(IsValidInterfaceId(id));
  DCHECK(client);

  base::AutoLock locker(lock_);
  DCHECK(ContainsKey(endpoints_, id));

  InterfaceEndpoint* endpoint = endpoints_[id].get();
  DCHECK(!endpoint->client());
  DCHECK(!endpoint->closed());

  endpoint->set_task_runner(base::MessageLoop::current()->task_runner());
  endpoint->set_client(client);

  if (endpoint->peer_closed())
    tasks_.push_back(Task::CreateNotifyErrorTask(endpoint));
  ProcessTasks(true);
}

void MultiplexRouter::DetachEndpointClient(
    const ScopedInterfaceEndpointHandle& handle) {
  const InterfaceId id = handle.id();

  DCHECK(IsValidInterfaceId(id));

  base::AutoLock locker(lock_);
  DCHECK(ContainsKey(endpoints_, id));

  InterfaceEndpoint* endpoint = endpoints_[id].get();
  DCHECK(endpoint->client());
  DCHECK(endpoint->task_runner()->BelongsToCurrentThread());
  DCHECK(!endpoint->closed());

  endpoint->set_task_runner(nullptr);
  endpoint->set_client(nullptr);
}

bool MultiplexRouter::SendMessage(const ScopedInterfaceEndpointHandle& handle,
                                  Message* message) {
  const InterfaceId id = handle.id();

  base::AutoLock locker(lock_);
  if (!ContainsKey(endpoints_, id))
    return false;

  InterfaceEndpoint* endpoint = endpoints_[id].get();
  if (endpoint->peer_closed())
    return false;

  message->set_interface_id(id);
  return connector_.Accept(message);
}

void MultiplexRouter::RaiseError() {
  if (task_runner_->BelongsToCurrentThread()) {
    connector_.RaiseError();
  } else {
    task_runner_->PostTask(FROM_HERE,
                           base::Bind(&MultiplexRouter::RaiseError, this));
  }
}

scoped_ptr<AssociatedGroup> MultiplexRouter::CreateAssociatedGroup() {
  scoped_ptr<AssociatedGroup> group(new AssociatedGroup);
  group->router_ = this;
  return group;
}

// static
MultiplexRouter* MultiplexRouter::GetRouter(AssociatedGroup* associated_group) {
  return associated_group->router_.get();
}

bool MultiplexRouter::HasAssociatedEndpoints() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  base::AutoLock locker(lock_);

  if (endpoints_.size() > 1)
    return true;
  if (endpoints_.size() == 0)
    return false;

  return !ContainsKey(endpoints_, kMasterInterfaceId);
}

void MultiplexRouter::EnableTestingMode() {
  DCHECK(thread_checker_.CalledOnValidThread());
  base::AutoLock locker(lock_);

  testing_mode_ = true;
  connector_.set_enforce_errors_from_incoming_receiver(false);
}

bool MultiplexRouter::Accept(Message* message) {
  DCHECK(thread_checker_.CalledOnValidThread());

  scoped_refptr<MultiplexRouter> protector(this);
  base::AutoLock locker(lock_);
  tasks_.push_back(Task::CreateIncomingMessageTask(message));
  ProcessTasks(false);

  // Always return true. If we see errors during message processing, we will
  // explicitly call Connector::RaiseError() to disconnect the message pipe.
  return true;
}

bool MultiplexRouter::OnPeerAssociatedEndpointClosed(InterfaceId id) {
  lock_.AssertAcquired();

  if (IsMasterInterfaceId(id))
    return false;

  if (!ContainsKey(endpoints_, id))
    endpoints_[id] = new InterfaceEndpoint(this, id);

  InterfaceEndpoint* endpoint = endpoints_[id].get();
  DCHECK(!endpoint->peer_closed());

  if (endpoint->client())
    tasks_.push_back(Task::CreateNotifyErrorTask(endpoint));
  UpdateEndpointStateMayRemove(endpoint, PEER_ENDPOINT_CLOSED);

  // No need to trigger a ProcessTasks() because it is already on the stack.

  return true;
}

bool MultiplexRouter::OnAssociatedEndpointClosedBeforeSent(InterfaceId id) {
  lock_.AssertAcquired();

  if (IsMasterInterfaceId(id))
    return false;

  if (!ContainsKey(endpoints_, id))
    endpoints_[id] = new InterfaceEndpoint(this, id);

  InterfaceEndpoint* endpoint = endpoints_[id].get();
  DCHECK(!endpoint->closed());
  UpdateEndpointStateMayRemove(endpoint, ENDPOINT_CLOSED);

  control_message_proxy_.NotifyPeerEndpointClosed(id);

  return true;
}

void MultiplexRouter::OnPipeConnectionError() {
  DCHECK(thread_checker_.CalledOnValidThread());

  scoped_refptr<MultiplexRouter> protector(this);
  base::AutoLock locker(lock_);

  encountered_error_ = true;

  for (auto iter = endpoints_.begin(); iter != endpoints_.end();) {
    InterfaceEndpoint* endpoint = iter->second.get();
    // Increment the iterator before calling UpdateEndpointStateMayRemove()
    // because it may remove the corresponding value from the map.
    ++iter;

    if (endpoint->client())
      tasks_.push_back(Task::CreateNotifyErrorTask(endpoint));

    UpdateEndpointStateMayRemove(endpoint, PEER_ENDPOINT_CLOSED);
  }

  ProcessTasks(false);
}

void MultiplexRouter::ProcessTasks(bool force_async) {
  lock_.AssertAcquired();

  while (!tasks_.empty()) {
    scoped_ptr<Task> task(std::move(tasks_.front()));
    tasks_.pop_front();

    bool processed = task->IsNotifyErrorTask()
                         ? ProcessNotifyErrorTask(task.get(), &force_async)
                         : ProcessIncomingMessageTask(task.get(), &force_async);

    if (!processed) {
      tasks_.push_front(std::move(task));
      break;
    }
  }
}

bool MultiplexRouter::ProcessNotifyErrorTask(Task* task, bool* force_async) {
  lock_.AssertAcquired();
  InterfaceEndpoint* endpoint = task->endpoint_to_notify.get();
  if (!endpoint->client())
    return true;

  if (!endpoint->task_runner()->BelongsToCurrentThread() || *force_async) {
    endpoint->task_runner()->PostTask(
        FROM_HERE, base::Bind(&MultiplexRouter::LockAndCallProcessTasks, this));
    return false;
  }

  *force_async = true;
  InterfaceEndpointClient* client = endpoint->client();
  {
    // We must unlock before calling into |client| because it may call this
    // object within NotifyError(). Holding the lock will lead to deadlock.
    //
    // It is safe to call into |client| without the lock. Because |client| is
    // always accessed on the same thread, including DetachEndpointClient().
    base::AutoUnlock unlocker(lock_);
    client->NotifyError();
  }
  return true;
}

bool MultiplexRouter::ProcessIncomingMessageTask(Task* task,
                                                 bool* force_async) {
  lock_.AssertAcquired();
  Message* message = task->message.get();

  if (PipeControlMessageHandler::IsPipeControlMessage(message)) {
    if (!control_message_handler_.Accept(message))
      RaiseErrorInNonTestingMode();
    return true;
  }

  InterfaceId id = message->interface_id();
  DCHECK(IsValidInterfaceId(id));

  if (!ContainsKey(endpoints_, id)) {
    DCHECK(!IsMasterInterfaceId(id));

    // Currently, it is legitimate to receive messages for an endpoint
    // that is not registered. For example, the endpoint is transferred in
    // a message that is discarded. Once we add support to specify all
    // enclosing endpoints in message header, we should be able to remove
    // this.
    InterfaceEndpoint* endpoint = new InterfaceEndpoint(this, id);
    endpoints_[id] = endpoint;
    UpdateEndpointStateMayRemove(endpoint, ENDPOINT_CLOSED);

    control_message_proxy_.NotifyPeerEndpointClosed(id);
    return true;
  }

  InterfaceEndpoint* endpoint = endpoints_[id].get();
  if (endpoint->closed())
    return true;

  if (!endpoint->client()) {
    // We need to wait until a client is attached in order to dispatch further
    // messages.
    return false;
  }

  if (!endpoint->task_runner()->BelongsToCurrentThread() || *force_async) {
    endpoint->task_runner()->PostTask(
        FROM_HERE, base::Bind(&MultiplexRouter::LockAndCallProcessTasks, this));
    return false;
  }

  *force_async = true;
  InterfaceEndpointClient* client = endpoint->client();
  scoped_ptr<Message> owned_message = std::move(task->message);
  bool result = false;
  {
    // We must unlock before calling into |client| because it may call this
    // object within HandleIncomingMessage(). Holding the lock will lead to
    // deadlock.
    //
    // It is safe to call into |client| without the lock. Because |client| is
    // always accessed on the same thread, including DetachEndpointClient().
    base::AutoUnlock unlocker(lock_);
    result = client->HandleIncomingMessage(owned_message.get());
  }
  if (!result)
    RaiseErrorInNonTestingMode();

  return true;
}

void MultiplexRouter::LockAndCallProcessTasks() {
  // There is no need to hold a ref to this class in this case because this is
  // always called using base::Bind(), which holds a ref.
  base::AutoLock locker(lock_);
  ProcessTasks(false);
}

void MultiplexRouter::UpdateEndpointStateMayRemove(
    InterfaceEndpoint* endpoint,
    EndpointStateUpdateType type) {
  switch (type) {
    case ENDPOINT_CLOSED:
      endpoint->set_closed();
      break;
    case PEER_ENDPOINT_CLOSED:
      endpoint->set_peer_closed();
      break;
  }
  if (endpoint->closed() && endpoint->peer_closed())
    endpoints_.erase(endpoint->id());
}

void MultiplexRouter::RaiseErrorInNonTestingMode() {
  lock_.AssertAcquired();
  if (!testing_mode_)
    RaiseError();
}

}  // namespace internal
}  // namespace mojo
