/// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/activity/public/activity.h"
#include "athena/activity/public/activity_manager.h"
#include "athena/activity/public/activity_view_model.h"
#include "athena/resource_manager/memory_pressure_notifier.h"
#include "athena/resource_manager/public/resource_manager.h"
#include "athena/test/athena_test_base.h"
#include "athena/wm/public/window_manager.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace athena {
namespace test {

namespace {

// A dummy test app activity which works without content / ShellAppWindow.
class TestActivity : public Activity,
                     public ActivityViewModel {
 public:
  TestActivity(std::string title) : title_(base::UTF8ToUTF16(title)),
                                    view_(new views::View()),
                                    current_state_(ACTIVITY_UNLOADED),
                                    media_state_(ACTIVITY_MEDIA_STATE_NONE),
                                    is_visible_(false) {};
  virtual ~TestActivity() {}

  void set_media_state(ActivityMediaState media_state) {
    media_state_ = media_state;
  }
  void set_visible(bool visible) { is_visible_ = visible; }

  // Activity overrides:
  virtual ActivityViewModel* GetActivityViewModel() OVERRIDE { return this; }
  virtual void SetCurrentState(ActivityState state) OVERRIDE {
    current_state_ = state;
  }
  virtual ActivityState GetCurrentState() OVERRIDE { return current_state_; }
  virtual bool IsVisible() OVERRIDE { return is_visible_; }
  virtual ActivityMediaState GetMediaState() OVERRIDE { return media_state_; }
  virtual aura::Window* GetWindow() OVERRIDE {
    return view_->GetWidget()->GetNativeWindow();
  }

  // ActivityViewModel overrides:
  virtual void Init() OVERRIDE {}
  virtual SkColor GetRepresentativeColor() const OVERRIDE { return 0; }
  virtual base::string16 GetTitle() const OVERRIDE { return title_; }
  virtual bool UsesFrame() const OVERRIDE { return true; }
  virtual views::View* GetContentsView() OVERRIDE { return view_; }
  virtual void CreateOverviewModeImage() OVERRIDE {}
  virtual gfx::ImageSkia GetOverviewModeImage() OVERRIDE { return image_; }
  virtual void PrepareContentsForOverview() OVERRIDE {}
  virtual void ResetContentsView() OVERRIDE {}

 private:
  // The presentation values.
  const base::string16 title_;
  gfx::ImageSkia image_;

  // The associated view.
  views::View* view_;

  // The current state.
  ActivityState current_state_;

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

  DCHECK_NE(Activity::ACTIVITY_UNLOADED, app_visible->GetCurrentState());
  DCHECK_NE(Activity::ACTIVITY_UNLOADED, app_unloadable1->GetCurrentState());
  DCHECK_NE(Activity::ACTIVITY_UNLOADED, app_unloadable2->GetCurrentState());

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
  WindowManager::GetInstance()->ToggleSplitViewForTest();
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

}  // namespace test
}  // namespace athena
