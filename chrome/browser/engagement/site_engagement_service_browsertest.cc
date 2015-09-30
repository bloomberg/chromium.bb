// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/task_runner.h"
#include "base/test/simple_test_clock.h"
#include "base/thread_task_runner_handle.h"
#include "base/timer/mock_timer.h"
#include "base/values.h"
#include "chrome/browser/engagement/site_engagement_helper.h"
#include "chrome/browser/engagement/site_engagement_service.h"
#include "chrome/browser/engagement/site_engagement_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/keycodes/keyboard_codes.h"

class TestSiteEngagementHelper : public SiteEngagementHelper {
 public:
  explicit TestSiteEngagementHelper(content::WebContents* web_contents,
                                    bool enable_callbacks)
      : SiteEngagementHelper(web_contents),
        enable_callbacks_(enable_callbacks) { }

  ~TestSiteEngagementHelper() override { }

  void RecordUserInput() override {
    SiteEngagementHelper::RecordUserInput();
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, quit_closure_);
  }

  // Must be called before initiating user input.
  void SetQuitClosure(base::Closure quit_closure) {
    quit_closure_ = quit_closure;
  }

  // Override to allow callback registration to be manually controlled in tests.
  bool ShouldRecordEngagement() override {
    return enable_callbacks_;
  }

 private:
  friend class SiteEngagementServiceBrowserTest;
  base::Closure quit_closure_;
  bool enable_callbacks_;

};

class SiteEngagementServiceBrowserTest : public InProcessBrowserTest {
 public:
  // Create a SiteEngagementHelper. Called here as friend class methods cannot
  // be called in tests.
  scoped_ptr<SiteEngagementHelper> CreateHelper(
      content::WebContents* web_contents) {
    scoped_ptr<SiteEngagementHelper> helper(
        new SiteEngagementHelper(web_contents));
    DCHECK(helper.get());

    return helper.Pass();
  }

  // Create a TestSiteEngagementHelper, which calls a quit closure when user
  // input is recorded.
  scoped_ptr<TestSiteEngagementHelper> CreateTestHelper(
      content::WebContents* web_contents,
      bool enable_callbacks) {
    scoped_ptr<TestSiteEngagementHelper> helper(
        new TestSiteEngagementHelper(web_contents, enable_callbacks));
    DCHECK(helper.get());

    return helper.Pass();
  }

  // Simulate a key press event and handle it.
  void HandleKeyPress(SiteEngagementHelper* helper, ui::KeyboardCode key) {
    content::NativeWebKeyboardEvent event;
    event.windowsKeyCode = key;
    event.type = blink::WebKeyboardEvent::RawKeyDown;
    helper->input_tracker_.HandleKeyPressEvent(event);
  }

  // Simulate a mouse event and handle it.
  void HandleMouseEvent(SiteEngagementHelper* helper,
                        blink::WebMouseEvent::Button button,
                        blink::WebInputEvent::Type type) {
    blink::WebMouseEvent event;
    event.button = button;
    event.type = type;
    helper->input_tracker_.HandleMouseEvent(event);
  }

  // Set a timer object for test purposes.
  void SetHelperTimer(SiteEngagementHelper* helper,
                      scoped_ptr<base::Timer> timer) {
    helper->input_tracker_.SetTimerForTesting(timer.Pass());
  }

  bool CallbacksAdded(SiteEngagementHelper* helper) {
    return helper->input_tracker_.callbacks_added();
  }
};

