// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "jingle/notifier/listener/non_blocking_push_client.h"

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/message_loop_proxy.h"
#include "base/location.h"
#include "base/logging.h"
#include "jingle/notifier/listener/push_client_observer.h"

namespace notifier {

// All methods are called on the delegate thread unless specified
// otherwise.
class NonBlockingPushClient::Core
    : public base::RefCountedThreadSafe<NonBlockingPushClient::Core>,
      public PushClientObserver {
 public:
  // Called on the parent thread.
  explicit Core(
      const CreateBlockingPushClientCallback&
          create_blocking_push_client_callback,
      const scoped_refptr<base::SingleThreadTaskRunner>&
          delegate_task_runner,
      const base::WeakPtr<NonBlockingPushClient>& parent_push_client);

  void CreateOnDelegateThread(
      const CreateBlockingPushClientCallback&
          create_blocking_push_client_callback);

  // Must be called before being destroyed.
  void DestroyOnDelegateThread();

  void UpdateSubscriptions(const SubscriptionList& subscriptions);
  void UpdateCredentials(const std::string& email, const std::string& token);
  void SendNotification(const Notification& data);

  virtual void OnNotificationStateChange(
      bool notifications_enabled) OVERRIDE;
  virtual void OnIncomingNotification(
      const Notification& notification) OVERRIDE;

 private:
  friend class base::RefCountedThreadSafe<NonBlockingPushClient::Core>;

  // Called on either the parent thread or the delegate thread.
  virtual ~Core();

  const scoped_refptr<base::SingleThreadTaskRunner> parent_task_runner_;
  const scoped_refptr<base::SingleThreadTaskRunner> delegate_task_runner_;

  const base::WeakPtr<NonBlockingPushClient> parent_push_client_;
  scoped_ptr<PushClient> delegate_push_client_;

  DISALLOW_COPY_AND_ASSIGN(Core);
};

NonBlockingPushClient::Core::Core(
    const CreateBlockingPushClientCallback&
        create_blocking_push_client_callback,
    const scoped_refptr<base::SingleThreadTaskRunner>& delegate_task_runner,
    const base::WeakPtr<NonBlockingPushClient>& parent_push_client)
    : parent_task_runner_(base::MessageLoopProxy::current()),
      delegate_task_runner_(delegate_task_runner),
      parent_push_client_(parent_push_client) {
  delegate_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&NonBlockingPushClient::Core::CreateOnDelegateThread,
                 this, create_blocking_push_client_callback));
}

NonBlockingPushClient::Core::~Core() {
  // We should be destroyed on the parent or delegate thread.
  // However, BelongsToCurrentThread() may return false for both if
  // we're destroyed late in shutdown, e.g. after the BrowserThread
  // structures have been destroyed.

  // DestroyOnDelegateThread() may not have been run.  One
  // particular case is when the DestroyOnDelegateThread closure is
  // destroyed by the delegate thread's MessageLoop, which then
  // releases the last reference to this object (although the last
  // reference may end up being released on the parent thread after
  // this); this case occasionally happens on the sync integration
  // tests.
  //
  // In any case, even though nothing may be using
  // |delegate_push_client_| anymore, we can't assume it's safe to
  // delete it, nor can we assume that it's safe to remove ourselves
  // as an observer.  So we just leak it.
  ignore_result(delegate_push_client_.release());
}

void NonBlockingPushClient::Core::CreateOnDelegateThread(
    const CreateBlockingPushClientCallback&
        create_blocking_push_client_callback) {
  DCHECK(delegate_task_runner_->BelongsToCurrentThread());
  delegate_push_client_ = create_blocking_push_client_callback.Run();
  delegate_push_client_->AddObserver(this);
}

void NonBlockingPushClient::Core::DestroyOnDelegateThread() {
  DCHECK(delegate_task_runner_->BelongsToCurrentThread());
  delegate_push_client_->RemoveObserver(this);
  delegate_push_client_.reset();
}

