// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/lib/multiplex_router.h"

#include <stdint.h>

#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/sequenced_task_runner.h"
#include "base/stl_util.h"
#include "base/synchronization/waitable_event.h"
#include "mojo/public/cpp/bindings/interface_endpoint_client.h"
#include "mojo/public/cpp/bindings/interface_endpoint_controller.h"
#include "mojo/public/cpp/bindings/lib/may_auto_lock.h"
#include "mojo/public/cpp/bindings/sync_event_watcher.h"

namespace mojo {
namespace internal {

// InterfaceEndpoint stores the information of an interface endpoint registered
// with the router.
// No one other than the router's |endpoints_| and |tasks_| should hold refs to
// this object.
class MultiplexRouter::InterfaceEndpoint
    : public base::RefCountedThreadSafe<InterfaceEndpoint>,
      public InterfaceEndpointController {
 public:
  InterfaceEndpoint(MultiplexRouter* router, InterfaceId id)
      : router_(router),
        id_(id),
        closed_(false),
        peer_closed_(false),
        handle_created_(false),
        client_(nullptr) {}

  // ---------------------------------------------------------------------------
  // The following public methods are safe to call from any sequence without
  // locking.

  InterfaceId id() const { return id_; }

  // ---------------------------------------------------------------------------
  // The following public methods are called under the router's lock.

  bool closed() const { return closed_; }
  void set_closed() {
    router_->AssertLockAcquired();
    closed_ = true;
  }

  bool peer_closed() const { return peer_closed_; }
  void set_peer_closed() {
    router_->AssertLockAcquired();
    peer_closed_ = true;
  }

  bool handle_created() const { return handle_created_; }
  void set_handle_created() {
    router_->AssertLockAcquired();
    handle_created_ = true;
  }

  const base::Optional<DisconnectReason>& disconnect_reason() const {
    return disconnect_reason_;
  }
  void set_disconnect_reason(
      const base::Optional<DisconnectReason>& disconnect_reason) {
    router_->AssertLockAcquired();
    disconnect_reason_ = disconnect_reason;
  }

  base::SequencedTaskRunner* task_runner() const { return task_runner_.get(); }

  InterfaceEndpointClient* client() const { return client_; }

  void AttachClient(InterfaceEndpointClient* client,
                    scoped_refptr<base::SequencedTaskRunner> runner) {
    router_->AssertLockAcquired();
    DCHECK(!client_);
    DCHECK(!closed_);
    DCHECK(runner->RunsTasksInCurrentSequence());

    task_runner_ = std::move(runner);
    client_ = client;
  }

  // This method must be called on the same sequence as the corresponding
  // AttachClient() call.
  void DetachClient() {
    router_->AssertLockAcquired();
    DCHECK(client_);
    DCHECK(task_runner_->RunsTasksInCurrentSequence());
    DCHECK(!closed_);

    task_runner_ = nullptr;
    client_ = nullptr;
    sync_watcher_.reset();
  }

  void SignalSyncMessageEvent() {
    router_->AssertLockAcquired();
    if (sync_message_event_signaled_)
      return;
    sync_message_event_signaled_ = true;
    if (sync_message_event_)
      sync_message_event_->Signal();
  }

  void ResetSyncMessageSignal() {
    router_->AssertLockAcquired();
    if (!sync_message_event_signaled_)
      return;
    sync_message_event_signaled_ = false;
    if (sync_message_event_)
      sync_message_event_->Reset();
  }

  // ---------------------------------------------------------------------------
  // The following public methods (i.e., InterfaceEndpointController
  // implementation) are called by the client on the same sequence as the
  // AttachClient() call. They are called outside of the router's lock.

  bool SendMessage(Message* message) override {
    DCHECK(task_runner_->RunsTasksInCurrentSequence());
    message->set_interface_id(id_);
    return router_->connector_.Accept(message);
  }

  void AllowWokenUpBySyncWatchOnSameThread() override {
    DCHECK(task_runner_->RunsTasksInCurrentSequence());

    EnsureSyncWatcherExists();
    sync_watcher_->AllowWokenUpBySyncWatchOnSameThread();
  }

  bool SyncWatch(const bool* should_stop) override {
    DCHECK(task_runner_->RunsTasksInCurrentSequence());

    EnsureSyncWatcherExists();
    return sync_watcher_->SyncWatch(should_stop);
  }

 private:
  friend class base::RefCountedThreadSafe<InterfaceEndpoint>;

  ~InterfaceEndpoint() override {
    router_->AssertLockAcquired();

    DCHECK(!client_);
    DCHECK(closed_);
    DCHECK(peer_closed_);
    DCHECK(!sync_watcher_);
  }

  void OnSyncEventSignaled() {
    DCHECK(task_runner_->RunsTasksInCurrentSequence());
    scoped_refptr<MultiplexRouter> router_protector(router_);

    MayAutoLock locker(&router_->lock_);
    scoped_refptr<InterfaceEndpoint> self_protector(this);

    bool more_to_process = router_->ProcessFirstSyncMessageForEndpoint(id_);

    if (!more_to_process)
      ResetSyncMessageSignal();

    // Currently there are no queued sync messages and the peer has closed so
    // there won't be incoming sync messages in the future.
    if (!more_to_process && peer_closed_) {
      // If a SyncWatch() call (or multiple ones) of this interface endpoint is
      // on the call stack, resetting the sync watcher will allow it to exit
      // when the call stack unwinds to that frame.
      sync_watcher_.reset();
    }
  }

  void EnsureSyncWatcherExists() {
    DCHECK(task_runner_->RunsTasksInCurrentSequence());
    if (sync_watcher_)
      return;

    {
      MayAutoLock locker(&router_->lock_);
      if (!sync_message_event_) {
        sync_message_event_.emplace(
            base::WaitableEvent::ResetPolicy::MANUAL,
            base::WaitableEvent::InitialState::NOT_SIGNALED);
        if (sync_message_event_signaled_)
          sync_message_event_->Signal();
      }
    }
    sync_watcher_.reset(
        new SyncEventWatcher(&sync_message_event_.value(),
                             base::Bind(&InterfaceEndpoint::OnSyncEventSignaled,
                                        base::Unretained(this))));
  }

  // ---------------------------------------------------------------------------
  // The following members are safe to access from any sequence.

  MultiplexRouter* const router_;
  const InterfaceId id_;

  // ---------------------------------------------------------------------------
  // The following members are accessed under the router's lock.

  // Whether the endpoint has been closed.
  bool closed_;
  // Whether the peer endpoint has been closed.
  bool peer_closed_;

  // Whether there is already a ScopedInterfaceEndpointHandle created for this
  // endpoint.
  bool handle_created_;

  base::Optional<DisconnectReason> disconnect_reason_;

  // The task runner on which |client_|'s methods can be called.
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  // Not owned. It is null if no client is attached to this endpoint.
  InterfaceEndpointClient* client_;

  // An event used to signal that sync messages are available. The event is
  // initialized under the router's lock and remains unchanged afterwards. It
  // may be accessed outside of the router's lock later.
  base::Optional<base::WaitableEvent> sync_message_event_;
  bool sync_message_event_signaled_ = false;

  // ---------------------------------------------------------------------------
  // The following members are only valid while a client is attached. They are
  // used exclusively on the client's sequence. They may be accessed outside of
  // the router's lock.

  std::unique_ptr<SyncEventWatcher> sync_watcher_;

  DISALLOW_COPY_AND_ASSIGN(InterfaceEndpoint);
};

// MessageWrapper objects are always destroyed under the router's lock. On
// destruction, if the message it wrappers contains
// ScopedInterfaceEndpointHandles (which cannot be destructed under the
// router's lock), the wrapper unlocks to clean them up.
class MultiplexRouter::MessageWrapper {
 public:
  MessageWrapper() = default;

