// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/notifications/notification_manager.h"

#include "base/lazy_instance.h"
#include "base/strings/utf_string_conversions.h"
#include "base/thread_task_runner_handle.h"
#include "base/threading/thread_local.h"
#include "content/child/notifications/notification_data_conversions.h"
#include "content/child/notifications/notification_dispatcher.h"
#include "content/child/service_worker/web_service_worker_registration_impl.h"
#include "content/child/thread_safe_sender.h"
#include "content/child/worker_task_runner.h"
#include "content/public/common/platform_notification_data.h"
#include "third_party/WebKit/public/platform/WebSerializedOrigin.h"
#include "third_party/WebKit/public/platform/modules/notifications/WebNotificationDelegate.h"
#include "third_party/skia/include/core/SkBitmap.h"

using blink::WebNotificationPermission;

namespace content {
namespace {

int CurrentWorkerId() {
  return WorkerTaskRunner::Instance()->CurrentWorkerId();
}

}  // namespace

static base::LazyInstance<base::ThreadLocalPointer<NotificationManager>>::Leaky
    g_notification_manager_tls = LAZY_INSTANCE_INITIALIZER;

NotificationManager::NotificationManager(
    ThreadSafeSender* thread_safe_sender,
    base::SingleThreadTaskRunner* main_thread_task_runner,
    NotificationDispatcher* notification_dispatcher)
    : thread_safe_sender_(thread_safe_sender),
      notification_dispatcher_(notification_dispatcher),
      pending_notifications_(main_thread_task_runner) {
  g_notification_manager_tls.Pointer()->Set(this);
}

NotificationManager::~NotificationManager() {
  g_notification_manager_tls.Pointer()->Set(nullptr);
}

NotificationManager* NotificationManager::ThreadSpecificInstance(
    ThreadSafeSender* thread_safe_sender,
    base::SingleThreadTaskRunner* main_thread_task_runner,
    NotificationDispatcher* notification_dispatcher) {
  if (g_notification_manager_tls.Pointer()->Get())
    return g_notification_manager_tls.Pointer()->Get();

  NotificationManager* manager = new NotificationManager(
      thread_safe_sender, main_thread_task_runner, notification_dispatcher);
  if (CurrentWorkerId())
    WorkerTaskRunner::Instance()->AddStopObserver(manager);
  return manager;
}

void NotificationManager::OnWorkerRunLoopStopped() {
  delete this;
}

void NotificationManager::show(
    const blink::WebSerializedOrigin& origin,
    const blink::WebNotificationData& notification_data,
    blink::WebNotificationDelegate* delegate) {
  if (notification_data.icon.isEmpty()) {
    DisplayPageNotification(origin, notification_data, delegate, SkBitmap());
    return;
  }

  pending_notifications_.FetchPageNotificationResources(
      notification_data,
      delegate,
      base::Bind(&NotificationManager::DisplayPageNotification,
                 base::Unretained(this),  // this owns |pending_notifications_|
                 origin,
                 notification_data,
                 delegate));
}

void NotificationManager::showPersistent(
    const blink::WebSerializedOrigin& origin,
    const blink::WebNotificationData& notification_data,
    blink::WebServiceWorkerRegistration* service_worker_registration,
    blink::WebNotificationShowCallbacks* callbacks) {
  DCHECK(service_worker_registration);
  int64 service_worker_registration_id =
      static_cast<WebServiceWorkerRegistrationImpl*>(
          service_worker_registration)->registration_id();

  scoped_ptr<blink::WebNotificationShowCallbacks> owned_callbacks(callbacks);
  if (notification_data.icon.isEmpty()) {
    DisplayPersistentNotification(origin,
                                  notification_data,
                                  service_worker_registration_id,
                                  owned_callbacks.Pass(),
                                  SkBitmap());
    return;
  }

  pending_notifications_.FetchPersistentNotificationResources(
      notification_data,
      base::Bind(&NotificationManager::DisplayPersistentNotification,
                 base::Unretained(this),  // this owns |pending_notifications_|
                 origin,
                 notification_data,
                 service_worker_registration_id,
                 base::Passed(&owned_callbacks)));
}

void NotificationManager::getNotifications(
    const blink::WebString& filter_tag,
    blink::WebServiceWorkerRegistration* service_worker_registration,
    blink::WebNotificationGetCallbacks* callbacks) {
  DCHECK(service_worker_registration);
  DCHECK(callbacks);

  WebServiceWorkerRegistrationImpl* service_worker_registration_impl =
      static_cast<WebServiceWorkerRegistrationImpl*>(
          service_worker_registration);

  GURL origin = GURL(service_worker_registration_impl->scope()).GetOrigin();
  int64 service_worker_registration_id =
      service_worker_registration_impl->registration_id();

  // TODO(peter): GenerateNotificationId is more of a request id. Consider
  // renaming the method in the NotificationDispatcher if this makes sense.
  int request_id =
      notification_dispatcher_->GenerateNotificationId(CurrentWorkerId());

  pending_get_notification_requests_.AddWithID(callbacks, request_id);

  thread_safe_sender_->Send(
      new PlatformNotificationHostMsg_GetNotifications(
          request_id,
          service_worker_registration_id,
          origin,
          base::UTF16ToUTF8(filter_tag)));
}

void NotificationManager::close(blink::WebNotificationDelegate* delegate) {
  if (pending_notifications_.CancelPageNotificationFetches(delegate))
    return;

  for (auto& iter : active_page_notifications_) {
    if (iter.second != delegate)
      continue;

    thread_safe_sender_->Send(
        new PlatformNotificationHostMsg_Close(iter.first));
    active_page_notifications_.erase(iter.first);
    return;
  }

  // It should not be possible for Blink to call close() on a Notification which
  // does not exist in either the pending or active notification lists.
  NOTREACHED();
}

void NotificationManager::closePersistent(
    const blink::WebSerializedOrigin& origin,
    const blink::WebString& persistent_notification_id) {
  thread_safe_sender_->Send(new PlatformNotificationHostMsg_ClosePersistent(
      GURL(origin.string()),
      base::UTF16ToUTF8(persistent_notification_id)));
}

void NotificationManager::notifyDelegateDestroyed(
    blink::WebNotificationDelegate* delegate) {
  if (pending_notifications_.CancelPageNotificationFetches(delegate))
    return;

  for (auto& iter : active_page_notifications_) {
    if (iter.second != delegate)
      continue;

    active_page_notifications_.erase(iter.first);
    return;
  }
}

WebNotificationPermission NotificationManager::checkPermission(
    const blink::WebSerializedOrigin& origin) {
  WebNotificationPermission permission =
      blink::WebNotificationPermissionAllowed;
  thread_safe_sender_->Send(new PlatformNotificationHostMsg_CheckPermission(
      GURL(origin.string()), &permission));

  return permission;
}

bool NotificationManager::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(NotificationManager, message)
    IPC_MESSAGE_HANDLER(PlatformNotificationMsg_DidShow, OnDidShow);
    IPC_MESSAGE_HANDLER(PlatformNotificationMsg_DidClose, OnDidClose);
    IPC_MESSAGE_HANDLER(PlatformNotificationMsg_DidClick, OnDidClick);
    IPC_MESSAGE_HANDLER(PlatformNotificationMsg_DidGetNotifications,
                        OnDidGetNotifications)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return handled;
}

