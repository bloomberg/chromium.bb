// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/test/chrome/athena_browsertest.h"

#include "athena/activity/public/activity.h"
#include "athena/activity/public/activity_factory.h"
#include "athena/resource_manager/public/resource_manager.h"
#include "base/command_line.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/notification_types.h"
#include "ui/compositor/compositor_switches.h"

namespace athena {

AthenaBrowserTest::AthenaBrowserTest() {
}

AthenaBrowserTest::~AthenaBrowserTest() {
}

void AthenaBrowserTest::SetUp() {
  CommandLine* command_line = CommandLine::ForCurrentProcess();

  // Make sure that we draw the output.
  // TODO(skuhne): Consider having tests setting this flag individually instead.
  command_line->AppendSwitch(switches::kEnablePixelOutputInTests);

  InProcessBrowserTest::SetUp();
}

void AthenaBrowserTest::SendMemoryPressureEvent(
    ResourceManager::MemoryPressure pressure) {
  CHECK(ResourceManager::Get());
  ResourceManager::Get()->SetMemoryPressureAndStopMonitoring(pressure);
  WaitUntilIdle();
}

Activity* AthenaBrowserTest::CreateWebActivity(
    const base::string16& title,
    const GURL& url) {
  content::WindowedNotificationObserver observer(
      content::NOTIFICATION_LOAD_STOP,
      content::NotificationService::AllSources());
  content::BrowserContext* bc = ProfileManager::GetActiveUserProfile();
  Activity* activity =
      ActivityFactory::Get()->CreateWebActivity(bc, title, url);
  Activity::Show(activity);
  observer.Wait();
  return activity;
}

Activity* AthenaBrowserTest::CreateAppActivity(
    const base::string16& title,
    const std::string app_id) {
  // TODO(skuhne): To define next.
  return NULL;
}

void AthenaBrowserTest::WaitUntilIdle() {
  base::MessageLoopForUI::current()->RunUntilIdle();
}

void AthenaBrowserTest::SetUpOnMainThread() {
  // Set the memory pressure to low and turning off undeterministic resource
  // observer events.
  SendMemoryPressureEvent(ResourceManager::MEMORY_PRESSURE_LOW);
}

}  // namespace athena