  MessageWrapper(MultiplexRouter* router, Message message)
      : router_(router), value_(std::move(message)) {}

  MessageWrapper(MessageWrapper&& other)
      : router_(other.router_), value_(std::move(other.value_)) {}

  ~MessageWrapper() {
    if (value_.associated_endpoint_handles()->empty())
      return;

    router_->AssertLockAcquired();
    {
      MayAutoUnlock unlocker(&router_->lock_);
      value_.mutable_associated_endpoint_handles()->clear();
    }
  }

  MessageWrapper& operator=(MessageWrapper&& other) {
    router_ = other.router_;
    value_ = std::move(other.value_);
    return *this;
  }

  Message& value() { return value_; }

 private:
  MultiplexRouter* router_ = nullptr;
  Message value_;

  DISALLOW_COPY_AND_ASSIGN(MessageWrapper);
};

struct MultiplexRouter::Task {
 public:
  // Doesn't take ownership of |message| but takes its contents.
  static std::unique_ptr<Task> CreateMessageTask(
      MessageWrapper message_wrapper) {
    Task* task = new Task(MESSAGE);
    task->message_wrapper = std::move(message_wrapper);
    return base::WrapUnique(task);
  }
  static std::unique_ptr<Task> CreateNotifyErrorTask(
      InterfaceEndpoint* endpoint) {
    Task* task = new Task(NOTIFY_ERROR);
    task->endpoint_to_notify = endpoint;
    return base::WrapUnique(task);
  }

