// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_PROFILE_SYNC_TEST_UTIL_H_
#define CHROME_BROWSER_SYNC_PROFILE_SYNC_TEST_UTIL_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "base/message_loop.h"
#include "base/synchronization/waitable_event.h"
#include "chrome/browser/sync/profile_sync_service_observer.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace base {
class Thread;
}

ACTION_P(Notify, type) {
  content::NotificationService::current()->Notify(
      type,
      content::NotificationService::AllSources(),
      content::NotificationService::NoDetails());
}

ACTION(QuitUIMessageLoop) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  MessageLoop::current()->Quit();
}

class ProfileSyncServiceObserverMock : public ProfileSyncServiceObserver {
 public:
  ProfileSyncServiceObserverMock();
  virtual ~ProfileSyncServiceObserverMock();

  MOCK_METHOD0(OnStateChanged, void());
};

class ThreadNotifier :  // NOLINT
    public base::RefCountedThreadSafe<ThreadNotifier> {
 public:
  explicit ThreadNotifier(base::Thread* notify_thread);

  void Notify(int type, const content::NotificationDetails& details);

  void Notify(int type,
              const content::NotificationSource& source,
              const content::NotificationDetails& details);

 private:
  friend class base::RefCountedThreadSafe<ThreadNotifier>;
  virtual ~ThreadNotifier();

  void NotifyTask(int type,
                  const content::NotificationSource& source,
                  const content::NotificationDetails& details);

  base::WaitableEvent done_event_;
  base::Thread* notify_thread_;
};

#endif  // CHROME_BROWSER_SYNC_PROFILE_SYNC_TEST_UTIL_H_
