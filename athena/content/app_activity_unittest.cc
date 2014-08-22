/// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/activity/public/activity_factory.h"
#include "athena/activity/public/activity_manager.h"
#include "athena/content/app_activity.h"
#include "athena/content/app_activity_registry.h"
#include "athena/content/public/app_content_control_delegate.h"
#include "athena/content/public/app_registry.h"
#include "athena/test/athena_test_base.h"
#include "ui/aura/window.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"


namespace content {
class BrowserContext;
}

namespace athena {
namespace test {

namespace {

// An identifier for the running apps.
const char kDummyApp1[] = "aaaaaaa";
const char kDummyApp2[] = "bbbbbbb";

// A dummy test app activity which works without content / ShellAppWindow.
class TestAppActivity : public AppActivity {
 public:
  explicit TestAppActivity(const std::string& app_id) :
      AppActivity(NULL),
      app_id_(app_id),
      view_(new views::View()),
      current_state_(ACTIVITY_VISIBLE) {
    app_activity_registry_ =
        AppRegistry::Get()->GetAppActivityRegistry(app_id, NULL);
    app_activity_registry_->RegisterAppActivity(this);
  }
  virtual ~TestAppActivity() {
    app_activity_registry_->UnregisterAppActivity(this);
  }

  AppActivityRegistry* app_activity_registry() {
    return app_activity_registry_;
  }

  // Activity:
  virtual ActivityViewModel* GetActivityViewModel() OVERRIDE {
    return this;
  }
  virtual void SetCurrentState(Activity::ActivityState state) OVERRIDE {
    current_state_ = state;
  }
  virtual ActivityState GetCurrentState() OVERRIDE {
    return current_state_;
  }
  virtual bool IsVisible() OVERRIDE {
    return true;
  }
  virtual ActivityMediaState GetMediaState() OVERRIDE {
    return Activity::ACTIVITY_MEDIA_STATE_NONE;
  }
  virtual aura::Window* GetWindow() OVERRIDE {
    return view_->GetWidget()->GetNativeWindow();
  }

  // ActivityViewModel:
  virtual void Init() OVERRIDE {}
  virtual SkColor GetRepresentativeColor() const OVERRIDE { return 0; }
  virtual base::string16 GetTitle() const OVERRIDE { return title_; }
  virtual bool UsesFrame() const OVERRIDE { return true; }
  virtual views::View* GetContentsView() OVERRIDE { return view_; }
  virtual void CreateOverviewModeImage() OVERRIDE {}

 private:
  // If known the registry which holds all activities for the associated app.
  AppActivityRegistry* app_activity_registry_;

  // The application ID.
  const std::string& app_id_;

  // The title of the activity.
  base::string16 title_;

  // Our view.
  views::View* view_;

  // The current state for this activity.
  ActivityState current_state_;

  DISALLOW_COPY_AND_ASSIGN(TestAppActivity);
};

// An AppContentDelegateClass which we can query for call stats.
class TestAppContentControlDelegate : public AppContentControlDelegate {
 public:
  TestAppContentControlDelegate() : unload_called_(0),
                                    restart_called_(0) {}
  virtual ~TestAppContentControlDelegate() {}

  int unload_called() { return unload_called_; }
  int restart_called() { return restart_called_; }
  void SetExtensionID(const std::string& extension_id) {
    extension_id_to_return_ = extension_id;
  }

  // Unload an application. Returns true when unloaded.
  virtual bool UnloadApplication(
      const std::string& app_id,
      content::BrowserContext* browser_context) OVERRIDE {
    unload_called_++;
    // Since we did not close anything we let the framework clean up.
    return false;
  }
  // Restarts an application. Returns true when the restart was initiated.
  virtual bool RestartApplication(
      const std::string& app_id,
      content::BrowserContext* browser_context) OVERRIDE {
    restart_called_++;
    return true;
  }
  // Returns the application ID (or an empty string) for a given web content.
  virtual std::string GetApplicationID(
      content::WebContents* web_contents) OVERRIDE {
    return extension_id_to_return_;
  }

 private:
  int unload_called_;
  int restart_called_;
  std::string extension_id_to_return_;

  DISALLOW_COPY_AND_ASSIGN(TestAppContentControlDelegate);
};

}  // namespace

// Our testing base.
class AppActivityTest : public AthenaTestBase {
 public:
  AppActivityTest() : test_app_content_control_delegate_(NULL) {}
  virtual ~AppActivityTest() {}