  ~Task() {}

  bool IsMessageTask() const { return type == MESSAGE; }
  bool IsNotifyErrorTask() const { return type == NOTIFY_ERROR; }

  MessageWrapper message_wrapper;
  scoped_refptr<InterfaceEndpoint> endpoint_to_notify;

  enum Type { MESSAGE, NOTIFY_ERROR };
  Type type;

 private:
  explicit Task(Type in_type) : type(in_type) {}

  DISALLOW_COPY_AND_ASSIGN(Task);
};

MultiplexRouter::MultiplexRouter(
    ScopedMessagePipeHandle message_pipe,
    Config config,
    bool set_interface_id_namesapce_bit,
    scoped_refptr<base::SequencedTaskRunner> runner)
    : set_interface_id_namespace_bit_(set_interface_id_namesapce_bit),
      task_runner_(runner),
      header_validator_(nullptr),
      filters_(this),
      connector_(std::move(message_pipe),
                 config == MULTI_INTERFACE ? Connector::MULTI_THREADED_SEND
                                           : Connector::SINGLE_THREADED_SEND,
                 std::move(runner)),
      control_message_handler_(this),
      control_message_proxy_(&connector_),
      next_interface_id_value_(1),
      posted_to_process_tasks_(false),
      encountered_error_(false),
      paused_(false),
      testing_mode_(false) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  if (config == MULTI_INTERFACE)
    lock_.emplace();

  if (config == SINGLE_INTERFACE_WITH_SYNC_METHODS ||
      config == MULTI_INTERFACE) {
    // Always participate in sync handle watching in multi-interface mode,
    // because even if it doesn't expect sync requests during sync handle
    // watching, it may still need to dispatch messages to associated endpoints
    // on a different sequence.
    connector_.AllowWokenUpBySyncWatchOnSameThread();
  }
  connector_.set_incoming_receiver(&filters_);
  connector_.set_connection_error_handler(base::Bind(
      &MultiplexRouter::OnPipeConnectionError, base::Unretained(this)));

