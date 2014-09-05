/// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/activity/activity_manager_impl.h"

#include "athena/activity/public/activity.h"
#include "athena/activity/public/activity_factory.h"
#include "athena/test/athena_test_base.h"
#include "ui/aura/window.h"

namespace athena {

typedef test::AthenaTestBase ActivityManagerTest;

TEST_F(ActivityManagerTest, Basic) {
  ActivityManagerImpl* activity_manager =
      static_cast<ActivityManagerImpl*>(ActivityManager::Get());
  Activity* activity1 = athena::ActivityFactory::Get()->CreateWebActivity(
      NULL, GURL());
  activity_manager->AddActivity(activity1);
  EXPECT_EQ(1, activity_manager->num_activities());

  Activity* activity2 =
      athena::ActivityFactory::Get()->CreateWebActivity(NULL, GURL());
  activity_manager->AddActivity(activity2);
  EXPECT_EQ(2, activity_manager->num_activities());

  Activity::Delete(activity1);
  EXPECT_EQ(1, activity_manager->num_activities());

  // Deleting the activity's window should delete the activity itself.
  delete activity2->GetWindow();
  EXPECT_EQ(0, activity_manager->num_activities());
}

}  // namespace athena
