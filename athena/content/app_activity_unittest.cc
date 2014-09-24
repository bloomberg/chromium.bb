/// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/activity/public/activity_factory.h"
#include "athena/activity/public/activity_manager.h"
#include "athena/content/app_activity.h"
#include "athena/content/app_activity_registry.h"
#include "athena/content/public/app_registry.h"
#include "athena/extensions/public/extensions_delegate.h"
#include "athena/resource_manager/public/resource_manager.h"
#include "athena/test/athena_test_base.h"
#include "extensions/common/extension_set.h"
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
  explicit TestAppActivity(const std::string& app_id)
      : AppActivity(app_id),
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
    if (state == ACTIVITY_UNLOADED)
      app_activity_registry_->Unload();
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
  virtual views::Widget* CreateWidget() OVERRIDE { return NULL; }
  virtual gfx::ImageSkia GetOverviewModeImage() OVERRIDE {
    return gfx::ImageSkia();
  }

 private:
  // If known the registry which holds all activities for the associated app.
  AppActivityRegistry* app_activity_registry_;

  // The title of the activity.
  base::string16 title_;

  // Our view.
  views::View* view_;

  // The current state for this activity.
  ActivityState current_state_;

  DISALLOW_COPY_AND_ASSIGN(TestAppActivity);
};

// An AppContentDelegateClass which we can query for call stats.
class TestExtensionsDelegate : public ExtensionsDelegate {
 public:
  TestExtensionsDelegate() : unload_called_(0), restart_called_(0) {}
  virtual ~TestExtensionsDelegate() {}

  int unload_called() const { return unload_called_; }
  int restart_called() const { return restart_called_; }

  // ExtensionsDelegate:
  virtual content::BrowserContext* GetBrowserContext() const OVERRIDE {
    return NULL;
  }
  virtual const extensions::ExtensionSet& GetInstalledExtensions() OVERRIDE {
    return extension_set_;
  }
  // Unload an application. Returns true when unloaded.
  virtual bool UnloadApp(const std::string& app_id) OVERRIDE {
    unload_called_++;
    // Since we did not close anything we let the framework clean up.
    return false;
  }
  // Restarts an application. Returns true when the restart was initiated.
  virtual bool LaunchApp(const std::string& app_id) OVERRIDE {
    restart_called_++;
    return true;
  }

 private:
  int unload_called_;
  int restart_called_;

  extensions::ExtensionSet extension_set_;

  DISALLOW_COPY_AND_ASSIGN(TestExtensionsDelegate);
};

}  // namespace

// Our testing base.
class AppActivityTest : public AthenaTestBase {
 public:
  AppActivityTest() : test_extensions_delegate_(NULL) {}
  virtual ~AppActivityTest() {}

  // AthenaTestBase:
  virtual void SetUp() OVERRIDE {
    AthenaTestBase::SetUp();
    // Create and install our TestAppContentDelegate with instrumentation.
    ExtensionsDelegate::Shutdown();
    // The instance will be deleted by ExtensionsDelegate::Shutdown().
    test_extensions_delegate_ = new TestExtensionsDelegate();
  }

  // A function to create an Activity.
  TestAppActivity* CreateAppActivity(const std::string& app_id) {
    TestAppActivity* activity = new TestAppActivity(app_id);
    ActivityManager::Get()->AddActivity(activity);
    return activity;
  }