IN_PROC_BROWSER_TEST_F(SiteEngagementServiceBrowserTest,
                       KeyPressEngagementAccumulation) {
  GURL url1("https://www.google.com/");
  GURL url2("http://www.google.com/");
  content::WebContents* web_contents =
    browser()->tab_strip_model()->GetActiveWebContents();

  scoped_ptr<SiteEngagementHelper> helper(CreateHelper(web_contents));
  SiteEngagementService* service =
    SiteEngagementServiceFactory::GetForProfile(browser()->profile());
  DCHECK(service);

  // Check that navigation triggers engagement.
  ui_test_utils::NavigateToURL(browser(), url1);
  EXPECT_DOUBLE_EQ(0.5, service->GetScore(url1));
  EXPECT_EQ(0, service->GetScore(url2));

  // Simulate a key press trigger and ensure it is treated correctly.
  HandleKeyPress(helper.get(), ui::VKEY_DOWN);

  EXPECT_DOUBLE_EQ(0.55, service->GetScore(url1));
  EXPECT_EQ(0, service->GetScore(url2));

  // Simulate three key presses, and ensure they are treated correctly.
  HandleKeyPress(helper.get(), ui::VKEY_UP);
  HandleKeyPress(helper.get(), ui::VKEY_RETURN);
  HandleKeyPress(helper.get(), ui::VKEY_J);

  EXPECT_DOUBLE_EQ(0.7, service->GetScore(url1));
  EXPECT_EQ(0, service->GetScore(url2));

  // Simulate key presses for a different link.
  ui_test_utils::NavigateToURL(browser(), url2);

  EXPECT_DOUBLE_EQ(0.7, service->GetScore(url1));
  EXPECT_DOUBLE_EQ(0.5, service->GetScore(url2));
  EXPECT_DOUBLE_EQ(1.2, service->GetTotalEngagementPoints());

  HandleKeyPress(helper.get(), ui::VKEY_K);
  EXPECT_DOUBLE_EQ(0.7, service->GetScore(url1));
  EXPECT_DOUBLE_EQ(0.55, service->GetScore(url2));
  EXPECT_DOUBLE_EQ(1.25, service->GetTotalEngagementPoints());
}

IN_PROC_BROWSER_TEST_F(SiteEngagementServiceBrowserTest,
                       MouseEventEngagementAccumulation) {
  GURL url1("https://www.google.com/");
  GURL url2("http://www.google.com/");
  content::WebContents* web_contents =
    browser()->tab_strip_model()->GetActiveWebContents();

  scoped_ptr<SiteEngagementHelper> helper(CreateHelper(web_contents));
  SiteEngagementService* service =
    SiteEngagementServiceFactory::GetForProfile(browser()->profile());
  DCHECK(service);

  ui_test_utils::NavigateToURL(browser(), url1);
  EXPECT_DOUBLE_EQ(0.5, service->GetScore(url1));
  EXPECT_EQ(0, service->GetScore(url2));

  HandleMouseEvent(helper.get(), blink::WebMouseEvent::ButtonLeft,
                   blink::WebInputEvent::MouseDown);

  EXPECT_DOUBLE_EQ(0.55, service->GetScore(url1));
  EXPECT_EQ(0, service->GetScore(url2));

  HandleMouseEvent(helper.get(), blink::WebMouseEvent::ButtonRight,
                   blink::WebInputEvent::MouseWheel);
  HandleMouseEvent(helper.get(), blink::WebMouseEvent::ButtonMiddle,
                   blink::WebInputEvent::MouseDown);
  HandleMouseEvent(helper.get(), blink::WebMouseEvent::ButtonLeft,
                   blink::WebInputEvent::MouseDown);

  EXPECT_DOUBLE_EQ(0.7, service->GetScore(url1));
  EXPECT_EQ(0, service->GetScore(url2));

  ui_test_utils::NavigateToURL(browser(), url2);

  EXPECT_DOUBLE_EQ(0.7, service->GetScore(url1));
  EXPECT_DOUBLE_EQ(0.5, service->GetScore(url2));
  EXPECT_DOUBLE_EQ(1.2, service->GetTotalEngagementPoints());

  HandleMouseEvent(helper.get(), blink::WebMouseEvent::ButtonLeft,
                   blink::WebInputEvent::MouseDown);
  EXPECT_DOUBLE_EQ(0.7, service->GetScore(url1));
  EXPECT_DOUBLE_EQ(0.55, service->GetScore(url2));
  EXPECT_DOUBLE_EQ(1.25, service->GetTotalEngagementPoints());
}

