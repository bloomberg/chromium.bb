// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/activity/public/activity.h"
#include "athena/resource_manager/public/resource_manager.h"
#include "athena/test/base/activity_lifetime_tracker.h"
#include "athena/test/base/test_util.h"
#include "athena/test/chrome/athena_app_browser_test.h"
#include "athena/wm/public/window_list_provider.h"
#include "athena/wm/public/window_manager.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/aura/client/focus_client.h"
#include "ui/wm/core/window_util.h"

namespace athena {

namespace {
// The test URL to navigate to.
const char kTestUrl[] = "chrome:about";
}

class AppActivityBrowserTest : public AthenaAppBrowserTest {
 public:
  AppActivityBrowserTest() {}
  ~AppActivityBrowserTest() override {}

  // AthenaAppBrowserTest:
  void SetUpOnMainThread() override {
    tracker_.reset(new ActivityLifetimeTracker);
    AthenaAppBrowserTest::SetUpOnMainThread();
  }

  void TearDownOnMainThread() override {
    tracker_.reset();
    AthenaAppBrowserTest::TearDownOnMainThread();
  }

 protected:
  // A |proxy_activity| got deleted and this function waits, using the |tracker|
  // until the application got restarted returning the new application activity.
  Activity* WaitForProxyDestruction(Activity* proxy_activity) {
    ActivityLifetimeTracker tracker;
    void* deleted_activity = nullptr;
    Activity* app_activity = nullptr;
    while (!app_activity && !deleted_activity) {
      deleted_activity = tracker_->GetDeletedActivityAndReset();
      app_activity = tracker_->GetNewActivityAndReset();
      test_util::WaitUntilIdle();
      usleep(5000);
    }
    EXPECT_EQ(deleted_activity, proxy_activity);
    EXPECT_TRUE(app_activity);
    return app_activity;
  }

  // Returns true when the window of the |activity| has the focus.
  bool IsActivityActive(Activity* activity) {
    return wm::IsActiveWindow(activity->GetWindow());
  }

  // Create a setup where the frontmost window is a web activity and then
  // an unloaded app activity (proxy). Note that the resource manager will be
  // set to CRITICAL to force the application to unload.
  void SetUpWebAndProxyActivity(Activity** web_activity,
                                Activity** proxy_activity) {
    // Create an application activity.
    Activity* app_activity = CreateTestAppActivity(GetTestAppID());
    ASSERT_TRUE(app_activity);
    EXPECT_EQ(app_activity, tracker_->GetNewActivityAndReset());
    EXPECT_EQ(nullptr, tracker_->GetDeletedActivityAndReset());

    // Then a web activity (which will then be in front of the app).
    *web_activity = test_util::CreateTestWebActivity(
        GetBrowserContext(), base::UTF8ToUTF16("App1"), GURL(kTestUrl));
    ASSERT_TRUE(*web_activity);
    EXPECT_EQ(*web_activity, tracker_->GetNewActivityAndReset());
    EXPECT_EQ(nullptr, tracker_->GetDeletedActivityAndReset());

    const aura::Window::Windows& windows =
        WindowManager::Get()->GetWindowListProvider()->GetWindowList();

    // The order of windows should now be: Web activity, app activity.
    EXPECT_EQ(app_activity->GetWindow(), windows[0]);
    EXPECT_EQ((*web_activity)->GetWindow(), windows[1]);

    // We let the ResourceManager unload now the app. To accomplish this, we
    // first set the app to INIVSIBLE and then let the ResourceManager unload it
    // by turning on the critical memory pressure.
    app_activity->SetCurrentState(Activity::ACTIVITY_INVISIBLE);
    EXPECT_EQ(Activity::ACTIVITY_INVISIBLE, app_activity->GetCurrentState());
    test_util::SendTestMemoryPressureEvent(
        ResourceManager::MEMORY_PRESSURE_CRITICAL);
    test_util::WaitUntilIdle();

    *proxy_activity = tracker_->GetNewActivityAndReset();
    ASSERT_TRUE(*proxy_activity);
    EXPECT_NE(app_activity, *proxy_activity);
    EXPECT_EQ(app_activity, tracker_->GetDeletedActivityAndReset());

    // Check that the order of windows is correct (the proxy is at the second
    // location).
    EXPECT_EQ((*proxy_activity)->GetWindow(), windows[0]);
    EXPECT_EQ((*web_activity)->GetWindow(), windows[1]);
  }