void NotificationManager::OnDidShow(int notification_id) {
  const auto& iter = active_page_notifications_.find(notification_id);
  if (iter == active_page_notifications_.end())
    return;

  iter->second->dispatchShowEvent();
}

void NotificationManager::OnDidClose(int notification_id) {
  const auto& iter = active_page_notifications_.find(notification_id);
  if (iter == active_page_notifications_.end())
    return;

  iter->second->dispatchCloseEvent();
  active_page_notifications_.erase(iter);
}

void NotificationManager::OnDidClick(int notification_id) {
  const auto& iter = active_page_notifications_.find(notification_id);
  if (iter == active_page_notifications_.end())
    return;

  iter->second->dispatchClickEvent();
}

void NotificationManager::OnDidGetNotifications(
    int request_id,
    const std::vector<PersistentNotificationInfo>& notification_infos) {
  blink::WebNotificationGetCallbacks* callbacks =
      pending_get_notification_requests_.Lookup(request_id);
  DCHECK(callbacks);
  if (!callbacks)
    return;

  scoped_ptr<blink::WebVector<blink::WebPersistentNotificationInfo>>
      notifications(new blink::WebVector<blink::WebPersistentNotificationInfo>(
          notification_infos.size()));

  for (size_t i = 0; i < notification_infos.size(); ++i) {
    blink::WebPersistentNotificationInfo web_notification_info;
    web_notification_info.persistentNotificationId =
        blink::WebString::fromUTF8(notification_infos[i].first);
    web_notification_info.data =
        ToWebNotificationData(notification_infos[i].second);

    (*notifications)[i] = web_notification_info;
  }

  callbacks->onSuccess(notifications.release());

  pending_get_notification_requests_.Remove(request_id);
}

void NotificationManager::DisplayPageNotification(
    const blink::WebSerializedOrigin& origin,
    const blink::WebNotificationData& notification_data,
    blink::WebNotificationDelegate* delegate,
    const SkBitmap& icon) {
  int notification_id =
      notification_dispatcher_->GenerateNotificationId(CurrentWorkerId());

  active_page_notifications_[notification_id] = delegate;
  thread_safe_sender_->Send(
      new PlatformNotificationHostMsg_Show(
          notification_id,
          GURL(origin.string()),
          icon,
          ToPlatformNotificationData(notification_data)));
}

void NotificationManager::DisplayPersistentNotification(
    const blink::WebSerializedOrigin& origin,
    const blink::WebNotificationData& notification_data,
    int64 service_worker_registration_id,
    scoped_ptr<blink::WebNotificationShowCallbacks> callbacks,
    const SkBitmap& icon) {
  thread_safe_sender_->Send(
      new PlatformNotificationHostMsg_ShowPersistent(
          service_worker_registration_id,
          GURL(origin.string()),
          icon,
          ToPlatformNotificationData(notification_data)));

  // There currently isn't a case in which the promise would be rejected per
  // our implementation, so always resolve it here.
  callbacks->onSuccess();
}

}  // namespace content