  std::unique_ptr<MessageHeaderValidator> header_validator =
      base::MakeUnique<MessageHeaderValidator>();
  header_validator_ = header_validator.get();
  filters_.Append(std::move(header_validator));
}

MultiplexRouter::~MultiplexRouter() {
  MayAutoLock locker(&lock_);

  sync_message_tasks_.clear();
  tasks_.clear();

  for (auto iter = endpoints_.begin(); iter != endpoints_.end();) {
    InterfaceEndpoint* endpoint = iter->second.get();
    // Increment the iterator before calling UpdateEndpointStateMayRemove()
    // because it may remove the corresponding value from the map.
    ++iter;

    if (!endpoint->closed()) {
      // This happens when a NotifyPeerEndpointClosed message been received, but
      // the interface ID hasn't been used to create local endpoint handle.
      DCHECK(!endpoint->client());
      DCHECK(endpoint->peer_closed());
      UpdateEndpointStateMayRemove(endpoint, ENDPOINT_CLOSED);
    } else {
      UpdateEndpointStateMayRemove(endpoint, PEER_ENDPOINT_CLOSED);
    }
  }

  DCHECK(endpoints_.empty());
}

void MultiplexRouter::SetMasterInterfaceName(const char* name) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  header_validator_->SetDescription(std::string(name) +
                                    " [master] MessageHeaderValidator");
  control_message_handler_.SetDescription(
      std::string(name) + " [master] PipeControlMessageHandler");
  connector_.SetWatcherHeapProfilerTag(name);
}

InterfaceId MultiplexRouter::AssociateInterface(
    ScopedInterfaceEndpointHandle handle_to_send) {
  if (!handle_to_send.pending_association())
    return kInvalidInterfaceId;

  uint32_t id = 0;
  {
    MayAutoLock locker(&lock_);
    do {
      if (next_interface_id_value_ >= kInterfaceIdNamespaceMask)
        next_interface_id_value_ = 1;
      id = next_interface_id_value_++;
      if (set_interface_id_namespace_bit_)
        id |= kInterfaceIdNamespaceMask;
    } while (base::ContainsKey(endpoints_, id));

    InterfaceEndpoint* endpoint = new InterfaceEndpoint(this, id);
    endpoints_[id] = endpoint;
    if (encountered_error_)
      UpdateEndpointStateMayRemove(endpoint, PEER_ENDPOINT_CLOSED);
    endpoint->set_handle_created();
  }

  if (!NotifyAssociation(&handle_to_send, id)) {
    // The peer handle of |handle_to_send|, which is supposed to join this
    // associated group, has been closed.
    {
      MayAutoLock locker(&lock_);
      InterfaceEndpoint* endpoint = FindEndpoint(id);
      if (endpoint)
        UpdateEndpointStateMayRemove(endpoint, ENDPOINT_CLOSED);
    }

    control_message_proxy_.NotifyPeerEndpointClosed(
        id, handle_to_send.disconnect_reason());
  }
  return id;
}

ScopedInterfaceEndpointHandle MultiplexRouter::CreateLocalEndpointHandle(
    InterfaceId id) {
  if (!IsValidInterfaceId(id))
    return ScopedInterfaceEndpointHandle();

  MayAutoLock locker(&lock_);
  bool inserted = false;
  InterfaceEndpoint* endpoint = FindOrInsertEndpoint(id, &inserted);
  if (inserted) {
    DCHECK(!endpoint->handle_created());

    if (encountered_error_)
      UpdateEndpointStateMayRemove(endpoint, PEER_ENDPOINT_CLOSED);
  } else {
    // If the endpoint already exist, it is because we have received a
    // notification that the peer endpoint has closed.
    CHECK(!endpoint->closed());
    CHECK(endpoint->peer_closed());

    if (endpoint->handle_created())
      return ScopedInterfaceEndpointHandle();
  }

  endpoint->set_handle_created();
  return CreateScopedInterfaceEndpointHandle(id);
}

void MultiplexRouter::CloseEndpointHandle(
    InterfaceId id,
    const base::Optional<DisconnectReason>& reason) {
  if (!IsValidInterfaceId(id))
    return;

  MayAutoLock locker(&lock_);
  DCHECK(base::ContainsKey(endpoints_, id));
  InterfaceEndpoint* endpoint = endpoints_[id].get();
  DCHECK(!endpoint->client());
  DCHECK(!endpoint->closed());
  UpdateEndpointStateMayRemove(endpoint, ENDPOINT_CLOSED);

  if (!IsMasterInterfaceId(id) || reason) {
    MayAutoUnlock unlocker(&lock_);
    control_message_proxy_.NotifyPeerEndpointClosed(id, reason);
  }

  ProcessTasks(NO_DIRECT_CLIENT_CALLS, nullptr);
}

