// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_PROFILE_SYNC_TEST_UTIL_H_
#define CHROME_BROWSER_SYNC_PROFILE_SYNC_TEST_UTIL_H_
#pragma once

#include <string>

#include "base/message_loop.h"
#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "base/synchronization/waitable_event.h"
#include "chrome/browser/browser_thread.h"
#include "chrome/browser/sync/profile_sync_service_observer.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/notification_source.h"
#include "chrome/common/notification_type.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace base {
class Thread;
}

ACTION_P(Notify, type) {
  NotificationService::current()->Notify(type,
                                         NotificationService::AllSources(),
                                         NotificationService::NoDetails());
}

ACTION(QuitUIMessageLoop) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  MessageLoop::current()->Quit();
}

class ProfileSyncServiceObserverMock : public ProfileSyncServiceObserver {
 public:
  ProfileSyncServiceObserverMock();
  virtual ~ProfileSyncServiceObserverMock();

  MOCK_METHOD0(OnStateChanged, void());
};

class ThreadNotificationService
    : public base::RefCountedThreadSafe<ThreadNotificationService> {
 public:
  explicit ThreadNotificationService(base::Thread* notification_thread);

  void Init();
  void TearDown();

 private:
  friend class base::RefCountedThreadSafe<ThreadNotificationService>;
  virtual ~ThreadNotificationService();

  void InitTask();
  void TearDownTask();

  base::WaitableEvent done_event_;
  base::Thread* notification_thread_;
  scoped_ptr<NotificationService> service_;
};

class ThreadNotifier :  // NOLINT
    public base::RefCountedThreadSafe<ThreadNotifier> {
 public:
  explicit ThreadNotifier(base::Thread* notify_thread);

  void Notify(NotificationType type, const NotificationDetails& details);

  void Notify(NotificationType type,
              const NotificationSource& source,
              const NotificationDetails& details);

 private:
  friend class base::RefCountedThreadSafe<ThreadNotifier>;
  virtual ~ThreadNotifier();

  void NotifyTask(NotificationType type,
                  const NotificationSource& source,
                  const NotificationDetails& details);

  base::WaitableEvent done_event_;
  base::Thread* notify_thread_;
};

#endif  // CHROME_BROWSER_SYNC_PROFILE_SYNC_TEST_UTIL_H_