  // AthenaTestBase:
  virtual void SetUp() OVERRIDE {
    AthenaTestBase::SetUp();
    // Create and install our TestAppContentDelegate with instrumentation.
    test_app_content_control_delegate_ = new TestAppContentControlDelegate();
    AppRegistry::Get()->SetDelegate(test_app_content_control_delegate_);
  }

  // A function to create an Activity.
  TestAppActivity* CreateAppActivity(const std::string& app_id) {
    TestAppActivity* activity = new TestAppActivity(app_id);
    ActivityManager::Get()->AddActivity(activity);
    return activity;
  }

  void CloseActivity(Activity* activity) {
    delete activity;
    RunAllPendingInMessageLoop();
  }

  // Get the position of the activity in the navigation history.
  int GetActivityPosition(Activity* activity) {
    aura::Window* window = activity->GetActivityViewModel()->GetContentsView()
                               ->GetWidget()->GetNativeWindow();
    aura::Window::Windows windows = activity->GetWindow()->parent()->children();
    for (size_t i = 0; i < windows.size(); i++) {
      if (windows[i] == window)
        return i;
    }
    return -1;
  }

 protected:
  TestAppContentControlDelegate* test_app_content_control_delegate() {
    return test_app_content_control_delegate_;
  }

 private:
  TestAppContentControlDelegate* test_app_content_control_delegate_;