InterfaceEndpointController* MultiplexRouter::AttachEndpointClient(
    const ScopedInterfaceEndpointHandle& handle,
    InterfaceEndpointClient* client,
    scoped_refptr<base::SequencedTaskRunner> runner) {
  const InterfaceId id = handle.id();

  DCHECK(IsValidInterfaceId(id));
  DCHECK(client);

  MayAutoLock locker(&lock_);
  DCHECK(base::ContainsKey(endpoints_, id));

  InterfaceEndpoint* endpoint = endpoints_[id].get();
  endpoint->AttachClient(client, std::move(runner));

  if (endpoint->peer_closed())
    tasks_.push_back(Task::CreateNotifyErrorTask(endpoint));
  ProcessTasks(NO_DIRECT_CLIENT_CALLS, nullptr);

  return endpoint;
}

void MultiplexRouter::DetachEndpointClient(
    const ScopedInterfaceEndpointHandle& handle) {
  // TODO(crbug.com/741047): Remove this check.
  CheckObjectIsValid();

  const InterfaceId id = handle.id();

  DCHECK(IsValidInterfaceId(id));

  MayAutoLock locker(&lock_);
  // TODO(crbug.com/741047): change this to DCEHCK.
  CHECK(base::ContainsKey(endpoints_, id));

  InterfaceEndpoint* endpoint = endpoints_[id].get();
  endpoint->DetachClient();
}

void MultiplexRouter::RaiseError() {
  if (task_runner_->RunsTasksInCurrentSequence()) {
    connector_.RaiseError();
  } else {
    task_runner_->PostTask(FROM_HERE,
                           base::Bind(&MultiplexRouter::RaiseError, this));
  }
}

bool MultiplexRouter::PrefersSerializedMessages() {
  MayAutoLock locker(&lock_);
  return connector_.PrefersSerializedMessages();
}

void MultiplexRouter::CloseMessagePipe() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  connector_.CloseMessagePipe();
  // CloseMessagePipe() above won't trigger connection error handler.
  // Explicitly call OnPipeConnectionError() so that associated endpoints will
  // get notified.
  OnPipeConnectionError();
}

void MultiplexRouter::PauseIncomingMethodCallProcessing() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  connector_.PauseIncomingMethodCallProcessing();

  MayAutoLock locker(&lock_);
  paused_ = true;

  for (auto iter = endpoints_.begin(); iter != endpoints_.end(); ++iter)
    iter->second->ResetSyncMessageSignal();
}

void MultiplexRouter::ResumeIncomingMethodCallProcessing() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  connector_.ResumeIncomingMethodCallProcessing();

  MayAutoLock locker(&lock_);
  paused_ = false;

  for (auto iter = endpoints_.begin(); iter != endpoints_.end(); ++iter) {
    auto sync_iter = sync_message_tasks_.find(iter->first);
    if (iter->second->peer_closed() ||
        (sync_iter != sync_message_tasks_.end() &&
         !sync_iter->second.empty())) {
      iter->second->SignalSyncMessageEvent();
    }
  }

  ProcessTasks(NO_DIRECT_CLIENT_CALLS, nullptr);
}

bool MultiplexRouter::HasAssociatedEndpoints() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  MayAutoLock locker(&lock_);

  if (endpoints_.size() > 1)
    return true;
  if (endpoints_.size() == 0)
    return false;

  return !base::ContainsKey(endpoints_, kMasterInterfaceId);
}

void MultiplexRouter::EnableTestingMode() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  MayAutoLock locker(&lock_);

  testing_mode_ = true;
  connector_.set_enforce_errors_from_incoming_receiver(false);
}

