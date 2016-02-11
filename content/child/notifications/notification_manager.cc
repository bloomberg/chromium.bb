// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/notifications/notification_manager.h"

#include <cmath>
#include <utility>

#include "base/lazy_instance.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/utf_string_conversions.h"
#include "base/thread_task_runner_handle.h"
#include "base/threading/thread_local.h"
#include "content/child/notifications/notification_data_conversions.h"
#include "content/child/notifications/notification_dispatcher.h"
#include "content/child/service_worker/web_service_worker_registration_impl.h"
#include "content/child/thread_safe_sender.h"
#include "content/common/notification_constants.h"
#include "content/public/common/notification_resources.h"
#include "content/public/common/platform_notification_data.h"
#include "third_party/WebKit/public/platform/URLConversion.h"
#include "third_party/WebKit/public/platform/WebSecurityOrigin.h"
#include "third_party/WebKit/public/platform/modules/notifications/WebNotificationDelegate.h"

using blink::WebNotificationPermission;

namespace content {
namespace {

int CurrentWorkerId() {
  return WorkerThread::GetCurrentId();
}

// Checks whether |notification_data| specifies any non-empty resources that
// need to be fetched.
bool HasResourcesToFetch(const blink::WebNotificationData& notification_data) {
  if (!notification_data.icon.isEmpty())
    return true;
  for (const auto& action : notification_data.actions) {
    if (!action.icon.isEmpty())
      return true;
  }
  return false;
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
      notifications_tracker_(main_thread_task_runner) {
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
    WorkerThread::AddObserver(manager);
  return manager;
}

void NotificationManager::WillStopCurrentWorkerThread() {
  delete this;
}

void NotificationManager::show(
    const blink::WebSecurityOrigin& origin,
    const blink::WebNotificationData& notification_data,
    blink::WebNotificationDelegate* delegate) {
  DCHECK_EQ(0u, notification_data.actions.size());

  if (!HasResourcesToFetch(notification_data)) {
    DisplayPageNotification(origin, notification_data, delegate,
                            NotificationResources());
    return;
  }

  notifications_tracker_.FetchResources(
      notification_data, delegate,
      base::Bind(&NotificationManager::DisplayPageNotification,
                 base::Unretained(this),  // this owns |notifications_tracker_|
                 origin, notification_data, delegate));
}

void NotificationManager::showPersistent(
    const blink::WebSecurityOrigin& origin,
    const blink::WebNotificationData& notification_data,
    blink::WebServiceWorkerRegistration* service_worker_registration,
    blink::WebNotificationShowCallbacks* callbacks) {
  DCHECK(service_worker_registration);

  int64_t service_worker_registration_id =
      static_cast<WebServiceWorkerRegistrationImpl*>(
          service_worker_registration)
          ->registration_id();

  scoped_ptr<blink::WebNotificationShowCallbacks> owned_callbacks(callbacks);

  // Verify that the author-provided payload size does not exceed our limit.
  // This is an implementation-defined limit to prevent abuse of notification
  // data as a storage mechanism. A UMA histogram records the requested sizes,
  // which enables us to track how much data authors are attempting to store.
  //
  // If the size exceeds this limit, reject the showNotification() promise. This
  // is outside of the boundaries set by the specification, but it gives authors
  // an indication that something has gone wrong.
  size_t author_data_size = notification_data.data.size();
  UMA_HISTOGRAM_MEMORY_KB("Notifications.AuthorDataSizeKB",
                          static_cast<int>(ceil(author_data_size / 1024.0)));

  if (author_data_size > PlatformNotificationData::kMaximumDeveloperDataSize) {
    owned_callbacks->onError();
    return;
  }

  if (!HasResourcesToFetch(notification_data)) {
    NotificationResources notification_resources;

    // Action indices are expected to have a corresponding icon bitmap, which
    // may be empty when the developer provided no (or an invalid) icon.
    if (!notification_data.actions.isEmpty()) {
      notification_resources.action_icons.resize(
          notification_data.actions.size());
    }

    DisplayPersistentNotification(
        origin, notification_data, service_worker_registration_id,
        std::move(owned_callbacks), notification_resources);
    return;
  }

  notifications_tracker_.FetchResources(
      notification_data, nullptr /* delegate */,
      base::Bind(&NotificationManager::DisplayPersistentNotification,
                 base::Unretained(this),  // this owns |notifications_tracker_|
                 origin, notification_data, service_worker_registration_id,
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
  int64_t service_worker_registration_id =
      service_worker_registration_impl->registration_id();

  // TODO(peter): GenerateNotificationId is more of a request id. Consider
  // renaming the method in the NotificationDispatcher if this makes sense.
  int request_id =
      notification_dispatcher_->GenerateNotificationId(CurrentWorkerId());

  pending_get_notification_requests_.AddWithID(callbacks, request_id);

  thread_safe_sender_->Send(new PlatformNotificationHostMsg_GetNotifications(
      request_id, service_worker_registration_id, origin,
      base::UTF16ToUTF8(base::StringPiece16(filter_tag))));
}

void NotificationManager::close(blink::WebNotificationDelegate* delegate) {
  if (notifications_tracker_.CancelResourceFetches(delegate))
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
    const blink::WebSecurityOrigin& origin,
    int64_t persistent_notification_id) {
  thread_safe_sender_->Send(new PlatformNotificationHostMsg_ClosePersistent(
      // TODO(mkwst): This is potentially doing the wrong thing with unique
      // origins. Perhaps also 'file:', 'blob:' and 'filesystem:'. See
      // https://crbug.com/490074 for detail.
      blink::WebStringToGURL(origin.toString()), persistent_notification_id));
}

void NotificationManager::notifyDelegateDestroyed(
    blink::WebNotificationDelegate* delegate) {
  if (notifications_tracker_.CancelResourceFetches(delegate))
    return;

  for (auto& iter : active_page_notifications_) {
    if (iter.second != delegate)
      continue;

    active_page_notifications_.erase(iter.first);
    return;
  }
}

WebNotificationPermission NotificationManager::checkPermission(
    const blink::WebSecurityOrigin& origin) {
  WebNotificationPermission permission =
      blink::WebNotificationPermissionAllowed;
  // TODO(mkwst): This is potentially doing the wrong thing with unique
  // origins. Perhaps also 'file:', 'blob:' and 'filesystem:'. See
  // https://crbug.com/490074 for detail.
  thread_safe_sender_->Send(new PlatformNotificationHostMsg_CheckPermission(
      blink::WebStringToGURL(origin.toString()), &permission));

  return permission;
}

size_t NotificationManager::maxActions() {
  return kPlatformNotificationMaxActions;
}

bool NotificationManager::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(NotificationManager, message)
    IPC_MESSAGE_HANDLER(PlatformNotificationMsg_DidShow, OnDidShow);
    IPC_MESSAGE_HANDLER(PlatformNotificationMsg_DidShowPersistent,
                        OnDidShowPersistent)
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

void NotificationManager::OnDidShowPersistent(int request_id, bool success) {
  blink::WebNotificationShowCallbacks* callbacks =
      pending_show_notification_requests_.Lookup(request_id);
  DCHECK(callbacks);

  if (!callbacks)
    return;

  if (success)
    callbacks->onSuccess();
  else
    callbacks->onError();

  pending_show_notification_requests_.Remove(request_id);
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

  blink::WebVector<blink::WebPersistentNotificationInfo> notifications(
      notification_infos.size());

  for (size_t i = 0; i < notification_infos.size(); ++i) {
    blink::WebPersistentNotificationInfo web_notification_info;
    web_notification_info.persistentId = notification_infos[i].first;
    web_notification_info.data =
        ToWebNotificationData(notification_infos[i].second);

    notifications[i] = web_notification_info;
  }

  callbacks->onSuccess(notifications);

  pending_get_notification_requests_.Remove(request_id);
}

void NotificationManager::DisplayPageNotification(
    const blink::WebSecurityOrigin& origin,
    const blink::WebNotificationData& notification_data,
    blink::WebNotificationDelegate* delegate,
    const NotificationResources& notification_resources) {
  DCHECK_EQ(0u, notification_data.actions.size());
  DCHECK_EQ(0u, notification_resources.action_icons.size());

  int notification_id =
      notification_dispatcher_->GenerateNotificationId(CurrentWorkerId());

  active_page_notifications_[notification_id] = delegate;
  // TODO(mkwst): This is potentially doing the wrong thing with unique
  // origins. Perhaps also 'file:', 'blob:' and 'filesystem:'. See
  // https://crbug.com/490074 for detail.
  thread_safe_sender_->Send(new PlatformNotificationHostMsg_Show(
      notification_id, blink::WebStringToGURL(origin.toString()),
      ToPlatformNotificationData(notification_data), notification_resources));
}

void NotificationManager::DisplayPersistentNotification(
    const blink::WebSecurityOrigin& origin,
    const blink::WebNotificationData& notification_data,
    int64_t service_worker_registration_id,
    scoped_ptr<blink::WebNotificationShowCallbacks> callbacks,
    const NotificationResources& notification_resources) {
  DCHECK_EQ(notification_data.actions.size(),
            notification_resources.action_icons.size());

  // TODO(peter): GenerateNotificationId is more of a request id. Consider
  // renaming the method in the NotificationDispatcher if this makes sense.
  int request_id =
      notification_dispatcher_->GenerateNotificationId(CurrentWorkerId());

  pending_show_notification_requests_.AddWithID(callbacks.release(),
                                                request_id);

  // TODO(mkwst): This is potentially doing the wrong thing with unique
  // origins. Perhaps also 'file:', 'blob:' and 'filesystem:'. See
  // https://crbug.com/490074 for detail.
  thread_safe_sender_->Send(new PlatformNotificationHostMsg_ShowPersistent(
      request_id, service_worker_registration_id,
      blink::WebStringToGURL(origin.toString()),
      ToPlatformNotificationData(notification_data), notification_resources));
}

}  // namespace content
