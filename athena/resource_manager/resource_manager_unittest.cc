/// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/activity/public/activity.h"
#include "athena/activity/public/activity_manager.h"
#include "athena/activity/public/activity_view_model.h"
#include "athena/resource_manager/memory_pressure_notifier.h"
#include "athena/resource_manager/public/resource_manager.h"
#include "athena/test/athena_test_base.h"
#include "athena/test/sample_activity.h"
#include "athena/wm/public/window_manager.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace athena {
namespace test {

namespace {

// A dummy test app activity which works without content / ShellAppWindow.
class TestActivity : public SampleActivity {
 public:
  explicit TestActivity(const std::string& title)
      : SampleActivity(0, 0, base::UTF8ToUTF16(title)),
        media_state_(ACTIVITY_MEDIA_STATE_NONE),
        is_visible_(false) {}
  virtual ~TestActivity() {}

  void set_media_state(ActivityMediaState media_state) {
    media_state_ = media_state;
  }
  void set_visible(bool visible) { is_visible_ = visible; }

  // Activity overrides:
  virtual bool IsVisible() OVERRIDE { return is_visible_; }
  virtual ActivityMediaState GetMediaState() OVERRIDE { return media_state_; }

 private:
  // The current media state.
  ActivityMediaState media_state_;

  // Returns if it is visible or not.
  bool is_visible_;

  DISALLOW_COPY_AND_ASSIGN(TestActivity);
};

}  // namespace

// Our testing base.
class ResourceManagerTest : public AthenaTestBase {
 public:
  ResourceManagerTest() {}
  virtual ~ResourceManagerTest() {}

  virtual void SetUp() OVERRIDE {
    AthenaTestBase::SetUp();
    // Override the delay to be instantaneous.
    ResourceManager::Get()->SetWaitTimeBetweenResourceManageCalls(0);
  }
  virtual void TearDown() OVERRIDE {
    while (!activity_list_.empty())
      DeleteActivity(activity_list_[0]);
    AthenaTestBase::TearDown();
  }

  TestActivity* CreateActivity(const std::string& title) {
    TestActivity* activity = new TestActivity(title);
    ActivityManager::Get()->AddActivity(activity);
    activity->SetCurrentState(Activity::ACTIVITY_INVISIBLE);
    activity_list_.push_back(activity);
    return activity;
  }

  void DeleteActivity(Activity* activity) {
    Activity::Delete(activity);
    RunAllPendingInMessageLoop();
    std::vector<TestActivity*>::iterator it = std::find(activity_list_.begin(),
                                                        activity_list_.end(),
                                                        activity);
    DCHECK(it != activity_list_.end());
    activity_list_.erase(it);
  }

 private:
  std::vector<TestActivity*> activity_list_;

