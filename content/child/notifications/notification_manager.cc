// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/notifications/notification_manager.h"

#include "base/lazy_instance.h"
#include "base/threading/thread_local.h"
#include "content/child/notifications/notification_dispatcher.h"
#include "content/child/notifications/notification_image_loader.h"
#include "content/child/thread_safe_sender.h"
#include "content/child/worker_task_runner.h"
#include "content/common/platform_notification_messages.h"
#include "content/public/common/show_desktop_notification_params.h"
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
    DisplayNotification(origin, notification_data, delegate, SkBitmap());
    return;
  }

  scoped_refptr<NotificationImageLoader> pending_notification(
      new NotificationImageLoader(
          delegate,
          base::Bind(&NotificationManager::DisplayNotification,
                     weak_factory_.GetWeakPtr(),
                     origin,
                     notification_data)));

  // Image downloads have to be started on the main thread, but it's possible
  // for a race to occur where a worker terminates whilst the image download
  // has not been started yet. When this happens, the refcounting semantics
  // will ensure that the loader gets cancelled immediately after starting.
  main_thread_task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::Bind(&NotificationImageLoader::StartOnMainThread,
                 pending_notification,
                 notification_data.icon,
                 CurrentWorkerId()),
      base::Bind(&NotificationManager::DidStartImageLoad,
                 weak_factory_.GetWeakPtr(),
                 pending_notification));
}

void NotificationManager::close(blink::WebNotificationDelegate* delegate) {
  if (RemovePendingNotification(delegate))
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

void NotificationManager::notifyDelegateDestroyed(
    blink::WebNotificationDelegate* delegate) {
  if (RemovePendingNotification(delegate))
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
    IPC_MESSAGE_HANDLER(PlatformNotificationMsg_DidShow, OnShow);
    IPC_MESSAGE_HANDLER(PlatformNotificationMsg_DidClose, OnClose);
    IPC_MESSAGE_HANDLER(PlatformNotificationMsg_DidClick, OnClick);
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return handled;
}

void NotificationManager::OnShow(int id) {
  const auto& iter = active_notifications_.find(id);
  if (iter == active_notifications_.end())
    return;

  iter->second->dispatchShowEvent();
}

void NotificationManager::OnClose(int id) {
  const auto& iter = active_notifications_.find(id);
  if (iter == active_notifications_.end())
    return;

  iter->second->dispatchCloseEvent();
  active_notifications_.erase(iter);
}

void NotificationManager::OnClick(int id) {
  const auto& iter = active_notifications_.find(id);
  if (iter == active_notifications_.end())
    return;

  iter->second->dispatchClickEvent();
}

void NotificationManager::DidStartImageLoad(
    const scoped_refptr<NotificationImageLoader>& pending_notification) {
  pending_notifications_.insert(pending_notification);
}

void NotificationManager::DisplayNotification(
    const blink::WebSerializedOrigin& origin,
    const blink::WebNotificationData& notification_data,
    blink::WebNotificationDelegate* delegate,
    const SkBitmap& icon) {
  int notification_id =
      notification_dispatcher_->GenerateNotificationId(CurrentWorkerId());

  active_notifications_[notification_id] = delegate;

  ShowDesktopNotificationHostMsgParams params;
  params.origin = GURL(origin.string());
  params.icon = icon;
  params.title = notification_data.title;
  params.body = notification_data.body;

  // TODO(peter): Remove the usage of the Blink WebTextDirection enumeration for
  // the text direction of notifications throughout Chrome.
  params.direction = blink::WebTextDirectionLeftToRight;
  params.replace_id = notification_data.tag;

  thread_safe_sender_->Send(new PlatformNotificationHostMsg_Show(
      notification_id, params));

  // If this Notification contained an icon, it can be safely deleted now.
  RemovePendingNotification(delegate);
}

bool NotificationManager::RemovePendingNotification(
    blink::WebNotificationDelegate* delegate) {
  for (auto& pending_notification : pending_notifications_) {
    if (pending_notification->delegate() == delegate) {
      pending_notifications_.erase(pending_notification);
      return true;
    }
  }

  return false;
}

}  // namespace content