bool MultiplexRouter::Accept(Message* message) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (message->is_serialized() &&
      !message->DeserializeAssociatedEndpointHandles(this))
    return false;

  scoped_refptr<MultiplexRouter> protector(this);
  MayAutoLock locker(&lock_);

  DCHECK(!paused_);

  ClientCallBehavior client_call_behavior =
      connector_.during_sync_handle_watcher_callback()
          ? ALLOW_DIRECT_CLIENT_CALLS_FOR_SYNC_MESSAGES
          : ALLOW_DIRECT_CLIENT_CALLS;

  bool processed =
      tasks_.empty() && ProcessIncomingMessage(message, client_call_behavior,
                                               connector_.task_runner());

  if (!processed) {
    // Either the task queue is not empty or we cannot process the message
    // directly. In both cases, there is no need to call ProcessTasks().
    tasks_.push_back(
        Task::CreateMessageTask(MessageWrapper(this, std::move(*message))));
    Task* task = tasks_.back().get();

    if (task->message_wrapper.value().has_flag(Message::kFlagIsSync)) {
      InterfaceId id = task->message_wrapper.value().interface_id();
      sync_message_tasks_[id].push_back(task);
      InterfaceEndpoint* endpoint = FindEndpoint(id);
      if (endpoint)
        endpoint->SignalSyncMessageEvent();
    }
  } else if (!tasks_.empty()) {
    // Processing the message may result in new tasks (for error notification)
    // being added to the queue. In this case, we have to attempt to process the
    // tasks.
    ProcessTasks(client_call_behavior, connector_.task_runner());
  }

  // Always return true. If we see errors during message processing, we will
  // explicitly call Connector::RaiseError() to disconnect the message pipe.
  return true;
}

bool MultiplexRouter::OnPeerAssociatedEndpointClosed(
    InterfaceId id,
    const base::Optional<DisconnectReason>& reason) {
  DCHECK(!IsMasterInterfaceId(id) || reason);

  MayAutoLock locker(&lock_);
  InterfaceEndpoint* endpoint = FindOrInsertEndpoint(id, nullptr);

  if (reason)
    endpoint->set_disconnect_reason(reason);

  // It is possible that this endpoint has been set as peer closed. That is
  // because when the message pipe is closed, all the endpoints are updated with
  // PEER_ENDPOINT_CLOSED. We continue to process remaining tasks in the queue,
  // as long as there are refs keeping the router alive. If there is a
  // PeerAssociatedEndpointClosedEvent control message in the queue, we will get
  // here and see that the endpoint has been marked as peer closed.
  if (!endpoint->peer_closed()) {
    if (endpoint->client())
      tasks_.push_back(Task::CreateNotifyErrorTask(endpoint));
    UpdateEndpointStateMayRemove(endpoint, PEER_ENDPOINT_CLOSED);
  }

  // No need to trigger a ProcessTasks() because it is already on the stack.

  return true;
}

void MultiplexRouter::OnPipeConnectionError() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // TODO(crbug.com/741047): Remove this check.
  CheckObjectIsValid();

  scoped_refptr<MultiplexRouter> protector(this);
  MayAutoLock locker(&lock_);

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

  ProcessTasks(connector_.during_sync_handle_watcher_callback()
                   ? ALLOW_DIRECT_CLIENT_CALLS_FOR_SYNC_MESSAGES
                   : ALLOW_DIRECT_CLIENT_CALLS,
               connector_.task_runner());
}

void MultiplexRouter::ProcessTasks(
    ClientCallBehavior client_call_behavior,
    base::SequencedTaskRunner* current_task_runner) {
  AssertLockAcquired();

  if (posted_to_process_tasks_)
    return;

  while (!tasks_.empty() && !paused_) {
    std::unique_ptr<Task> task(std::move(tasks_.front()));
    tasks_.pop_front();

    InterfaceId id = kInvalidInterfaceId;
    bool sync_message =
        task->IsMessageTask() && !task->message_wrapper.value().IsNull() &&
        task->message_wrapper.value().has_flag(Message::kFlagIsSync);
    if (sync_message) {
      id = task->message_wrapper.value().interface_id();
      auto& sync_message_queue = sync_message_tasks_[id];
      DCHECK_EQ(task.get(), sync_message_queue.front());
      sync_message_queue.pop_front();
    }

    bool processed =
        task->IsNotifyErrorTask()
            ? ProcessNotifyErrorTask(task.get(), client_call_behavior,
                                     current_task_runner)
            : ProcessIncomingMessage(&task->message_wrapper.value(),
                                     client_call_behavior, current_task_runner);

    if (!processed) {
      if (sync_message) {
        auto& sync_message_queue = sync_message_tasks_[id];
        sync_message_queue.push_front(task.get());
      }
      tasks_.push_front(std::move(task));
      break;
    } else {
      if (sync_message) {
        auto iter = sync_message_tasks_.find(id);
        if (iter != sync_message_tasks_.end() && iter->second.empty())
          sync_message_tasks_.erase(iter);
      }
    }
  }
}