IN_PROC_BROWSER_TEST_F(SiteEngagementServiceBrowserTest,
                       MixedInputEngagementAccumulation) {
  GURL url1("https://www.google.com/");
  GURL url2("http://www.google.com/");
  content::WebContents* web_contents =
    browser()->tab_strip_model()->GetActiveWebContents();

  scoped_ptr<SiteEngagementHelper> helper(CreateHelper(web_contents));
  SiteEngagementService* service =
    SiteEngagementServiceFactory::GetForProfile(browser()->profile());
  DCHECK(service);

  ui_test_utils::NavigateToURL(browser(), url1);
  EXPECT_DOUBLE_EQ(0.5, service->GetScore(url1));
  EXPECT_EQ(0, service->GetScore(url2));

  HandleKeyPress(helper.get(), ui::VKEY_UP);
  HandleKeyPress(helper.get(), ui::VKEY_RETURN);
  HandleKeyPress(helper.get(), ui::VKEY_J);

  HandleMouseEvent(helper.get(), blink::WebMouseEvent::ButtonLeft,
                   blink::WebInputEvent::MouseDown);

  EXPECT_DOUBLE_EQ(0.7, service->GetScore(url1));
  EXPECT_EQ(0, service->GetScore(url2));

  HandleMouseEvent(helper.get(), blink::WebMouseEvent::ButtonRight,
                   blink::WebInputEvent::MouseDown);
  HandleMouseEvent(helper.get(), blink::WebMouseEvent::ButtonMiddle,
                   blink::WebInputEvent::MouseDown);
  HandleMouseEvent(helper.get(), blink::WebMouseEvent::ButtonNone,
                   blink::WebInputEvent::MouseWheel);

  EXPECT_DOUBLE_EQ(0.85, service->GetScore(url1));
  EXPECT_EQ(0, service->GetScore(url2));

  ui_test_utils::NavigateToURL(browser(), url2);

  EXPECT_DOUBLE_EQ(0.85, service->GetScore(url1));
  EXPECT_DOUBLE_EQ(0.5, service->GetScore(url2));
  EXPECT_DOUBLE_EQ(1.35, service->GetTotalEngagementPoints());

  HandleMouseEvent(helper.get(), blink::WebMouseEvent::ButtonNone,
                   blink::WebInputEvent::MouseWheel);
  HandleKeyPress(helper.get(), ui::VKEY_DOWN);

  EXPECT_DOUBLE_EQ(0.85, service->GetScore(url1));
  EXPECT_DOUBLE_EQ(0.6, service->GetScore(url2));
  EXPECT_DOUBLE_EQ(1.45, service->GetTotalEngagementPoints());
}

IN_PROC_BROWSER_TEST_F(SiteEngagementServiceBrowserTest,
                       CheckTimer) {
  GURL url1("https://www.google.com/");
  GURL url2("http://www.google.com/");
  content::WebContents* web_contents =
    browser()->tab_strip_model()->GetActiveWebContents();

  scoped_ptr<base::MockTimer> mock_timer(new base::MockTimer(true, false));
  base::MockTimer* timer = mock_timer.get();
  scoped_ptr<SiteEngagementHelper> helper(CreateHelper(web_contents));
  SetHelperTimer(helper.get(), mock_timer.Pass());

  SiteEngagementService* service =
    SiteEngagementServiceFactory::GetForProfile(browser()->profile());
  DCHECK(service);

  ui_test_utils::NavigateToURL(browser(), url1);
  EXPECT_DOUBLE_EQ(0.5, service->GetScore(url1));
  EXPECT_EQ(0, service->GetScore(url2));

  // Timer should not be running after navigation. It should start after input.
  EXPECT_FALSE(timer->IsRunning());
  HandleKeyPress(helper.get(), ui::VKEY_RETURN);
  EXPECT_TRUE(timer->IsRunning());

  EXPECT_DOUBLE_EQ(0.55, service->GetScore(url1));
  EXPECT_EQ(0, service->GetScore(url2));
  timer->Fire();

  EXPECT_FALSE(timer->IsRunning());
  HandleMouseEvent(helper.get(), blink::WebMouseEvent::ButtonNone,
                   blink::WebInputEvent::MouseWheel);
  EXPECT_TRUE(timer->IsRunning());

  EXPECT_DOUBLE_EQ(0.6, service->GetScore(url1));
  EXPECT_EQ(0, service->GetScore(url2));
  timer->Fire();

  EXPECT_FALSE(timer->IsRunning());
  ui_test_utils::NavigateToURL(browser(), url2);
  EXPECT_FALSE(timer->IsRunning());

  EXPECT_DOUBLE_EQ(0.6, service->GetScore(url1));
  EXPECT_DOUBLE_EQ(0.5, service->GetScore(url2));

  HandleMouseEvent(helper.get(), blink::WebMouseEvent::ButtonRight,
                   blink::WebInputEvent::MouseDown);
  EXPECT_TRUE(timer->IsRunning());

  EXPECT_DOUBLE_EQ(0.6, service->GetScore(url1));
  EXPECT_DOUBLE_EQ(0.55, service->GetScore(url2));
  EXPECT_DOUBLE_EQ(1.15, service->GetTotalEngagementPoints());
}

