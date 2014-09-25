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
  ActivityFactory* factory = ActivityFactory::Get();

  Activity* activity1 =
      factory->CreateWebActivity(NULL, base::string16(), GURL());
  EXPECT_EQ(1, activity_manager->num_activities());

  // Activity is not visible when created.
  EXPECT_FALSE(activity1->GetWindow()->TargetVisibility());
  Activity::Show(activity1);
  EXPECT_TRUE(activity1->GetWindow()->TargetVisibility());

  Activity* activity2 =
      factory->CreateWebActivity(NULL, base::string16(), GURL());
  EXPECT_EQ(2, activity_manager->num_activities());

  Activity::Delete(activity1);
  EXPECT_EQ(1, activity_manager->num_activities());

  // Deleting the activity's window should delete the activity itself.
  delete activity2->GetWindow();
  EXPECT_EQ(0, activity_manager->num_activities());
}

TEST_F(ActivityManagerTest, GetActivityForWindow) {
  ActivityManager* manager = ActivityManager::Get();
  ActivityFactory* factory = ActivityFactory::Get();

  Activity* activity1 =
      factory->CreateWebActivity(NULL, base::string16(), GURL());
  Activity* activity2 =
      factory->CreateWebActivity(NULL, base::string16(), GURL());

  EXPECT_EQ(activity1, manager->GetActivityForWindow(activity1->GetWindow()));
  EXPECT_EQ(activity2, manager->GetActivityForWindow(activity2->GetWindow()));

  EXPECT_EQ(NULL, manager->GetActivityForWindow(NULL));

  scoped_ptr<aura::Window> window = CreateTestWindow(NULL, gfx::Rect());
  EXPECT_EQ(NULL, manager->GetActivityForWindow(window.get()));
}

}  // namespace athena
