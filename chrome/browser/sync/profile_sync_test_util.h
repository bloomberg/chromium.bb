// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_PROFILE_SYNC_TEST_UTIL_H_
#define CHROME_BROWSER_SYNC_PROFILE_SYNC_TEST_UTIL_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "components/browser_sync/browser/profile_sync_service_mock.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "testing/gmock/include/gmock/gmock.h"

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
