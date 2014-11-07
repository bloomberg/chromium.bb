// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/test/base/test_util.h"

#include "athena/activity/public/activity.h"
#include "athena/activity/public/activity_factory.h"
#include "athena/resource_manager/public/resource_manager.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/notification_types.h"

namespace athena {
namespace test_util {

void SendTestMemoryPressureEvent(ResourceManager::MemoryPressure pressure) {
  CHECK(ResourceManager::Get());
  ResourceManager::Get()->SetMemoryPressureAndStopMonitoring(pressure);
  WaitUntilIdle();
}

Activity* CreateTestWebActivity(content::BrowserContext* context,
                                const base::string16& title,
                                const GURL& url) {
  content::WindowedNotificationObserver observer(
      content::NOTIFICATION_LOAD_STOP,
      content::NotificationService::AllSources());
  Activity* activity =
      ActivityFactory::Get()->CreateWebActivity(context, title, url);
  Activity::Show(activity);
  observer.Wait();
  return activity;
}

void WaitUntilIdle() {
  base::MessageLoopForUI::current()->RunUntilIdle();
}

}  // namespace test_util
}  // namespace athena
