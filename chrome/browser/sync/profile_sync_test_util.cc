// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/profile_sync_test_util.h"

#include "base/task.h"
#include "base/threading/thread.h"

ProfileSyncServiceObserverMock::ProfileSyncServiceObserverMock() {}

ProfileSyncServiceObserverMock::~ProfileSyncServiceObserverMock() {}

ThreadNotificationService::ThreadNotificationService(
    base::Thread* notification_thread)
    : done_event_(false, false),
      notification_thread_(notification_thread) {}

void ThreadNotificationService::Init() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  notification_thread_->message_loop()->PostTask(
      FROM_HERE,
      NewRunnableMethod(this, &ThreadNotificationService::InitTask));
  done_event_.Wait();
}

void ThreadNotificationService::TearDown() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  notification_thread_->message_loop()->PostTask(
      FROM_HERE,
      NewRunnableMethod(this,
                        &ThreadNotificationService::TearDownTask));
  done_event_.Wait();
}

ThreadNotificationService::~ThreadNotificationService() {}

void ThreadNotificationService::InitTask() {
  service_.reset(new NotificationService());
  done_event_.Signal();
}

void ThreadNotificationService::TearDownTask() {
  service_.reset(NULL);
  done_event_.Signal();
}

ThreadNotifier::ThreadNotifier(base::Thread* notify_thread)
    : done_event_(false, false),
      notify_thread_(notify_thread) {}

void ThreadNotifier::Notify(NotificationType type,
                            const NotificationDetails& details) {
  Notify(type, NotificationService::AllSources(), details);
}

void ThreadNotifier::Notify(NotificationType type,
                            const NotificationSource& source,
                            const NotificationDetails& details) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  notify_thread_->message_loop()->PostTask(
      FROM_HERE,
      NewRunnableMethod(this,
                        &ThreadNotifier::NotifyTask,
                        type,
                        source,
                        details));
  done_event_.Wait();
}

ThreadNotifier::~ThreadNotifier() {}

void ThreadNotifier::NotifyTask(NotificationType type,
                                const NotificationSource& source,
                                const NotificationDetails& details) {
  NotificationService::current()->Notify(type, source, details);
  done_event_.Signal();
}