bool MultiplexRouter::ProcessFirstSyncMessageForEndpoint(InterfaceId id) {
  AssertLockAcquired();

  auto iter = sync_message_tasks_.find(id);
  if (iter == sync_message_tasks_.end())
    return false;

  if (paused_)
    return true;

  MultiplexRouter::Task* task = iter->second.front();
  iter->second.pop_front();

  DCHECK(task->IsMessageTask());
  MessageWrapper message_wrapper = std::move(task->message_wrapper);

  // Note: after this call, |task| and |iter| may be invalidated.
  bool processed = ProcessIncomingMessage(
      &message_wrapper.value(), ALLOW_DIRECT_CLIENT_CALLS_FOR_SYNC_MESSAGES,
      nullptr);
  DCHECK(processed);

  iter = sync_message_tasks_.find(id);
  if (iter == sync_message_tasks_.end())
    return false;

  if (iter->second.empty()) {
    sync_message_tasks_.erase(iter);
    return false;
  }

  return true;
}

bool MultiplexRouter::ProcessNotifyErrorTask(
    Task* task,
    ClientCallBehavior client_call_behavior,
    base::SequencedTaskRunner* current_task_runner) {
  DCHECK(!current_task_runner ||
         current_task_runner->RunsTasksInCurrentSequence());
  DCHECK(!paused_);

  AssertLockAcquired();
  InterfaceEndpoint* endpoint = task->endpoint_to_notify.get();
  if (!endpoint->client())
    return true;

  if (client_call_behavior != ALLOW_DIRECT_CLIENT_CALLS ||
      endpoint->task_runner() != current_task_runner) {
    MaybePostToProcessTasks(endpoint->task_runner());
    return false;
  }

  DCHECK(endpoint->task_runner()->RunsTasksInCurrentSequence());

  InterfaceEndpointClient* client = endpoint->client();
  base::Optional<DisconnectReason> disconnect_reason(
      endpoint->disconnect_reason());

  {
    // We must unlock before calling into |client| because it may call this
    // object within NotifyError(). Holding the lock will lead to deadlock.
    //
    // It is safe to call into |client| without the lock. Because |client| is
    // always accessed on the same sequence, including DetachEndpointClient().
    MayAutoUnlock unlocker(&lock_);
    client->NotifyError(disconnect_reason);
  }
  return true;
}