IN_PROC_BROWSER_TEST_F(SiteEngagementServiceBrowserTest,
                       SimulateInput) {
  GURL url1("https://www.google.com/");
  GURL url2("http://www.google.com/");
  content::WebContents* web_contents =
    browser()->tab_strip_model()->GetActiveWebContents();

  scoped_ptr<base::MockTimer> mock_timer(new base::MockTimer(true, false));
  base::MockTimer* timer = mock_timer.get();
  scoped_ptr<TestSiteEngagementHelper> helper(
      CreateTestHelper(web_contents, true));
  SetHelperTimer(helper.get(), mock_timer.Pass());

  SiteEngagementService* service =
    SiteEngagementServiceFactory::GetForProfile(browser()->profile());
  DCHECK(service);
  EXPECT_FALSE(timer->IsRunning());

  // Navigate and click. Ensure that the timer is running as expected.
  {
    base::RunLoop run_loop;
    helper->SetQuitClosure(run_loop.QuitClosure());

    ui_test_utils::NavigateToURL(browser(), url1);
    EXPECT_DOUBLE_EQ(0.5, service->GetScore(url1));
    EXPECT_EQ(0, service->GetScore(url2));
    EXPECT_FALSE(timer->IsRunning());
    EXPECT_TRUE(CallbacksAdded(helper.get()));

    // Click and wait until the run loop exits.
    content::SimulateMouseClick(web_contents, 0,
                                blink::WebMouseEvent::ButtonLeft);
    run_loop.Run();

    EXPECT_DOUBLE_EQ(0.55, service->GetScore(url1));
    EXPECT_EQ(0, service->GetScore(url2));
    EXPECT_TRUE(timer->IsRunning());
    EXPECT_FALSE(CallbacksAdded(helper.get()));
    timer->Fire();
  }

  // Navigate and click.
  {
    base::RunLoop run_loop;
    helper->SetQuitClosure(run_loop.QuitClosure());

    EXPECT_FALSE(timer->IsRunning());
    EXPECT_TRUE(CallbacksAdded(helper.get()));
    ui_test_utils::NavigateToURL(browser(), url1);
    EXPECT_DOUBLE_EQ(1.05, service->GetScore(url1));
    EXPECT_EQ(0, service->GetScore(url2));
    EXPECT_FALSE(timer->IsRunning());
    EXPECT_TRUE(CallbacksAdded(helper.get()));

    content::SimulateMouseClick(web_contents, 0,
                                blink::WebMouseEvent::ButtonLeft);
    run_loop.Run();

    EXPECT_DOUBLE_EQ(1.1, service->GetScore(url1));
    EXPECT_EQ(0, service->GetScore(url2));
    EXPECT_TRUE(timer->IsRunning());
    EXPECT_FALSE(CallbacksAdded(helper.get()));
    timer->Fire();
  }

  // Click only.
  {
    base::RunLoop run_loop;
    helper->SetQuitClosure(run_loop.QuitClosure());

    EXPECT_FALSE(timer->IsRunning());
    EXPECT_TRUE(CallbacksAdded(helper.get()));
    content::SimulateMouseClick(web_contents, 0,
                                blink::WebMouseEvent::ButtonMiddle);
    run_loop.Run();

    EXPECT_DOUBLE_EQ(1.15, service->GetScore(url1));
    EXPECT_EQ(0, service->GetScore(url2));
    EXPECT_TRUE(timer->IsRunning());
    EXPECT_FALSE(CallbacksAdded(helper.get()));
    timer->Fire();
  }

  // Navigate and click.
  {
    base::RunLoop run_loop;
    helper->SetQuitClosure(run_loop.QuitClosure());

    EXPECT_FALSE(timer->IsRunning());
    EXPECT_TRUE(CallbacksAdded(helper.get()));
    ui_test_utils::NavigateToURL(browser(), url1);
    EXPECT_DOUBLE_EQ(1.65, service->GetScore(url1));
    EXPECT_EQ(0, service->GetScore(url2));
    content::SimulateMouseClick(web_contents, 0,
                                blink::WebMouseEvent::ButtonRight);
    run_loop.Run();

    EXPECT_DOUBLE_EQ(1.7, service->GetScore(url1));
    EXPECT_EQ(0, service->GetScore(url2));
    EXPECT_TRUE(timer->IsRunning());
    EXPECT_FALSE(CallbacksAdded(helper.get()));

    // Simulate another event while the timer is running to make sure that this
    // input is ignored.
    content::SimulateMouseClick(web_contents, 0,
                                blink::WebMouseEvent::ButtonLeft);

    EXPECT_DOUBLE_EQ(1.7, service->GetScore(url1));
    EXPECT_EQ(0, service->GetScore(url2));
    EXPECT_TRUE(timer->IsRunning());
    EXPECT_FALSE(CallbacksAdded(helper.get()));
    timer->Fire();
  }
}