void NonBlockingPushClient::Core::UpdateSubscriptions(
    const SubscriptionList& subscriptions) {
  DCHECK(delegate_task_runner_->BelongsToCurrentThread());
  DCHECK(delegate_push_client_.get());
  delegate_push_client_->UpdateSubscriptions(subscriptions);
}

void NonBlockingPushClient::Core::UpdateCredentials(
      const std::string& email, const std::string& token) {
  DCHECK(delegate_task_runner_->BelongsToCurrentThread());
  DCHECK(delegate_push_client_.get());
  delegate_push_client_->UpdateCredentials(email, token);
}

void NonBlockingPushClient::Core::SendNotification(
    const Notification& notification) {
  DCHECK(delegate_task_runner_->BelongsToCurrentThread());
  DCHECK(delegate_push_client_.get());
  delegate_push_client_->SendNotification(notification);
}

void NonBlockingPushClient::Core::OnNotificationStateChange(
    bool notifications_enabled) {
  DCHECK(delegate_task_runner_->BelongsToCurrentThread());
  parent_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&NonBlockingPushClient::OnNotificationStateChange,
                 parent_push_client_, notifications_enabled));
}

void NonBlockingPushClient::Core::OnIncomingNotification(
    const Notification& notification) {
  DCHECK(delegate_task_runner_->BelongsToCurrentThread());
  parent_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&NonBlockingPushClient::OnIncomingNotification,
                 parent_push_client_, notification));
}

NonBlockingPushClient::NonBlockingPushClient(
    const scoped_refptr<base::SingleThreadTaskRunner>& delegate_task_runner,
    const CreateBlockingPushClientCallback&
        create_blocking_push_client_callback)
    : weak_ptr_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)),
      delegate_task_runner_(delegate_task_runner),
      core_(new Core(create_blocking_push_client_callback,
                     delegate_task_runner_,
                     weak_ptr_factory_.GetWeakPtr())) {}

NonBlockingPushClient::~NonBlockingPushClient() {
  DCHECK(non_thread_safe_.CalledOnValidThread());
  delegate_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&NonBlockingPushClient::Core::DestroyOnDelegateThread,
                 core_.get()));
}

void NonBlockingPushClient::AddObserver(PushClientObserver* observer) {
  DCHECK(non_thread_safe_.CalledOnValidThread());
  observers_.AddObserver(observer);
}

void NonBlockingPushClient::RemoveObserver(PushClientObserver* observer) {
  DCHECK(non_thread_safe_.CalledOnValidThread());
  observers_.RemoveObserver(observer);
}

void NonBlockingPushClient::UpdateSubscriptions(
    const SubscriptionList& subscriptions) {
  DCHECK(non_thread_safe_.CalledOnValidThread());
  delegate_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&NonBlockingPushClient::Core::UpdateSubscriptions,
                 core_.get(), subscriptions));
}

void NonBlockingPushClient::UpdateCredentials(
    const std::string& email, const std::string& token) {
  DCHECK(non_thread_safe_.CalledOnValidThread());
  delegate_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&NonBlockingPushClient::Core::UpdateCredentials,
                 core_.get(), email, token));
}

void NonBlockingPushClient::SendNotification(
    const Notification& notification) {
  DCHECK(non_thread_safe_.CalledOnValidThread());
  delegate_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&NonBlockingPushClient::Core::SendNotification, core_.get(),
                 notification));
}

void NonBlockingPushClient::OnNotificationStateChange(
    bool notifications_enabled) {
  DCHECK(non_thread_safe_.CalledOnValidThread());
  FOR_EACH_OBSERVER(PushClientObserver, observers_,
                    OnNotificationStateChange(notifications_enabled));
}

void NonBlockingPushClient::OnIncomingNotification(
    const Notification& notification) {
  DCHECK(non_thread_safe_.CalledOnValidThread());
  FOR_EACH_OBSERVER(PushClientObserver, observers_,
                    OnIncomingNotification(notification));
}

}  // namespace notifier