  DISALLOW_COPY_AND_ASSIGN(ResourceManagerTest);
};

// Only creates and destroys it to see that the system gets properly shut down.
TEST_F(ResourceManagerTest, SimpleTest) {
}

// Test that we release an activity when the memory pressure goes critical.
TEST_F(ResourceManagerTest, OnCriticalWillUnloadOneActivity) {
  // Create a few dummy activities in the reverse order as we need them.
  TestActivity* app_unloadable2 = CreateActivity("unloadable2");
  TestActivity* app_unloadable1 = CreateActivity("unloadable1");
  TestActivity* app_visible = CreateActivity("visible");
  app_visible->set_visible(true);
  app_unloadable1->set_visible(false);
  app_unloadable2->set_visible(false);

  // Set the initial visibility states.
  app_visible->SetCurrentState(Activity::ACTIVITY_VISIBLE);
  app_unloadable1->SetCurrentState(Activity::ACTIVITY_INVISIBLE);
  app_unloadable2->SetCurrentState(Activity::ACTIVITY_INVISIBLE);

  // Call the resource manager and say we are in a critical memory condition.
  ResourceManager::Get()->SetMemoryPressureAndStopMonitoring(
      MemoryPressureObserver::MEMORY_PRESSURE_CRITICAL);
  DCHECK_NE(Activity::ACTIVITY_UNLOADED, app_visible->GetCurrentState());
  DCHECK_NE(Activity::ACTIVITY_UNLOADED, app_unloadable1->GetCurrentState());
  DCHECK_EQ(Activity::ACTIVITY_UNLOADED, app_unloadable2->GetCurrentState());

  // Calling it a second time will release the second app.
  ResourceManager::Get()->SetMemoryPressureAndStopMonitoring(
      MemoryPressureObserver::MEMORY_PRESSURE_CRITICAL);
  DCHECK_NE(Activity::ACTIVITY_UNLOADED, app_visible->GetCurrentState());
  DCHECK_EQ(Activity::ACTIVITY_UNLOADED, app_unloadable1->GetCurrentState());
  DCHECK_EQ(Activity::ACTIVITY_UNLOADED, app_unloadable2->GetCurrentState());

  // Calling it once more will change nothing.
  ResourceManager::Get()->SetMemoryPressureAndStopMonitoring(
      MemoryPressureObserver::MEMORY_PRESSURE_CRITICAL);
  DCHECK_NE(Activity::ACTIVITY_UNLOADED, app_visible->GetCurrentState());
  DCHECK_EQ(Activity::ACTIVITY_UNLOADED, app_unloadable1->GetCurrentState());
  DCHECK_EQ(Activity::ACTIVITY_UNLOADED, app_unloadable2->GetCurrentState());
}

// Test that media playing activities only get unloaded if there is no other
// way.
TEST_F(ResourceManagerTest, OnCriticalMediaHandling) {
  // Create a few dummy activities in the reverse order as we need them.
  TestActivity* app_media_locked2 = CreateActivity("medialocked2");
  TestActivity* app_unloadable = CreateActivity("unloadable2");
  TestActivity* app_media_locked1 = CreateActivity("medialocked1");
  TestActivity* app_visible = CreateActivity("visible");
  app_visible->set_visible(true);
  app_unloadable->set_visible(false);
  app_media_locked1->set_visible(false);
  app_media_locked2->set_visible(false);

  app_media_locked1->set_media_state(
      Activity::ACTIVITY_MEDIA_STATE_AUDIO_PLAYING);
  app_media_locked2->set_media_state(Activity::ACTIVITY_MEDIA_STATE_RECORDING);

  // Set the initial visibility states.
  app_visible->SetCurrentState(Activity::ACTIVITY_VISIBLE);
  app_media_locked1->SetCurrentState(Activity::ACTIVITY_INVISIBLE);
  app_unloadable->SetCurrentState(Activity::ACTIVITY_INVISIBLE);
  app_media_locked2->SetCurrentState(Activity::ACTIVITY_INVISIBLE);

  DCHECK_NE(Activity::ACTIVITY_UNLOADED, app_visible->GetCurrentState());
  DCHECK_NE(Activity::ACTIVITY_UNLOADED, app_media_locked1->GetCurrentState());
  DCHECK_NE(Activity::ACTIVITY_UNLOADED, app_unloadable->GetCurrentState());
  DCHECK_NE(Activity::ACTIVITY_UNLOADED, app_media_locked2->GetCurrentState());

  // Calling it with a critical situation first, it will release the non media
  // locked app.
  ResourceManager::Get()->SetMemoryPressureAndStopMonitoring(
      MemoryPressureObserver::MEMORY_PRESSURE_CRITICAL);
  DCHECK_NE(Activity::ACTIVITY_UNLOADED, app_visible->GetCurrentState());
  DCHECK_NE(Activity::ACTIVITY_UNLOADED, app_media_locked1->GetCurrentState());
  DCHECK_EQ(Activity::ACTIVITY_UNLOADED, app_unloadable->GetCurrentState());
  DCHECK_NE(Activity::ACTIVITY_UNLOADED, app_media_locked2->GetCurrentState());

  // Calling it the second time, the oldest media playing activity will get
  // unloaded.
  ResourceManager::Get()->SetMemoryPressureAndStopMonitoring(
      MemoryPressureObserver::MEMORY_PRESSURE_CRITICAL);
  DCHECK_NE(Activity::ACTIVITY_UNLOADED, app_visible->GetCurrentState());
  DCHECK_NE(Activity::ACTIVITY_UNLOADED, app_media_locked1->GetCurrentState());
  DCHECK_EQ(Activity::ACTIVITY_UNLOADED, app_unloadable->GetCurrentState());
  DCHECK_EQ(Activity::ACTIVITY_UNLOADED, app_media_locked2->GetCurrentState());

  // Calling it the third time, the oldest media playing activity will get
  // unloaded.
  ResourceManager::Get()->SetMemoryPressureAndStopMonitoring(
      MemoryPressureObserver::MEMORY_PRESSURE_CRITICAL);
  DCHECK_NE(Activity::ACTIVITY_UNLOADED, app_visible->GetCurrentState());
  DCHECK_EQ(Activity::ACTIVITY_UNLOADED, app_media_locked1->GetCurrentState());
  DCHECK_EQ(Activity::ACTIVITY_UNLOADED, app_unloadable->GetCurrentState());
  DCHECK_EQ(Activity::ACTIVITY_UNLOADED, app_media_locked2->GetCurrentState());
}

// Test the visibility of items.
TEST_F(ResourceManagerTest, VisibilityChanges) {
  // Create a few dummy activities in the reverse order as we need them.
  TestActivity* app4 = CreateActivity("app4");
  TestActivity* app3 = CreateActivity("app3");
  TestActivity* app2 = CreateActivity("app2");
  TestActivity* app1 = CreateActivity("app1");
  app1->SetCurrentState(Activity::ACTIVITY_VISIBLE);
  app2->SetCurrentState(Activity::ACTIVITY_VISIBLE);
  app3->SetCurrentState(Activity::ACTIVITY_VISIBLE);
  app4->SetCurrentState(Activity::ACTIVITY_VISIBLE);

  // Applying low resource pressure should keep everything as is.
  ResourceManager::Get()->SetMemoryPressureAndStopMonitoring(
      MemoryPressureObserver::MEMORY_PRESSURE_LOW);
  EXPECT_EQ(Activity::ACTIVITY_VISIBLE, app1->GetCurrentState());
  EXPECT_EQ(Activity::ACTIVITY_VISIBLE, app2->GetCurrentState());
  EXPECT_EQ(Activity::ACTIVITY_VISIBLE, app3->GetCurrentState());
  EXPECT_EQ(Activity::ACTIVITY_VISIBLE, app4->GetCurrentState());

  // Applying moderate resource pressure we should see 3 visible activities.
  // This is testing an internal algorithm constant, but for the time being
  // this should suffice.
  ResourceManager::Get()->SetMemoryPressureAndStopMonitoring(
      MemoryPressureObserver::MEMORY_PRESSURE_MODERATE);
  EXPECT_EQ(Activity::ACTIVITY_VISIBLE, app1->GetCurrentState());
  EXPECT_EQ(Activity::ACTIVITY_VISIBLE, app2->GetCurrentState());
  EXPECT_EQ(Activity::ACTIVITY_VISIBLE, app3->GetCurrentState());
  EXPECT_EQ(Activity::ACTIVITY_INVISIBLE, app4->GetCurrentState());

  // Applying higher pressure should get rid of everything unneeded.
  ResourceManager::Get()->SetMemoryPressureAndStopMonitoring(
      MemoryPressureObserver::MEMORY_PRESSURE_CRITICAL);
  EXPECT_EQ(Activity::ACTIVITY_VISIBLE, app1->GetCurrentState());
  EXPECT_EQ(Activity::ACTIVITY_INVISIBLE, app2->GetCurrentState());
  EXPECT_EQ(Activity::ACTIVITY_INVISIBLE, app3->GetCurrentState());
  // Note: This might very well be unloaded with this memory pressure.
  EXPECT_NE(Activity::ACTIVITY_VISIBLE, app4->GetCurrentState());

  // Once the split view mode gets turned on, more windows should become
  // visible.
  WindowManager::Get()->ToggleSplitViewForTest();
  EXPECT_EQ(Activity::ACTIVITY_VISIBLE, app1->GetCurrentState());
  EXPECT_EQ(Activity::ACTIVITY_VISIBLE, app2->GetCurrentState());
  EXPECT_NE(Activity::ACTIVITY_VISIBLE, app3->GetCurrentState());
  EXPECT_NE(Activity::ACTIVITY_VISIBLE, app4->GetCurrentState());

  // Going back to a relaxed memory pressure should reload the old activities.
  ResourceManager::Get()->SetMemoryPressureAndStopMonitoring(
      MemoryPressureObserver::MEMORY_PRESSURE_LOW);
  EXPECT_EQ(Activity::ACTIVITY_VISIBLE, app1->GetCurrentState());
  EXPECT_EQ(Activity::ACTIVITY_VISIBLE, app2->GetCurrentState());
  EXPECT_EQ(Activity::ACTIVITY_VISIBLE, app3->GetCurrentState());
  EXPECT_NE(Activity::ACTIVITY_INVISIBLE, app4->GetCurrentState());
}

// Make sure that an activity which got just reduced from visible to invisible,
// does not get thrown out of memory in the same step.
TEST_F(ResourceManagerTest, NoUnloadFromVisible) {
  // The timeout override in milliseconds.
  const int kTimeoutOverrideInMs = 20;
  // Tell the resource manager to wait for 20ms between calls.
  ResourceManager::Get()->SetWaitTimeBetweenResourceManageCalls(
      kTimeoutOverrideInMs);

  // Create a few dummy activities in the reverse order as we need them.
  TestActivity* app2 = CreateActivity("app2");
  TestActivity* app1 = CreateActivity("app1");
  app1->SetCurrentState(Activity::ACTIVITY_VISIBLE);
  app2->SetCurrentState(Activity::ACTIVITY_VISIBLE);

  // Applying low resource pressure should turn one item invisible.
  ResourceManager::Get()->SetMemoryPressureAndStopMonitoring(
      MemoryPressureObserver::MEMORY_PRESSURE_CRITICAL);
  EXPECT_EQ(Activity::ACTIVITY_VISIBLE, app1->GetCurrentState());
  EXPECT_EQ(Activity::ACTIVITY_INVISIBLE, app2->GetCurrentState());

  // Trying to apply the memory pressure again does not do anything.
  ResourceManager::Get()->SetMemoryPressureAndStopMonitoring(
      MemoryPressureObserver::MEMORY_PRESSURE_CRITICAL);
  EXPECT_EQ(Activity::ACTIVITY_VISIBLE, app1->GetCurrentState());
  EXPECT_EQ(Activity::ACTIVITY_INVISIBLE, app2->GetCurrentState());

  // Waiting and applying the pressure again should unload it.
  usleep(kTimeoutOverrideInMs * 1000);
  ResourceManager::Get()->SetMemoryPressureAndStopMonitoring(
      MemoryPressureObserver::MEMORY_PRESSURE_CRITICAL);
  EXPECT_EQ(Activity::ACTIVITY_VISIBLE, app1->GetCurrentState());
  EXPECT_EQ(Activity::ACTIVITY_UNLOADED, app2->GetCurrentState());
}

// Make sure that ActivityVisibility changes will be updated instantaneously
// when the ResourceManager is called for operation.
TEST_F(ResourceManagerTest, VisibilityChangeIsInstantaneous) {
  // Create a few dummy activities in the reverse order as we need them.
  TestActivity* app3 = CreateActivity("app3");
  TestActivity* app2 = CreateActivity("app2");
  TestActivity* app1 = CreateActivity("app1");
  app1->SetCurrentState(Activity::ACTIVITY_VISIBLE);
  app2->SetCurrentState(Activity::ACTIVITY_VISIBLE);
  app3->SetCurrentState(Activity::ACTIVITY_VISIBLE);

  // Tell the resource manager to wait for a long time between calls.
  ResourceManager::Get()->SetWaitTimeBetweenResourceManageCalls(1000);

  // Applying higher pressure should get rid of everything unneeded.
  ResourceManager::Get()->SetMemoryPressureAndStopMonitoring(
      MemoryPressureObserver::MEMORY_PRESSURE_CRITICAL);
  EXPECT_EQ(Activity::ACTIVITY_VISIBLE, app1->GetCurrentState());
  EXPECT_EQ(Activity::ACTIVITY_INVISIBLE, app2->GetCurrentState());
  EXPECT_EQ(Activity::ACTIVITY_INVISIBLE, app3->GetCurrentState());

  // Setting now one window visible again and call a second time should
  // immediately change the state again.
  app2->SetCurrentState(Activity::ACTIVITY_VISIBLE);
  EXPECT_EQ(Activity::ACTIVITY_VISIBLE, app1->GetCurrentState());
  EXPECT_EQ(Activity::ACTIVITY_VISIBLE, app2->GetCurrentState());
  EXPECT_EQ(Activity::ACTIVITY_INVISIBLE, app3->GetCurrentState());
  ResourceManager::Get()->SetMemoryPressureAndStopMonitoring(
      MemoryPressureObserver::MEMORY_PRESSURE_CRITICAL);

  EXPECT_EQ(Activity::ACTIVITY_VISIBLE, app1->GetCurrentState());
  EXPECT_EQ(Activity::ACTIVITY_INVISIBLE, app2->GetCurrentState());
  EXPECT_EQ(Activity::ACTIVITY_INVISIBLE, app3->GetCurrentState());
}

// Make sure that a timeout has to be reached before another resource managing
// operations can be performed.
TEST_F(ResourceManagerTest, ResourceChangeDelayed) {
  // Create a few dummy activities in the reverse order as we need them.
  TestActivity* app4 = CreateActivity("app4");
  TestActivity* app3 = CreateActivity("app3");
  TestActivity* app2 = CreateActivity("app2");
  TestActivity* app1 = CreateActivity("app1");
  app1->SetCurrentState(Activity::ACTIVITY_VISIBLE);
  app2->SetCurrentState(Activity::ACTIVITY_INVISIBLE);
  app3->SetCurrentState(Activity::ACTIVITY_INVISIBLE);
  app4->SetCurrentState(Activity::ACTIVITY_INVISIBLE);

  // The timeout override in milliseconds.
  const int kTimeoutOverrideInMs = 20;
  // Tell the resource manager to wait for 20ms between calls.
  ResourceManager::Get()->SetWaitTimeBetweenResourceManageCalls(
      kTimeoutOverrideInMs);
  // Applying higher pressure should get unload the oldest activity.
  ResourceManager::Get()->SetMemoryPressureAndStopMonitoring(
      MemoryPressureObserver::MEMORY_PRESSURE_CRITICAL);
  EXPECT_EQ(Activity::ACTIVITY_VISIBLE, app1->GetCurrentState());
  EXPECT_EQ(Activity::ACTIVITY_INVISIBLE, app2->GetCurrentState());
  EXPECT_EQ(Activity::ACTIVITY_INVISIBLE, app3->GetCurrentState());
  EXPECT_EQ(Activity::ACTIVITY_UNLOADED, app4->GetCurrentState());
  // Trying to apply the resource pressure again within the timeout time should
  // not trigger any operation.
  ResourceManager::Get()->SetMemoryPressureAndStopMonitoring(
      MemoryPressureObserver::MEMORY_PRESSURE_CRITICAL);
  EXPECT_EQ(Activity::ACTIVITY_VISIBLE, app1->GetCurrentState());
  EXPECT_EQ(Activity::ACTIVITY_INVISIBLE, app2->GetCurrentState());
  EXPECT_EQ(Activity::ACTIVITY_INVISIBLE, app3->GetCurrentState());
  EXPECT_EQ(Activity::ACTIVITY_UNLOADED, app4->GetCurrentState());

  // Passing the timeout however should allow for another call.
  usleep(kTimeoutOverrideInMs * 1000);
  ResourceManager::Get()->SetMemoryPressureAndStopMonitoring(
      MemoryPressureObserver::MEMORY_PRESSURE_CRITICAL);
  EXPECT_EQ(Activity::ACTIVITY_VISIBLE, app1->GetCurrentState());
  EXPECT_EQ(Activity::ACTIVITY_INVISIBLE, app2->GetCurrentState());
  EXPECT_EQ(Activity::ACTIVITY_UNLOADED, app3->GetCurrentState());
  EXPECT_EQ(Activity::ACTIVITY_UNLOADED, app4->GetCurrentState());
}

}  // namespace test
}  // namespace athena