  void DeleteActivity(Activity* activity) {
    Activity::Delete(activity);
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

  // To avoid interference of the ResourceManager in these AppActivity
  // framework tests, we disable the ResourceManager for some tests.
  // Every use/interference of this function gets explained.
  void DisableResourceManager() {
    ResourceManager::Get()->Pause(true);
  }

 protected:
  TestExtensionsDelegate* test_extensions_delegate() {
    return test_extensions_delegate_;
  }

 private:
  TestExtensionsDelegate* test_extensions_delegate_;

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
    DeleteActivity(app_activity);
  }
  EXPECT_EQ(0, AppRegistry::Get()->NumberOfApplications());
  EXPECT_EQ(0, test_extensions_delegate()->unload_called());
  EXPECT_EQ(0, test_extensions_delegate()->restart_called());
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
    DeleteActivity(app_activity1);
    DeleteActivity(app_activity2);
  }
  EXPECT_EQ(0, AppRegistry::Get()->NumberOfApplications());
  EXPECT_EQ(0, test_extensions_delegate()->unload_called());
  EXPECT_EQ(0, test_extensions_delegate()->restart_called());
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
    DeleteActivity(app_activity1);
    EXPECT_EQ(1, AppRegistry::Get()->NumberOfApplications());
    EXPECT_EQ(1, app_activity2->app_activity_registry()->NumberOfActivities());
    DeleteActivity(app_activity2);
  }
  EXPECT_EQ(0, AppRegistry::Get()->NumberOfApplications());
  {
    TestAppActivity* app_activity1 = CreateAppActivity(kDummyApp1);
    TestAppActivity* app_activity2 = CreateAppActivity(kDummyApp1);
    EXPECT_EQ(1, AppRegistry::Get()->NumberOfApplications());
    EXPECT_EQ(2, app_activity1->app_activity_registry()->NumberOfActivities());
    EXPECT_EQ(app_activity1->app_activity_registry(),
              app_activity2->app_activity_registry());
    DeleteActivity(app_activity2);
    EXPECT_EQ(1, AppRegistry::Get()->NumberOfApplications());
    EXPECT_EQ(1, app_activity1->app_activity_registry()->NumberOfActivities());
    DeleteActivity(app_activity1);
  }
  EXPECT_EQ(0, AppRegistry::Get()->NumberOfApplications());
  EXPECT_EQ(0, test_extensions_delegate()->unload_called());
  EXPECT_EQ(0, test_extensions_delegate()->restart_called());
}

// Test unload and the creation of the proxy, then "closing the activity".
TEST_F(AppActivityTest, TestUnloadFollowedByClose) {
  // We do not want the ResourceManager to interfere with this test. In this
  // case it would (dependent on its current internal implementation)
  // automatically re-load the unloaded activity if it is in an "active"
  // position.
  DisableResourceManager();
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
  RunAllPendingInMessageLoop();
  EXPECT_EQ(0, test_extensions_delegate()->unload_called());

  // After setting our activity to unloaded however the application should get
  // unloaded as requested.
  app_activity->SetCurrentState(Activity::ACTIVITY_UNLOADED);
  RunAllPendingInMessageLoop();
  EXPECT_EQ(1, test_extensions_delegate()->unload_called());

  // Check that our created application is gone, and instead a proxy got
  // created.
  ASSERT_EQ(1, AppRegistry::Get()->NumberOfApplications());
  ASSERT_EQ(app_activity_registry,
            AppRegistry::Get()->GetAppActivityRegistry(kDummyApp1, NULL));
  EXPECT_EQ(0, app_activity_registry->NumberOfActivities());
  Activity* activity_proxy = app_activity_registry->unloaded_activity_proxy();
  ASSERT_TRUE(activity_proxy);
  EXPECT_NE(app_activity, activity_proxy);
  EXPECT_EQ(Activity::ACTIVITY_UNLOADED, activity_proxy->GetCurrentState());
  EXPECT_EQ(0, test_extensions_delegate()->restart_called());

  // Close the proxy object and make sure that nothing bad happens.
  DeleteActivity(activity_proxy);

  EXPECT_EQ(0, AppRegistry::Get()->NumberOfApplications());
  EXPECT_EQ(1, test_extensions_delegate()->unload_called());
  EXPECT_EQ(0, test_extensions_delegate()->restart_called());
}

