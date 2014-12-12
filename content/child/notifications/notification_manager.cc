// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/notifications/notification_manager.h"

#include "base/lazy_instance.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_local.h"
#include "content/child/notifications/notification_data_conversions.h"
#include "content/child/notifications/notification_dispatcher.h"
#include "content/child/notifications/notification_image_loader.h"
#include "content/child/service_worker/web_service_worker_registration_impl.h"
#include "content/child/thread_safe_sender.h"
#include "content/child/worker_task_runner.h"
#include "content/common/platform_notification_messages.h"
#include "content/public/common/platform_notification_data.h"
#include "third_party/WebKit/public/platform/WebNotificationData.h"
#include "third_party/WebKit/public/platform/WebNotificationDelegate.h"
#include "third_party/WebKit/public/platform/WebSerializedOrigin.h"
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
      main_thread_task_runner_(main_thread_task_runner),
      notification_dispatcher_(notification_dispatcher),
      weak_factory_(this) {
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
    DisplayNotification(origin, notification_data, delegate,
                        nullptr /* image_loader */);
    return;
  }

  pending_page_notifications_[delegate] = CreateImageLoader(
      notification_data.icon,
      base::Bind(&NotificationManager::DisplayNotification,
                 weak_factory_.GetWeakPtr(),
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

  int request_id = persistent_notification_requests_.Add(callbacks);
  if (notification_data.icon.isEmpty()) {
    DisplayPersistentNotification(origin,
                                  notification_data,
                                  service_worker_registration_id,
                                  request_id,
                                  nullptr /* image_loader */);
    return;
  }

  pending_persistent_notifications_.insert(CreateImageLoader(
      notification_data.icon,
      base::Bind(&NotificationManager::DisplayPersistentNotification,
                 weak_factory_.GetWeakPtr(),
                 origin,
                 notification_data,
                 service_worker_registration_id,
                 request_id)));
}

void NotificationManager::close(blink::WebNotificationDelegate* delegate) {
  if (RemovePendingPageNotification(delegate))
    return;

  auto iter = active_notifications_.begin();
  for (; iter != active_notifications_.end(); ++iter) {
    if (iter->second != delegate)
      continue;

    thread_safe_sender_->Send(
        new PlatformNotificationHostMsg_Close(iter->first));
    active_notifications_.erase(iter);

    delegate->dispatchCloseEvent();
    return;
  }

  // It should not be possible for Blink to call close() on a Notification which
  // does not exist anymore in the manager.
  NOTREACHED();
}

void NotificationManager::closePersistent(
    const blink::WebString& persistent_notification_id) {
  thread_safe_sender_->Send(new PlatformNotificationHostMsg_ClosePersistent(
      base::UTF16ToUTF8(persistent_notification_id)));
}

void NotificationManager::notifyDelegateDestroyed(
    blink::WebNotificationDelegate* delegate) {
  if (RemovePendingPageNotification(delegate))
    return;

  auto iter = active_notifications_.begin();
  for (; iter != active_notifications_.end(); ++iter) {
    if (iter->second != delegate)
      continue;

    active_notifications_.erase(iter);
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
    IPC_MESSAGE_HANDLER(PlatformNotificationMsg_DidShowPersistent,
                        OnDidShowPersistent)
    IPC_MESSAGE_HANDLER(PlatformNotificationMsg_DidShow, OnDidShow);
    IPC_MESSAGE_HANDLER(PlatformNotificationMsg_DidClose, OnDidClose);
    IPC_MESSAGE_HANDLER(PlatformNotificationMsg_DidClick, OnDidClick);
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return handled;
}

void NotificationManager::OnDidShowPersistent(int request_id) {
  blink::WebNotificationShowCallbacks* callbacks =
      persistent_notification_requests_.Lookup(request_id);
  DCHECK(callbacks);

  // There currently isn't a case in which the promise would be rejected per
  // our implementation, so always resolve it here.
  callbacks->onSuccess();

  persistent_notification_requests_.Remove(request_id);
}

void NotificationManager::OnDidShow(int notification_id) {
  const auto& iter = active_notifications_.find(notification_id);
  if (iter == active_notifications_.end())
    return;

  iter->second->dispatchShowEvent();
}

void NotificationManager::OnDidClose(int notification_id) {
  const auto& iter = active_notifications_.find(notification_id);
  if (iter == active_notifications_.end())
    return;

  iter->second->dispatchCloseEvent();
  active_notifications_.erase(iter);
}

void NotificationManager::OnDidClick(int notification_id) {
  const auto& iter = active_notifications_.find(notification_id);
  if (iter == active_notifications_.end())
    return;

  iter->second->dispatchClickEvent();
}

scoped_refptr<NotificationImageLoader> NotificationManager::CreateImageLoader(
    const blink::WebURL& image_url,
    const NotificationImageLoadedCallback& callback) const {
  scoped_refptr<NotificationImageLoader> pending_notification(
      new NotificationImageLoader(callback));

  main_thread_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&NotificationImageLoader::StartOnMainThread,
                 pending_notification,
                 image_url,
                 CurrentWorkerId()));

  return pending_notification;
}

void NotificationManager::DisplayNotification(
    const blink::WebSerializedOrigin& origin,
    const blink::WebNotificationData& notification_data,
    blink::WebNotificationDelegate* delegate,
    scoped_refptr<NotificationImageLoader> image_loader) {
  int notification_id =
      notification_dispatcher_->GenerateNotificationId(CurrentWorkerId());

  active_notifications_[notification_id] = delegate;

  SkBitmap icon;
  if (image_loader)
    icon = image_loader->GetDecodedImage();

  thread_safe_sender_->Send(
      new PlatformNotificationHostMsg_Show(
          notification_id,
          GURL(origin.string()),
          icon,
          ToPlatformNotificationData(notification_data)));

  // If this Notification contained an icon, it can be safely deleted now.
  RemovePendingPageNotification(delegate);
}

void NotificationManager::DisplayPersistentNotification(
    const blink::WebSerializedOrigin& origin,
    const blink::WebNotificationData& notification_data,
    int64 service_worker_registration_id,
    int request_id,
    scoped_refptr<NotificationImageLoader> image_loader) {
  SkBitmap icon;
  if (image_loader) {
    pending_persistent_notifications_.erase(image_loader);
    icon = image_loader->GetDecodedImage();
  }

  thread_safe_sender_->Send(
      new PlatformNotificationHostMsg_ShowPersistent(
          request_id,
          service_worker_registration_id,
          GURL(origin.string()),
          icon,
          ToPlatformNotificationData(notification_data)));
}

bool NotificationManager::RemovePendingPageNotification(
    blink::WebNotificationDelegate* delegate) {
  const auto& iter = pending_page_notifications_.find(delegate);
  if (iter == pending_page_notifications_.end())
    return false;

  pending_page_notifications_.erase(iter);
  return true;
}

}  // namespace content