bool MultiplexRouter::ProcessIncomingMessage(
    Message* message,
    ClientCallBehavior client_call_behavior,
    base::SequencedTaskRunner* current_task_runner) {
  DCHECK(!current_task_runner ||
         current_task_runner->RunsTasksInCurrentSequence());
  DCHECK(!paused_);
  DCHECK(message);
  AssertLockAcquired();

  if (message->IsNull()) {
    // This is a sync message and has been processed during sync handle
    // watching.
    return true;
  }

  if (PipeControlMessageHandler::IsPipeControlMessage(message)) {
    bool result = false;

    {
      MayAutoUnlock unlocker(&lock_);
      result = control_message_handler_.Accept(message);
    }

    if (!result)
      RaiseErrorInNonTestingMode();

    return true;
  }

  InterfaceId id = message->interface_id();
  DCHECK(IsValidInterfaceId(id));

  InterfaceEndpoint* endpoint = FindEndpoint(id);
  if (!endpoint || endpoint->closed())
    return true;

  if (!endpoint->client()) {
    // We need to wait until a client is attached in order to dispatch further
    // messages.
    return false;
  }

  bool can_direct_call;
  if (message->has_flag(Message::kFlagIsSync)) {
    can_direct_call = client_call_behavior != NO_DIRECT_CLIENT_CALLS &&
                      endpoint->task_runner()->RunsTasksInCurrentSequence();
  } else {
    can_direct_call = client_call_behavior == ALLOW_DIRECT_CLIENT_CALLS &&
                      endpoint->task_runner() == current_task_runner;
  }

  if (!can_direct_call) {
    MaybePostToProcessTasks(endpoint->task_runner());
    return false;
  }

  DCHECK(endpoint->task_runner()->RunsTasksInCurrentSequence());

  InterfaceEndpointClient* client = endpoint->client();
  bool result = false;
  {
    // We must unlock before calling into |client| because it may call this
    // object within HandleIncomingMessage(). Holding the lock will lead to
    // deadlock.
    //
    // It is safe to call into |client| without the lock. Because |client| is
    // always accessed on the same sequence, including DetachEndpointClient().
    MayAutoUnlock unlocker(&lock_);
    result = client->HandleIncomingMessage(message);
  }
  if (!result)
    RaiseErrorInNonTestingMode();

  return true;
}

void MultiplexRouter::MaybePostToProcessTasks(
    base::SequencedTaskRunner* task_runner) {
  AssertLockAcquired();
  if (posted_to_process_tasks_)
    return;

  posted_to_process_tasks_ = true;
  posted_to_task_runner_ = task_runner;
  task_runner->PostTask(
      FROM_HERE, base::Bind(&MultiplexRouter::LockAndCallProcessTasks, this));
}

void MultiplexRouter::LockAndCallProcessTasks() {
  // There is no need to hold a ref to this class in this case because this is
  // always called using base::Bind(), which holds a ref.
  MayAutoLock locker(&lock_);
  posted_to_process_tasks_ = false;
  scoped_refptr<base::SequencedTaskRunner> runner(
      std::move(posted_to_task_runner_));
  ProcessTasks(ALLOW_DIRECT_CLIENT_CALLS, runner.get());
}

void MultiplexRouter::UpdateEndpointStateMayRemove(
    InterfaceEndpoint* endpoint,
    EndpointStateUpdateType type) {
  if (type == ENDPOINT_CLOSED) {
    endpoint->set_closed();
  } else {
    endpoint->set_peer_closed();
    // If the interface endpoint is performing a sync watch, this makes sure
    // it is notified and eventually exits the sync watch.
    endpoint->SignalSyncMessageEvent();
  }
  if (endpoint->closed() && endpoint->peer_closed())
    endpoints_.erase(endpoint->id());
}

void MultiplexRouter::RaiseErrorInNonTestingMode() {
  AssertLockAcquired();
  if (!testing_mode_)
    RaiseError();
}

MultiplexRouter::InterfaceEndpoint* MultiplexRouter::FindOrInsertEndpoint(
    InterfaceId id,
    bool* inserted) {
  AssertLockAcquired();
  // Either |inserted| is nullptr or it points to a boolean initialized as
  // false.
  DCHECK(!inserted || !*inserted);

  InterfaceEndpoint* endpoint = FindEndpoint(id);
  if (!endpoint) {
    endpoint = new InterfaceEndpoint(this, id);
    endpoints_[id] = endpoint;
    if (inserted)
      *inserted = true;
  }

  return endpoint;
}

MultiplexRouter::InterfaceEndpoint* MultiplexRouter::FindEndpoint(
    InterfaceId id) {
  AssertLockAcquired();
  auto iter = endpoints_.find(id);
  return iter != endpoints_.end() ? iter->second.get() : nullptr;
}

void MultiplexRouter::AssertLockAcquired() {
#if DCHECK_IS_ON()
  if (lock_)
    lock_->AssertAcquired();
#endif
}

}  // namespace internal
}  // namespace mojo