  DISALLOW_COPY_AND_ASSIGN(AppActivityTest);
};

// Only creates one activity and destroys it.
TEST_F(AppActivityTest, OneAppActivity) {
  EXPECT_EQ(0, AppRegistry::Get()->NumberOfApplications());
  {
    TestAppActivity* app_activity = CreateAppActivity(kDummyApp1);
    EXPECT_EQ(1, AppRegistry::Get()->NumberOfApplications());
    EXPECT_EQ(1, app_activity->app_activity_registry()->NumberOfActivities());
    EXPECT_EQ(AppRegistry::Get()->GetAppActivityRegistry(kDummyApp1, NULL),
              app_activity->app_activity_registry());
    CloseActivity(app_activity);
  }
  EXPECT_EQ(0, AppRegistry::Get()->NumberOfApplications());
  EXPECT_EQ(0, test_app_content_control_delegate()->unload_called());
  EXPECT_EQ(0, test_app_content_control_delegate()->restart_called());
}

// Test running of two applications.
TEST_F(AppActivityTest, TwoAppsWithOneActivityEach) {
  EXPECT_EQ(0, AppRegistry::Get()->NumberOfApplications());
  {
    TestAppActivity* app_activity1 = CreateAppActivity(kDummyApp1);
    EXPECT_EQ(1, AppRegistry::Get()->NumberOfApplications());
    EXPECT_EQ(1, app_activity1->app_activity_registry()->NumberOfActivities());
    TestAppActivity* app_activity2 = CreateAppActivity(kDummyApp2);
    EXPECT_EQ(2, AppRegistry::Get()->NumberOfApplications());
    EXPECT_EQ(1, app_activity2->app_activity_registry()->NumberOfActivities());
    EXPECT_EQ(1, app_activity1->app_activity_registry()->NumberOfActivities());
    CloseActivity(app_activity1);
    CloseActivity(app_activity2);
  }
  EXPECT_EQ(0, AppRegistry::Get()->NumberOfApplications());
  EXPECT_EQ(0, test_app_content_control_delegate()->unload_called());
  EXPECT_EQ(0, test_app_content_control_delegate()->restart_called());
}

// Create and destroy two activities for the same application.
TEST_F(AppActivityTest, TwoAppActivities) {
  EXPECT_EQ(0, AppRegistry::Get()->NumberOfApplications());
  {
    TestAppActivity* app_activity1 = CreateAppActivity(kDummyApp1);
    TestAppActivity* app_activity2 = CreateAppActivity(kDummyApp1);
    EXPECT_EQ(1, AppRegistry::Get()->NumberOfApplications());
    EXPECT_EQ(2, app_activity1->app_activity_registry()->NumberOfActivities());
    EXPECT_EQ(app_activity1->app_activity_registry(),
              app_activity2->app_activity_registry());
    CloseActivity(app_activity1);
    EXPECT_EQ(1, AppRegistry::Get()->NumberOfApplications());
    EXPECT_EQ(1, app_activity2->app_activity_registry()->NumberOfActivities());
    CloseActivity(app_activity2);
  }
  EXPECT_EQ(0, AppRegistry::Get()->NumberOfApplications());
  {
    TestAppActivity* app_activity1 = CreateAppActivity(kDummyApp1);
    TestAppActivity* app_activity2 = CreateAppActivity(kDummyApp1);
    EXPECT_EQ(1, AppRegistry::Get()->NumberOfApplications());
    EXPECT_EQ(2, app_activity1->app_activity_registry()->NumberOfActivities());
    EXPECT_EQ(app_activity1->app_activity_registry(),
              app_activity2->app_activity_registry());
    CloseActivity(app_activity2);
    EXPECT_EQ(1, AppRegistry::Get()->NumberOfApplications());
    EXPECT_EQ(1, app_activity1->app_activity_registry()->NumberOfActivities());
    CloseActivity(app_activity1);
  }
  EXPECT_EQ(0, AppRegistry::Get()->NumberOfApplications());
  EXPECT_EQ(0, test_app_content_control_delegate()->unload_called());
  EXPECT_EQ(0, test_app_content_control_delegate()->restart_called());
}

// Test unload and the creation of the proxy, then "closing the activity".
TEST_F(AppActivityTest, TestUnloadFollowedByClose) {
  EXPECT_EQ(0, AppRegistry::Get()->NumberOfApplications());

  TestAppActivity* app_activity = CreateAppActivity(kDummyApp1);
  EXPECT_EQ(1, AppRegistry::Get()->NumberOfApplications());
  AppActivityRegistry* app_activity_registry =
      app_activity->app_activity_registry();
  EXPECT_EQ(1, app_activity_registry->NumberOfActivities());
  EXPECT_EQ(Activity::ACTIVITY_VISIBLE, app_activity->GetCurrentState());

  // Calling Unload now should not do anything since at least one activity in
  // the registry is still visible.
  app_activity_registry->Unload();
  EXPECT_EQ(0, test_app_content_control_delegate()->unload_called());

  // After setting our activity to unloaded however the application should get
  // unloaded as requested.
  app_activity->SetCurrentState(Activity::ACTIVITY_UNLOADED);
  app_activity_registry->Unload();
  EXPECT_EQ(1, test_app_content_control_delegate()->unload_called());

  // Check that our created application is gone, and instead a proxy got
  // created.
  ASSERT_EQ(1, AppRegistry::Get()->NumberOfApplications());
  ASSERT_EQ(app_activity_registry,
            AppRegistry::Get()->GetAppActivityRegistry(kDummyApp1, NULL));
  EXPECT_EQ(0, app_activity_registry->NumberOfActivities());
  Activity* activity_proxy =
      app_activity_registry->unloaded_activity_proxy_for_test();
  ASSERT_TRUE(activity_proxy);
  EXPECT_NE(app_activity, activity_proxy);
  EXPECT_EQ(Activity::ACTIVITY_UNLOADED, activity_proxy->GetCurrentState());

  // Close the proxy object and make sure that nothing bad happens.
  CloseActivity(activity_proxy);

  EXPECT_EQ(0, AppRegistry::Get()->NumberOfApplications());
  EXPECT_EQ(1, test_app_content_control_delegate()->unload_called());
  EXPECT_EQ(0, test_app_content_control_delegate()->restart_called());
}

// Test that when unloading an app while multiple apps / activities are present,
// the proxy gets created in the correct location.
TEST_F(AppActivityTest, TestUnloadProxyLocation) {
  // Set up some activities for some applications.
  TestAppActivity* app_activity1a = CreateAppActivity(kDummyApp1);
  TestAppActivity* app_activity2a = CreateAppActivity(kDummyApp2);
  TestAppActivity* app_activity2b = CreateAppActivity(kDummyApp2);
  TestAppActivity* app_activity1b = CreateAppActivity(kDummyApp1);
  EXPECT_EQ(3, GetActivityPosition(app_activity1b));
  EXPECT_EQ(2, GetActivityPosition(app_activity2b));
  EXPECT_EQ(1, GetActivityPosition(app_activity2a));
  EXPECT_EQ(0, GetActivityPosition(app_activity1a));

  // Unload an app and make sure that the proxy is in the newest activity slot.
  AppActivityRegistry* app_activity_registry =
      app_activity2a->app_activity_registry();
  app_activity2a->SetCurrentState(Activity::ACTIVITY_UNLOADED);
  app_activity2b->SetCurrentState(Activity::ACTIVITY_UNLOADED);
  app_activity2a->app_activity_registry()->Unload();
  EXPECT_EQ(0, app_activity_registry->NumberOfActivities());
  Activity* activity_proxy =
      app_activity_registry->unloaded_activity_proxy_for_test();
  RunAllPendingInMessageLoop();

  EXPECT_EQ(2, GetActivityPosition(app_activity1b));
  EXPECT_EQ(1, GetActivityPosition(activity_proxy));
  EXPECT_EQ(0, GetActivityPosition(app_activity1a));

  CloseActivity(activity_proxy);
  CloseActivity(app_activity1b);
  CloseActivity(app_activity1a);
}

// Test that an unload with multiple activities of the same app will only unload
// when all activities were marked for unloading.
TEST_F(AppActivityTest, TestMultipleActivityUnloadLock) {
  EXPECT_EQ(0, AppRegistry::Get()->NumberOfApplications());

  TestAppActivity* app_activity1 = CreateAppActivity(kDummyApp1);
  TestAppActivity* app_activity2 = CreateAppActivity(kDummyApp1);
  TestAppActivity* app_activity3 = CreateAppActivity(kDummyApp1);

  // Check that we have 3 activities of the same application.
  EXPECT_EQ(1, AppRegistry::Get()->NumberOfApplications());
  AppActivityRegistry* app_activity_registry =
      app_activity1->app_activity_registry();
  EXPECT_EQ(app_activity_registry, app_activity2->app_activity_registry());
  EXPECT_EQ(app_activity_registry, app_activity3->app_activity_registry());
  EXPECT_EQ(3, app_activity_registry->NumberOfActivities());
  EXPECT_EQ(Activity::ACTIVITY_VISIBLE, app_activity1->GetCurrentState());
  EXPECT_EQ(Activity::ACTIVITY_VISIBLE, app_activity2->GetCurrentState());
  EXPECT_EQ(Activity::ACTIVITY_VISIBLE, app_activity3->GetCurrentState());

  // After setting all activities to UNLOADED the application should unload.
  app_activity1->SetCurrentState(Activity::ACTIVITY_UNLOADED);
  app_activity1->app_activity_registry()->Unload();
  EXPECT_EQ(0, test_app_content_control_delegate()->unload_called());
  app_activity2->SetCurrentState(Activity::ACTIVITY_UNLOADED);
  app_activity2->app_activity_registry()->Unload();
  EXPECT_EQ(0, test_app_content_control_delegate()->unload_called());
  app_activity3->SetCurrentState(Activity::ACTIVITY_UNLOADED);
  app_activity3->app_activity_registry()->Unload();
  EXPECT_EQ(1, test_app_content_control_delegate()->unload_called());

  // Now there should only be the proxy activity left.
  ASSERT_EQ(1, AppRegistry::Get()->NumberOfApplications());
  ASSERT_EQ(app_activity_registry,
            AppRegistry::Get()->GetAppActivityRegistry(kDummyApp1, NULL));
  EXPECT_EQ(0, app_activity_registry->NumberOfActivities());
  Activity* activity_proxy =
      app_activity_registry->unloaded_activity_proxy_for_test();
  ASSERT_TRUE(activity_proxy);
  EXPECT_NE(app_activity1, activity_proxy);
  EXPECT_NE(app_activity2, activity_proxy);
  EXPECT_NE(app_activity3, activity_proxy);
  EXPECT_EQ(Activity::ACTIVITY_UNLOADED, activity_proxy->GetCurrentState());

  // Close the proxy object and make sure that nothing bad happens.
  CloseActivity(activity_proxy);

  EXPECT_EQ(0, AppRegistry::Get()->NumberOfApplications());
  EXPECT_EQ(1, test_app_content_control_delegate()->unload_called());
  EXPECT_EQ(0, test_app_content_control_delegate()->restart_called());
}

// Test that activating the proxy will reload the application.
TEST_F(AppActivityTest, TestUnloadWithReload) {
  EXPECT_EQ(0, AppRegistry::Get()->NumberOfApplications());

  TestAppActivity* app_activity = CreateAppActivity(kDummyApp1);
  AppActivityRegistry* app_activity_registry =
      app_activity->app_activity_registry();

  // Unload the activity.
  app_activity->SetCurrentState(Activity::ACTIVITY_UNLOADED);
  app_activity_registry->Unload();
  EXPECT_EQ(1, test_app_content_control_delegate()->unload_called());

  // Try to activate the activity again. This will force the application to
  // reload.
  Activity* activity_proxy =
      app_activity_registry->unloaded_activity_proxy_for_test();
  activity_proxy->SetCurrentState(Activity::ACTIVITY_VISIBLE);
  EXPECT_EQ(1, test_app_content_control_delegate()->restart_called());

  // However - the restart in this test framework does not really restart and
  // all objects should be gone now.
  EXPECT_EQ(0, AppRegistry::Get()->NumberOfApplications());
}

}  // namespace test
}  // namespace athena