// Test that when unloading an app while multiple apps / activities are present,
// the proxy gets created in the correct location.
TEST_F(AppActivityTest, TestUnloadProxyLocation) {
  // Disable the resource manager since some build bots run this test for an
  // extended amount of time which allows the MemoryPressureNotifier to fire.
  DisableResourceManager();
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
  RunAllPendingInMessageLoop();
  EXPECT_EQ(0, app_activity_registry->NumberOfActivities());
  Activity* activity_proxy = app_activity_registry->unloaded_activity_proxy();
  RunAllPendingInMessageLoop();

  EXPECT_EQ(2, GetActivityPosition(app_activity1b));
  EXPECT_EQ(1, GetActivityPosition(activity_proxy));
  EXPECT_EQ(0, GetActivityPosition(app_activity1a));
  EXPECT_EQ(0, test_extensions_delegate()->restart_called());

  DeleteActivity(activity_proxy);
  DeleteActivity(app_activity1b);
  DeleteActivity(app_activity1a);
}

// Test that an unload with multiple activities of the same app will only unload
// when all activities were marked for unloading.
TEST_F(AppActivityTest, TestMultipleActivityUnloadLock) {
  // Disable the resource manager since some build bots run this test for an
  // extended amount of time which allows the MemoryPressureNotifier to fire.
  DisableResourceManager();

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
  RunAllPendingInMessageLoop();
  EXPECT_EQ(0, test_extensions_delegate()->unload_called());
  app_activity2->SetCurrentState(Activity::ACTIVITY_UNLOADED);
  RunAllPendingInMessageLoop();
  EXPECT_EQ(0, test_extensions_delegate()->unload_called());
  app_activity3->SetCurrentState(Activity::ACTIVITY_UNLOADED);
  RunAllPendingInMessageLoop();
  EXPECT_EQ(1, test_extensions_delegate()->unload_called());

  // Now there should only be the proxy activity left.
  ASSERT_EQ(1, AppRegistry::Get()->NumberOfApplications());
  ASSERT_EQ(app_activity_registry,
            AppRegistry::Get()->GetAppActivityRegistry(kDummyApp1, NULL));
  EXPECT_EQ(0, app_activity_registry->NumberOfActivities());
  Activity* activity_proxy = app_activity_registry->unloaded_activity_proxy();
  ASSERT_TRUE(activity_proxy);
  EXPECT_NE(app_activity1, activity_proxy);
  EXPECT_NE(app_activity2, activity_proxy);
  EXPECT_NE(app_activity3, activity_proxy);
  EXPECT_EQ(Activity::ACTIVITY_UNLOADED, activity_proxy->GetCurrentState());

  // Close the proxy object and make sure that nothing bad happens.
  DeleteActivity(activity_proxy);

  EXPECT_EQ(0, AppRegistry::Get()->NumberOfApplications());
  EXPECT_EQ(1, test_extensions_delegate()->unload_called());
  EXPECT_EQ(0, test_extensions_delegate()->restart_called());
}

// Test that activating the proxy will reload the application.
TEST_F(AppActivityTest, TestUnloadWithReload) {
  // We do not want the ResourceManager to interfere with this test. In this
  // case it would (dependent on its current internal implementation)
  // automatically re-load the unloaded activity if it is in an "active"
  // position.
  DisableResourceManager();
  EXPECT_EQ(0, AppRegistry::Get()->NumberOfApplications());

  TestAppActivity* app_activity = CreateAppActivity(kDummyApp1);
  AppActivityRegistry* app_activity_registry =
      app_activity->app_activity_registry();

  // Unload the activity.
  app_activity->SetCurrentState(Activity::ACTIVITY_UNLOADED);
  RunAllPendingInMessageLoop();
  EXPECT_EQ(1, test_extensions_delegate()->unload_called());

  // Try to activate the activity again. This will force the application to
  // reload.
  Activity* activity_proxy = app_activity_registry->unloaded_activity_proxy();
  activity_proxy->SetCurrentState(Activity::ACTIVITY_VISIBLE);
  EXPECT_EQ(1, test_extensions_delegate()->restart_called());

  // However - the restart in this test framework does not really restart and
  // all objects should be still there..
  EXPECT_EQ(1, AppRegistry::Get()->NumberOfApplications());
  EXPECT_TRUE(app_activity_registry->unloaded_activity_proxy());
  Activity::Delete(app_activity_registry->unloaded_activity_proxy());
  EXPECT_EQ(0, AppRegistry::Get()->NumberOfApplications());
}

}  // namespace test
}  // namespace athena
