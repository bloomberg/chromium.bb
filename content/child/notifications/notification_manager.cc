// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/notifications/notification_manager.h"

#include "base/lazy_instance.h"
#include "base/threading/thread_local.h"
#include "content/child/notifications/notification_dispatcher.h"
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
    NotificationDispatcher* notification_dispatcher)
    : thread_safe_sender_(thread_safe_sender),
      notification_dispatcher_(notification_dispatcher) {
  g_notification_manager_tls.Pointer()->Set(this);
}

NotificationManager::~NotificationManager() {
  g_notification_manager_tls.Pointer()->Set(nullptr);
}

NotificationManager* NotificationManager::ThreadSpecificInstance(
    ThreadSafeSender* thread_safe_sender,
    NotificationDispatcher* notification_dispatcher) {
  if (g_notification_manager_tls.Pointer()->Get())
    return g_notification_manager_tls.Pointer()->Get();

  NotificationManager* manager = new NotificationManager(
      thread_safe_sender, notification_dispatcher);
  if (WorkerTaskRunner::Instance()->CurrentWorkerId())
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
  int notification_id =
      notification_dispatcher_->GenerateNotificationId(CurrentWorkerId());

  active_notifications_[notification_id] = delegate;

  ShowDesktopNotificationHostMsgParams params;
  params.origin = GURL(origin.string());

  // TODO(peter): Move the notification_icon_loader to //content/child/ and use
  // it to download Notification icons here.
  params.icon = SkBitmap();
  params.title = notification_data.title;
  params.body = notification_data.body;

  // TODO(peter): Remove the usage of the Blink WebTextDirection enumeration for
  // the text direction of notifications throughout Chrome.
  params.direction = blink::WebTextDirectionLeftToRight;
  params.replace_id = notification_data.tag;

  thread_safe_sender_->Send(new PlatformNotificationHostMsg_Show(
      notification_id, params));
}

void NotificationManager::close(blink::WebNotificationDelegate* delegate) {
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

}  // namespace content
