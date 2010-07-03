// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/desktop_notification_service.h"

#include "base/task.h"
#include "base/waitable_event.h"
#include "chrome/browser/notifications/notifications_prefs_cache.h"
#include "chrome/browser/renderer_host/test/test_render_view_host.h"
#include "chrome/test/testing_profile.h"
#include "grit/generated_resources.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class WaitTask : public Task {
 public:
  WaitTask(base::WaitableEvent* event)
      : event_(event) {
  }
  virtual void Run() {
    event_->Wait();
  }

 private:
  base::WaitableEvent* event_;
};


class DesktopNotificationServiceTest : public RenderViewHostTestHarness {
 public:
  DesktopNotificationServiceTest()
      : event_(false, false) {
  }
  base::WaitableEvent event_;
};

TEST_F(DesktopNotificationServiceTest, DefaultContentSettingSentToCache) {
  // The current message loop was already initalized by the superclass.
  ChromeThread ui_thread(ChromeThread::UI, MessageLoop::current());

  // Create IO thread, start its message loop.
  ChromeThread io_thread(ChromeThread::IO);
  io_thread.Start();
  ChromeThread::PostTask(ChromeThread::IO, FROM_HERE, new WaitTask(&event_));

  // Creates the service, calls InitPrefs() on it which loads data from the
  // profile into the cache and then puts the cache in io thread mode.
  DesktopNotificationService* service =
      profile()->GetDesktopNotificationService();
  NotificationsPrefsCache* cache = service->prefs_cache();

  // The default pref registered in DesktopNotificationService is "ask",
  // and that's what sent to the cache.
  EXPECT_EQ(CONTENT_SETTING_ASK, cache->CachedDefaultContentSetting());

  // Change the default content setting. This will post a task on the IO thread
  // to update the cache.
  service->SetDefaultContentSetting(CONTENT_SETTING_BLOCK);

  // The updated pref shouldn't be sent to the cache immediately.
  EXPECT_EQ(CONTENT_SETTING_ASK, cache->CachedDefaultContentSetting());

  // Run IO thread tasks.
  event_.Signal();
  io_thread.Stop();

  // Now that IO thread events have been processed, it should be there.
  EXPECT_EQ(CONTENT_SETTING_BLOCK, cache->CachedDefaultContentSetting());
}

}  // namespace
