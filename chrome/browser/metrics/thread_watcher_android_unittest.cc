// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#include "base/android/application_status_listener.h"
#include "base/run_loop.h"
#include "base/synchronization/waitable_event.h"
#include "chrome/browser/metrics/thread_watcher.h"
#include "chrome/browser/metrics/thread_watcher_android.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::BrowserThread;

namespace {
void OnThreadWatcherTask(base::WaitableEvent* event) {
  event->Signal();
}

void PostAndWaitForWatchdogThread(base::WaitableEvent* event) {
  WatchDogThread::PostDelayedTask(
      FROM_HERE,
      base::Bind(&OnThreadWatcherTask, event),
      base::TimeDelta::FromSeconds(0));

  EXPECT_TRUE(event->TimedWait(base::TimeDelta::FromSeconds(1)));
}

void NotifyApplicationStateChange(base::android::ApplicationState state) {
  base::WaitableEvent watchdog_thread_event(false, false);

  base::android::ApplicationStatusListener::NotifyApplicationStateChange(state);
  base::RunLoop().RunUntilIdle();

  PostAndWaitForWatchdogThread(&watchdog_thread_event);
}

}  // namespace

TEST(ThreadWatcherAndroidTest, ApplicationStatusNotification) {
  // Do not delay the ThreadWatcherList initialization for this test.
  ThreadWatcherList::g_initialize_delay_seconds = 0;

  base::MessageLoopForUI message_loop_for_ui;
  content::TestBrowserThread ui_thread(BrowserThread::UI, &message_loop_for_ui);


  scoped_ptr<WatchDogThread> watchdog_thread_(new WatchDogThread());
  watchdog_thread_->Start();

  EXPECT_FALSE(ThreadWatcherList::g_thread_watcher_list_);


  // Register, and notify the application has just started,
  // and ensure the thread watcher list is created.
  ThreadWatcherAndroid::RegisterApplicationStatusListener();
  ThreadWatcherList::StartWatchingAll(*CommandLine::ForCurrentProcess());
  NotifyApplicationStateChange(
      base::android::APPLICATION_STATE_HAS_RUNNING_ACTIVITIES);
  EXPECT_TRUE(ThreadWatcherList::g_thread_watcher_list_);

  // Notify the application has been stopped, and ensure the thread watcher list
  // has been destroyed.
  NotifyApplicationStateChange(
      base::android::APPLICATION_STATE_HAS_STOPPED_ACTIVITIES);
  EXPECT_FALSE(ThreadWatcherList::g_thread_watcher_list_);

  // And again the last transition, STOPPED -> STARTED.
  NotifyApplicationStateChange(
      base::android::APPLICATION_STATE_HAS_RUNNING_ACTIVITIES);
  EXPECT_TRUE(ThreadWatcherList::g_thread_watcher_list_);
}