 private:
  // The activity tracker which is used for asynchronous operations.
  scoped_ptr<ActivityLifetimeTracker> tracker_;
  DISALLOW_COPY_AND_ASSIGN(AppActivityBrowserTest);
};

// Tests that an application can be loaded.
IN_PROC_BROWSER_TEST_F(AppActivityBrowserTest, StartApplication) {
  // There should be an application we can start.
  ASSERT_TRUE(!GetTestAppID().empty());

  // We should be able to start the application.
  ASSERT_TRUE(CreateTestAppActivity(GetTestAppID()));
}

// Test that creating an application (without a particular activity order
// location) should activate it initially.
IN_PROC_BROWSER_TEST_F(AppActivityBrowserTest, CreatedAppGetsFocus) {
  Activity* web_activity = test_util::CreateTestWebActivity(
      GetBrowserContext(), base::UTF8ToUTF16("App1"), GURL(kTestUrl));
  EXPECT_TRUE(IsActivityActive(web_activity));

  Activity* app_activity = CreateTestAppActivity(GetTestAppID());
  EXPECT_TRUE(IsActivityActive(app_activity));
}

// Test that setting an application state to UNLOADED a proxy gets created and
// upon changing it to invisible it gets reloaded it its current list location.
IN_PROC_BROWSER_TEST_F(AppActivityBrowserTest, UnloadReloadApplicationInPlace) {
  // Set up the experiment.
  Activity* proxy_activity = nullptr;
  Activity* web_activity = nullptr;
  SetUpWebAndProxyActivity(&web_activity, &proxy_activity);
  // By returning to a low memory pressure the application should start again.
  test_util::SendTestMemoryPressureEvent(ResourceManager::MEMORY_PRESSURE_LOW);
  Activity* app_activity = WaitForProxyDestruction(proxy_activity);
  proxy_activity = nullptr;  // The proxy is gone now.

  // After this, the application should remain at its current location in the
  // stack and the current window should stay active.
  const aura::Window::Windows& windows =
      WindowManager::Get()->GetWindowListProvider()->GetWindowList();
  EXPECT_EQ(app_activity->GetWindow(), windows[0]);
  EXPECT_EQ(web_activity->GetWindow(), windows[1]);
  EXPECT_TRUE(IsActivityActive(web_activity));
}

// Check that activating an unloaded application will bring it properly to the
// front of the stack (and activate it).
IN_PROC_BROWSER_TEST_F(AppActivityBrowserTest, ReloadActivatedApplication) {
  // Set up the experiment.
  Activity* proxy_activity = nullptr;
  Activity* web_activity = nullptr;
  SetUpWebAndProxyActivity(&web_activity, &proxy_activity);

  // Activating the proxy should push back the web app, lauch the application,
  // kill the proxy and turn it active.
  proxy_activity->GetWindow()->Show();
  wm::ActivateWindow(proxy_activity->GetWindow());
  const aura::Window::Windows& windows =
      WindowManager::Get()->GetWindowListProvider()->GetWindowList();
  EXPECT_EQ(web_activity->GetWindow(), windows[0]);

  Activity* app_activity = WaitForProxyDestruction(proxy_activity);
  proxy_activity = nullptr;  // The proxy is gone now.

  // After this, the application should remain at its current location in the
  // stack and the activation focus should remain on the current window as well.
  EXPECT_EQ(app_activity->GetWindow(), windows[1]);
  EXPECT_TRUE(IsActivityActive(app_activity));
  EXPECT_EQ(web_activity->GetWindow(), windows[0]);
}

// Check that moving a proxy window to the front will properly restart the app
// and activate it.
IN_PROC_BROWSER_TEST_F(AppActivityBrowserTest, ReloadMovedApplication) {
  // Set up the experiment.
  Activity* proxy_activity = nullptr;
  Activity* web_activity = nullptr;
  SetUpWebAndProxyActivity(&web_activity, &proxy_activity);
  // Moving the window to the front will restart the app.
  WindowManager::Get()->GetWindowListProvider()->StackWindowFrontOf(
      proxy_activity->GetWindow(),
      web_activity->GetWindow());
  const aura::Window::Windows& windows =
      WindowManager::Get()->GetWindowListProvider()->GetWindowList();
  EXPECT_EQ(web_activity->GetWindow(), windows[0]);

  Activity* app_activity = WaitForProxyDestruction(proxy_activity);
  proxy_activity = nullptr;  // The proxy is gone now.

  // After this, the application should remain at its current location in the
  // stack and the activation focus should remain on the current window as well.
  EXPECT_EQ(app_activity->GetWindow(), windows[1]);
  EXPECT_TRUE(IsActivityActive(app_activity));
  EXPECT_EQ(web_activity->GetWindow(), windows[0]);
}

}  // namespace athena
