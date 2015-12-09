// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_PROFILE_SYNC_TEST_UTIL_H_
#define CHROME_BROWSER_SYNC_PROFILE_SYNC_TEST_UTIL_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/synchronization/waitable_event.h"
#include "chrome/browser/sync/profile_sync_service_mock.h"
#include "components/sync_driver/sync_service_observer.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace base {
class Thread;
class Time;
class TimeDelta;
}

namespace content {
class BrowserContext;
}

namespace sync_driver {
class SyncClient;
}

class KeyedService;
class Profile;
class ProfileSyncServiceMock;
class TestingProfile;

// An empty syncer::NetworkTimeUpdateCallback. Used in various tests to
// instantiate ProfileSyncService.
void EmptyNetworkTimeUpdate(const base::Time&,
                            const base::TimeDelta&,
                            const base::TimeDelta&);

ACTION_P(Notify, type) {
  content::NotificationService::current()->Notify(
      type,
      content::NotificationService::AllSources(),
      content::NotificationService::NoDetails());
}

ACTION(QuitUIMessageLoop) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  base::MessageLoop::current()->QuitWhenIdle();
}

class SyncServiceObserverMock : public sync_driver::SyncServiceObserver {
 public:
  SyncServiceObserverMock();
  virtual ~SyncServiceObserverMock();

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

// Helper methods for constructing ProfileSyncService mocks.
ProfileSyncService::InitParams CreateProfileSyncServiceParamsForTest(
    Profile* profile);
ProfileSyncService::InitParams CreateProfileSyncServiceParamsForTest(
    scoped_ptr<sync_driver::SyncClient> sync_client,
    Profile* profile);

// A utility used by sync tests to create a TestingProfile with a Google
// Services username stored in a (Testing)PrefService.
scoped_ptr<TestingProfile> MakeSignedInTestingProfile();

// Helper routine to be used in conjunction with
// BrowserContextKeyedServiceFactory::SetTestingFactory().
scoped_ptr<KeyedService> BuildMockProfileSyncService(
    content::BrowserContext* context);

#endif  // CHROME_BROWSER_SYNC_PROFILE_SYNC_TEST_UTIL_H_