IN_PROC_BROWSER_TEST_F(SiteEngagementServiceBrowserTest,
                       ShowAndHide) {
  GURL url1("https://www.google.com/");
  GURL url2("http://www.google.com/");
  content::WebContents* web_contents =
    browser()->tab_strip_model()->GetActiveWebContents();

  scoped_ptr<base::MockTimer> mock_timer(new base::MockTimer(true, false));
  base::MockTimer* timer = mock_timer.get();
  scoped_ptr<TestSiteEngagementHelper> helper(
      CreateTestHelper(web_contents, true));
  SetHelperTimer(helper.get(), mock_timer.Pass());

  SiteEngagementService* service =
    SiteEngagementServiceFactory::GetForProfile(browser()->profile());
  DCHECK(service);
  EXPECT_FALSE(timer->IsRunning());

  // Navigate and hide.
  {
    base::RunLoop run_loop;
    helper->SetQuitClosure(run_loop.QuitClosure());

    ui_test_utils::NavigateToURL(browser(), url1);
    EXPECT_TRUE(CallbacksAdded(helper.get()));
    content::SimulateMouseClick(web_contents, 0,
                                blink::WebMouseEvent::ButtonRight);
    run_loop.Run();

    // Timer runs after input is recorded.
    EXPECT_TRUE(timer->IsRunning());
    EXPECT_FALSE(CallbacksAdded(helper.get()));

    ui_test_utils::NavigateToURLWithDisposition(
        browser(), url2, NEW_FOREGROUND_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_TAB);

    // Hiding the original tab should stop the timer.
    EXPECT_FALSE(timer->IsRunning());
    EXPECT_FALSE(CallbacksAdded(helper.get()));
    EXPECT_EQ(2, browser()->tab_strip_model()->count());

    // Timer should still be stopped on re-focus, but the input callbacks
    // should now be added.
    browser()->tab_strip_model()->ActivateTabAt(0, true);
    EXPECT_FALSE(timer->IsRunning());
    EXPECT_TRUE(CallbacksAdded(helper.get()));
  }

  // Click. Timer should be started.
  {
    base::RunLoop run_loop;
    helper->SetQuitClosure(run_loop.QuitClosure());

    content::SimulateMouseClick(web_contents, 0,
                                blink::WebMouseEvent::ButtonRight);
    run_loop.Run();

    // Timer runs after input is recorded, and after it fires the callbacks are
    // added.
    EXPECT_FALSE(CallbacksAdded(helper.get()));
    EXPECT_TRUE(timer->IsRunning());
    timer->Fire();
    EXPECT_FALSE(timer->IsRunning());
    EXPECT_TRUE(CallbacksAdded(helper.get()));
  }
}
