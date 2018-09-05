// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/top_controls_slide_controller.h"

#include <memory>
#include <numeric>
#include <vector>

#include "ash/public/cpp/ash_switches.h"
#include "ash/public/interfaces/constants.mojom.h"
#include "ash/public/interfaces/cros_display_config.mojom.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/path_service.h"
#include "base/strings/safe_sprintf.h"
#include "base/test/scoped_feature_list.h"
#include "cc/base/math_util.h"
#include "chrome/browser/ui/ash/tablet_mode_client.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/views/bookmarks/bookmark_bar_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/top_container_view.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/common/service_manager_connection.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "services/service_manager/public/cpp/connector.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/display/display.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/controls/native/native_view_host.h"
#include "ui/views/view_observer.h"

namespace {

enum class TopChromeShownState {
  kFullyShown,
  kFullyHidden,
};

// Using the given |generator| and the start and end points, it generates a
// gesture scroll sequence with appropriate velocity so that fling gesture
// scrolls are generated.
void GenerateGestureFlingScrollSequence(ui::test::EventGenerator* generator,
                                        const gfx::Point& start_point,
                                        const gfx::Point& end_point) {
  DCHECK(generator);
  generator->GestureScrollSequence(
      start_point, end_point,
      generator->CalculateScrollDurationForFlingVelocity(
          start_point, end_point, 100 /* velocity */, 2 /* steps */),
      2 /* steps */);
}

// Waits for the first non-empty paint for a given WebContents. To be able to
// test gesture scroll events reliably, we must wait until the tab is fully
// painted. Otherwise, the events will be ignored.
class TabNonEmptyPaintWaiter : public content::WebContentsObserver {
 public:
  explicit TabNonEmptyPaintWaiter(content::WebContents* web_contents)
      : content::WebContentsObserver(web_contents) {}

  ~TabNonEmptyPaintWaiter() override = default;

  void Wait() {
    if (web_contents()->CompletedFirstVisuallyNonEmptyPaint())
      return;

    run_loop_.Run();
  }

 private:
  // content::WebContentsObserver:
  void DidFirstVisuallyNonEmptyPaint() override { run_loop_.Quit(); }

  base::RunLoop run_loop_;

  DISALLOW_COPY_AND_ASSIGN(TabNonEmptyPaintWaiter);
};

// Waits for a given browser top controls shown ratio on a given browser window.
class TopControlsShownRatioWaiter {
 public:
  explicit TopControlsShownRatioWaiter(
      const TopControlsSlideController* controller)
      : controller_(controller) {}

  void WaitForRatio(float ratio) {
    waiting_for_shown_ratio_ = ratio;
    if (CheckRatio())
      return;

    run_loop_ = std::make_unique<base::RunLoop>();
    run_loop_->Run();
  }

 private:
  bool CheckRatio() {
    if (cc::MathUtil::IsWithinEpsilon(controller_->GetShownRatio(),
                                      waiting_for_shown_ratio_)) {
      if (run_loop_)
        run_loop_->Quit();

      return true;
    }

    ScheduleCheck();
    return false;
  }

  void ScheduleCheck() {
    base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(
            base::IgnoreResult(&TopControlsShownRatioWaiter::CheckRatio),
            base::Unretained(this)),
        base::TimeDelta::FromMilliseconds(100));
  }

  const TopControlsSlideController* controller_;

  std::unique_ptr<base::RunLoop> run_loop_;

  float waiting_for_shown_ratio_ = 0;

  DISALLOW_COPY_AND_ASSIGN(TopControlsShownRatioWaiter);
};

class TopControlsSlideControllerTest : public InProcessBrowserTest {
 public:
  TopControlsSlideControllerTest() = default;
  ~TopControlsSlideControllerTest() override = default;

  BrowserView* browser_view() const {
    return BrowserView::GetBrowserViewForBrowser(browser());
  }

  const TopControlsSlideController* top_controls_slide_controller() const {
    return browser_view()->top_controls_slide_controller();
  }

  // InProcessBrowserTest:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(
        features::kSlideTopChromeWithPageScrolls);
    InProcessBrowserTest::SetUp();
  }

  void SetUpDefaultCommandLine(base::CommandLine* command_line) override {
    InProcessBrowserTest::SetUpDefaultCommandLine(command_line);

    command_line->AppendSwitch(ash::switches::kAshEnableTabletMode);
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");

    // Add content/test/data so we can use cross_site_iframe_factory.html
    base::FilePath test_data_dir;
    ASSERT_TRUE(base::PathService::Get(base::DIR_SOURCE_ROOT, &test_data_dir));
    embedded_test_server()->ServeFilesFromDirectory(
        test_data_dir.AppendASCII("chrome/test/data/top_controls_scroll"));
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  void OpenUrlAtIndex(const GURL& url, int index) {
    AddTabAtIndex(0, url, ui::PAGE_TRANSITION_TYPED);
    TabNonEmptyPaintWaiter waiter(
        browser()->tab_strip_model()->GetActiveWebContents());
    waiter.Wait();
  }

  void ToggleTabletMode() {
    auto* tablet_mode_client = TabletModeClient::Get();
    tablet_mode_client->OnTabletModeToggled(
        !tablet_mode_client->tablet_mode_enabled());
  }

  bool GetTabletModeEnabled() const {
    return TabletModeClient::Get()->tablet_mode_enabled();
  }

  void CheckBrowserLayout(BrowserView* browser_view,
                          TopChromeShownState shown_state) const {
    const int top_controls_height = browser_view->GetTopControlsHeight();
    EXPECT_NE(top_controls_height, 0);

    ui::Layer* root_view_layer = browser_view->frame()->GetRootView()->layer();

    // The fully-shown and fully-hidden states are terminal states. We check
    // when we reach the steady state. The root view should not have a layer
    // now.
    ASSERT_FALSE(root_view_layer);

    // The contents layer transform should be restored to identity.
    gfx::Transform expected_transform;
    DCHECK(
        browser_view->contents_web_view()->holder()->GetNativeViewContainer());
    ui::Layer* contents_container_layer = browser_view->contents_web_view()
                                              ->holder()
                                              ->GetNativeViewContainer()
                                              ->layer();
    ASSERT_TRUE(contents_container_layer);
    CompareTranslations(expected_transform,
                        contents_container_layer->transform());

    // The BrowserView layout should be adjusted properly:
    const gfx::Rect& top_container_bounds =
        browser_view->top_container()->bounds();
    EXPECT_EQ(top_container_bounds.height(), top_controls_height);

    const int top_container_bottom = top_container_bounds.bottom();
    const gfx::Rect& contents_container_bounds =
        browser_view->contents_container()->bounds();
    // The top of the contents depends on whether there is a detached bookmark
    // bar as in the NTP page.
    int detached_bookmark_bar_height = 0;
    if (browser_view->bookmark_bar() &&
        browser_view->bookmark_bar()->IsDetached()) {
      // The detached bookmark bar appears to be part of the contents. It starts
      // right after the top container, and the contents container starts right
      // after it.
      const gfx::Rect& bookmark_bar_bounds =
          browser_view->bookmark_bar()->bounds();
      detached_bookmark_bar_height = bookmark_bar_bounds.height();
      EXPECT_EQ(top_container_bottom, bookmark_bar_bounds.y());
      EXPECT_EQ(bookmark_bar_bounds.bottom(), contents_container_bounds.y());
    } else {
      EXPECT_EQ(top_container_bottom, contents_container_bounds.y());
    }

    if (shown_state == TopChromeShownState::kFullyHidden) {
      // Top container is shifted up.
      EXPECT_EQ(top_container_bounds.y(), -top_controls_height);

      // Contents should occupy the entire height of the browser view, minus
      // the height of a detached bookmark bar if any.
      EXPECT_EQ(browser_view->height() - detached_bookmark_bar_height,
                browser_view->contents_container()->height());

      // Widget should not allow things to show outside its bounds.
      EXPECT_TRUE(browser_view->frame()->GetLayer()->GetMasksToBounds());
    } else {
      // Top container start at the top.
      EXPECT_EQ(top_container_bounds.y(), 0);

      // Contents should occupy the remainder of the browser view after the top
      // container and the detached bookmark bar if any.
      EXPECT_EQ(browser_view->height() - top_controls_height -
                    detached_bookmark_bar_height,
                browser_view->contents_container()->height());

      EXPECT_FALSE(browser_view->frame()->GetLayer()->GetMasksToBounds());
    }
  }

 private:
  void CompareTranslations(const gfx::Transform& t1,
                           const gfx::Transform& t2) const {
    const gfx::Vector2dF t1_translation = t1.To2dTranslation();
    const gfx::Vector2dF t2_translation = t2.To2dTranslation();
    EXPECT_FLOAT_EQ(t1_translation.x(), t2_translation.x());
    EXPECT_FLOAT_EQ(t1_translation.y(), t2_translation.y());
  }

  base::test::ScopedFeatureList scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(TopControlsSlideControllerTest);
};

IN_PROC_BROWSER_TEST_F(TopControlsSlideControllerTest, DisabledForHostedApps) {
  browser()->window()->Close();

  // Open a new app window.
  Browser::CreateParams params = Browser::CreateParams::CreateForApp(
      "test_browser_app", true /* trusted_source */, gfx::Rect(),
      browser()->profile(), true);
  params.initial_show_state = ui::SHOW_STATE_DEFAULT;
  Browser* browser = new Browser(params);
  AddBlankTabAndShow(browser);

  ASSERT_TRUE(browser->is_app());

  // No slide controller gets created for hosted apps.
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
  EXPECT_FALSE(browser_view->top_controls_slide_controller());

  // Renderer will get a zero-height top controls.
  EXPECT_EQ(browser_view->GetTopControlsHeight(), 0);
}

IN_PROC_BROWSER_TEST_F(TopControlsSlideControllerTest,
                       EnabledOnlyForTabletNonImmersiveModes) {
  EXPECT_FALSE(GetTabletModeEnabled());
  AddBlankTabAndShow(browser());
  // For a normal browser, the controller is created.
  ASSERT_TRUE(top_controls_slide_controller());
  // But the behavior is disabled since we didn't go to tablet mode yet.
  EXPECT_FALSE(top_controls_slide_controller()->IsEnabled());
  EXPECT_FLOAT_EQ(top_controls_slide_controller()->GetShownRatio(), 1.f);
  // The browser reports a zero height for top-chrome UIs when the behavior is
  // disabled, so the render doesn't think it needs to move the top controls.
  EXPECT_EQ(browser_view()->GetTopControlsHeight(), 0);

  // Now enable tablet mode.
  ToggleTabletMode();
  ASSERT_TRUE(GetTabletModeEnabled());

  // The behavior is enabled, but the shown ratio remains at 1.f since no page
  // scrolls happened yet.
  EXPECT_TRUE(top_controls_slide_controller()->IsEnabled());
  EXPECT_FLOAT_EQ(top_controls_slide_controller()->GetShownRatio(), 1.f);
  EXPECT_NE(browser_view()->GetTopControlsHeight(), 0);

  // Immersive fullscreen mode disables the behavior.
  chrome::ToggleFullscreenMode(browser());
  EXPECT_TRUE(browser_view()->IsFullscreen());
  EXPECT_FALSE(top_controls_slide_controller()->IsEnabled());
  EXPECT_EQ(browser_view()->GetTopControlsHeight(), 0);

  // Exit immersive mode.
  chrome::ToggleFullscreenMode(browser());
  EXPECT_FALSE(browser_view()->IsFullscreen());
  EXPECT_TRUE(top_controls_slide_controller()->IsEnabled());
  EXPECT_NE(browser_view()->GetTopControlsHeight(), 0);

  ToggleTabletMode();
  EXPECT_FALSE(GetTabletModeEnabled());
  EXPECT_FALSE(top_controls_slide_controller()->IsEnabled());
  EXPECT_EQ(browser_view()->GetTopControlsHeight(), 0);
}

IN_PROC_BROWSER_TEST_F(TopControlsSlideControllerTest, TestScrollingPage) {
  ToggleTabletMode();
  ASSERT_TRUE(GetTabletModeEnabled());
  EXPECT_TRUE(top_controls_slide_controller()->IsEnabled());
  EXPECT_FLOAT_EQ(top_controls_slide_controller()->GetShownRatio(), 1.f);

  // Navigate to our test page that has a long vertical content which we can use
  // to test page scrolling.
  OpenUrlAtIndex(embedded_test_server()->GetURL("/top_controls_scroll.html"),
                 0);

  aura::Window* browser_window = browser()->window()->GetNativeWindow();
  ui::test::EventGenerator event_generator(browser_window->GetRootWindow(),
                                           browser_window);

  // The above EventGenerator ctor initializes current_location() to the center
  // point of |browser_window|. Let's start a fast gesture scroll from that
  // point towards another point above it.
  const gfx::Point start_point = event_generator.current_location();
  const gfx::Point end_point = start_point + gfx::Vector2d(0, -100);
  GenerateGestureFlingScrollSequence(&event_generator, start_point, end_point);
  TopControlsShownRatioWaiter waiter(top_controls_slide_controller());
  waiter.WaitForRatio(0.f);
  EXPECT_FLOAT_EQ(top_controls_slide_controller()->GetShownRatio(), 0);
  CheckBrowserLayout(browser_view(), TopChromeShownState::kFullyHidden);

  // Perform another gesture scroll in the opposite direction and expect top-
  // chrome to be fully shown.
  GenerateGestureFlingScrollSequence(&event_generator, end_point, start_point);
  waiter.WaitForRatio(1.f);
  EXPECT_FLOAT_EQ(top_controls_slide_controller()->GetShownRatio(), 1.f);
  CheckBrowserLayout(browser_view(), TopChromeShownState::kFullyShown);
}

IN_PROC_BROWSER_TEST_F(TopControlsSlideControllerTest,
                       TestScrollingPageAndSwitchingToNTP) {
  ToggleTabletMode();
  ASSERT_TRUE(GetTabletModeEnabled());
  EXPECT_TRUE(top_controls_slide_controller()->IsEnabled());
  EXPECT_FLOAT_EQ(top_controls_slide_controller()->GetShownRatio(), 1.f);

  // Add a tab containing a local NTP page. NTP pages are not permitted to hide
  // top-chrome with scrolling.
  ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUINewTabURL));
  ASSERT_EQ(browser()->tab_strip_model()->count(), 1);

  // Navigate to our test page that has a long vertical content which we can use
  // to test page scrolling.
  OpenUrlAtIndex(embedded_test_server()->GetURL("/top_controls_scroll.html"),
                 0);
  ASSERT_EQ(browser()->tab_strip_model()->count(), 2);

  auto* browser_window = browser()->window()->GetNativeWindow();
  ui::test::EventGenerator event_generator(browser_window->GetRootWindow(),
                                           browser_window);

  // Scroll the `top_controls_scroll.html` page such that top-chrome is now
  // fully hidden.
  const gfx::Point start_point = event_generator.current_location();
  const gfx::Point end_point = start_point + gfx::Vector2d(0, -100);
  GenerateGestureFlingScrollSequence(&event_generator, start_point, end_point);
  TopControlsShownRatioWaiter waiter(top_controls_slide_controller());
  waiter.WaitForRatio(0.f);
  EXPECT_FLOAT_EQ(top_controls_slide_controller()->GetShownRatio(), 0);
  CheckBrowserLayout(browser_view(), TopChromeShownState::kFullyHidden);

  // Simulate (Ctrl + Tab) shortcut to select the next tab. Top-chrome should
  // show automatically.
  browser()->tab_strip_model()->SelectNextTab();
  EXPECT_EQ(browser()->tab_strip_model()->active_index(), 1);
  waiter.WaitForRatio(1.f);
  EXPECT_FLOAT_EQ(top_controls_slide_controller()->GetShownRatio(), 1.f);
  CheckBrowserLayout(browser_view(), TopChromeShownState::kFullyShown);

  // Since this is the NTP page, gesture scrolling will not hide top-chrome. It
  // will remain fully shown.
  GenerateGestureFlingScrollSequence(&event_generator, start_point, end_point);
  waiter.WaitForRatio(1.f);
  EXPECT_FLOAT_EQ(top_controls_slide_controller()->GetShownRatio(), 1.f);
  CheckBrowserLayout(browser_view(), TopChromeShownState::kFullyShown);

  // Switch back to the scrollable page, it should be possible now to hide top-
  // chrome.
  browser()->tab_strip_model()->SelectNextTab();
  EXPECT_EQ(browser()->tab_strip_model()->active_index(), 0);
  waiter.WaitForRatio(1.f);
  EXPECT_FLOAT_EQ(top_controls_slide_controller()->GetShownRatio(), 1.f);
  GenerateGestureFlingScrollSequence(&event_generator, start_point, end_point);
  waiter.WaitForRatio(0.f);
  EXPECT_FLOAT_EQ(top_controls_slide_controller()->GetShownRatio(), 0);
  CheckBrowserLayout(browser_view(), TopChromeShownState::kFullyHidden);
}

IN_PROC_BROWSER_TEST_F(TopControlsSlideControllerTest, TestClosingATab) {
  ToggleTabletMode();
  ASSERT_TRUE(GetTabletModeEnabled());
  EXPECT_TRUE(top_controls_slide_controller()->IsEnabled());
  EXPECT_FLOAT_EQ(top_controls_slide_controller()->GetShownRatio(), 1.f);

  // Navigate to our test scrollable page.
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/top_controls_scroll.html"));
  TabNonEmptyPaintWaiter paint_waiter(
      browser()->tab_strip_model()->GetActiveWebContents());
  paint_waiter.Wait();
  ASSERT_EQ(browser()->tab_strip_model()->count(), 1);

  // Scroll to fully hide top-chrome.
  auto* browser_window = browser()->window()->GetNativeWindow();
  ui::test::EventGenerator event_generator(browser_window->GetRootWindow(),
                                           browser_window);
  const gfx::Point start_point = event_generator.current_location();
  const gfx::Point end_point = start_point + gfx::Vector2d(0, -100);
  GenerateGestureFlingScrollSequence(&event_generator, start_point, end_point);
  TopControlsShownRatioWaiter waiter(top_controls_slide_controller());
  waiter.WaitForRatio(0.f);
  EXPECT_FLOAT_EQ(top_controls_slide_controller()->GetShownRatio(), 0);
  CheckBrowserLayout(browser_view(), TopChromeShownState::kFullyHidden);

  // Simulate (Ctrl + T) by inserting a new tab. Expect top-chrome to be fully
  // shown.
  chrome::NewTab(browser());
  waiter.WaitForRatio(1.f);
  EXPECT_EQ(browser()->tab_strip_model()->active_index(), 1);
  EXPECT_EQ(browser()->tab_strip_model()->count(), 2);
  EXPECT_FLOAT_EQ(top_controls_slide_controller()->GetShownRatio(), 1.f);
  CheckBrowserLayout(browser_view(), TopChromeShownState::kFullyShown);

  // Now simulate (Ctrl + W) by closing this newly created tab, expect to return
  // to the scrollable page tab, which used to have a top-chrome shown ratio of
  // 0. We should expect that it will animate back to a shown ratio of 1.
  // This test is needed to make sure that the native view of the web contents
  // of the newly selected tab after the current one is closed will attach to
  // the browser's native view host *before* we attempt to make any changes to
  // its top-chrome shown ratio.
  chrome::CloseTab(browser());
  waiter.WaitForRatio(1.f);
  EXPECT_EQ(browser()->tab_strip_model()->active_index(), 0);
  EXPECT_EQ(browser()->tab_strip_model()->count(), 1);
  EXPECT_FLOAT_EQ(top_controls_slide_controller()->GetShownRatio(), 1.f);
  CheckBrowserLayout(browser_view(), TopChromeShownState::kFullyShown);

  // It is still possible to slide top-chrome up.
  GenerateGestureFlingScrollSequence(&event_generator, start_point, end_point);
  waiter.WaitForRatio(0.f);
  EXPECT_FLOAT_EQ(top_controls_slide_controller()->GetShownRatio(), 0);
  CheckBrowserLayout(browser_view(), TopChromeShownState::kFullyHidden);
}

IN_PROC_BROWSER_TEST_F(TopControlsSlideControllerTest,
                       TestFocusEditableElements) {
  ToggleTabletMode();
  ASSERT_TRUE(GetTabletModeEnabled());
  EXPECT_TRUE(top_controls_slide_controller()->IsEnabled());
  EXPECT_FLOAT_EQ(top_controls_slide_controller()->GetShownRatio(), 1.f);

  // Navigate to our test page that has a long vertical content which we can use
  // to test page scrolling.
  OpenUrlAtIndex(embedded_test_server()->GetURL("/top_controls_scroll.html"),
                 0);

  auto* browser_window = browser()->window()->GetNativeWindow();
  ui::test::EventGenerator event_generator(browser_window->GetRootWindow(),
                                           browser_window);

  // Scroll the `top_controls_scroll.html` page such that top-chrome is now
  // fully hidden.
  const gfx::Point start_point = event_generator.current_location();
  const gfx::Point end_point = start_point + gfx::Vector2d(0, -100);
  GenerateGestureFlingScrollSequence(&event_generator, start_point, end_point);
  TopControlsShownRatioWaiter waiter(top_controls_slide_controller());
  waiter.WaitForRatio(0.f);
  EXPECT_FLOAT_EQ(top_controls_slide_controller()->GetShownRatio(), 0);
  CheckBrowserLayout(browser_view(), TopChromeShownState::kFullyHidden);

  // Define an internal lambda that returns the javascript function body that
  // can be executed on the focus on `top_controls_scroll.html` page to focus on
  // an editable text input field in the page, or blur its focus depending on
  // the |should_focus| parameter.
  auto get_js_function_body = [](bool should_focus) -> std::string {
    constexpr size_t kBufferSize = 1024;
    char buffer[kBufferSize];
    base::strings::SafeSPrintf(
        buffer,
        "domAutomationController.send("
        "    ((function() {"
        "        var editableElement ="
        "            document.getElementById('editable-element');"
        "        if (editableElement) {"
        "          editableElement.%s();"
        "          return true;"
        "        }"
        "        return false;"
        "      })()));",
        should_focus ? "focus" : "blur");
    return std::string(buffer);
  };

  // Focus on the editable element in the page and expect that top-chrome will
  // be shown.
  content::WebContents* contents = browser_view()->GetActiveWebContents();
  bool bool_result = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      contents, get_js_function_body(true /* should_focus */), &bool_result));
  EXPECT_TRUE(bool_result);
  waiter.WaitForRatio(1.f);
  EXPECT_FLOAT_EQ(top_controls_slide_controller()->GetShownRatio(), 1.f);
  CheckBrowserLayout(browser_view(), TopChromeShownState::kFullyShown);

  // Now try scrolling in a way that would normally hide top-chrome, and expect
  // that top-chrome will be forced shown as long as the editable element is
  // focused.
  GenerateGestureFlingScrollSequence(&event_generator, start_point, end_point);
  waiter.WaitForRatio(1.f);
  EXPECT_FLOAT_EQ(top_controls_slide_controller()->GetShownRatio(), 1.f);
  CheckBrowserLayout(browser_view(), TopChromeShownState::kFullyShown);

  // Now blur the focused editable element. Expect that top-chrome can now be
  // hidden with gesture scrolls.
  bool_result = false;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      contents, get_js_function_body(false /* should_focus */), &bool_result));
  EXPECT_TRUE(bool_result);
  GenerateGestureFlingScrollSequence(&event_generator, start_point, end_point);
  waiter.WaitForRatio(0.f);
  EXPECT_FLOAT_EQ(top_controls_slide_controller()->GetShownRatio(), 0);
  CheckBrowserLayout(browser_view(), TopChromeShownState::kFullyHidden);
}

// Used to wait for the browser view to change its bounds as a result of display
// rotation.
class BrowserViewLayoutWaiter : public views::ViewObserver {
 public:
  explicit BrowserViewLayoutWaiter(BrowserView* browser_view)
      : browser_view_(browser_view) {
    browser_view->AddObserver(this);
  }
  ~BrowserViewLayoutWaiter() override { browser_view_->RemoveObserver(this); }

  void Wait() {
    if (view_bounds_changed_) {
      view_bounds_changed_ = false;
      return;
    }

    run_loop_ = std::make_unique<base::RunLoop>();
    run_loop_->Run();
  }

  // views::ViewObserver:
  void OnViewBoundsChanged(views::View* observed_view) override {
    view_bounds_changed_ = true;
    if (run_loop_)
      run_loop_->Quit();
  }

 private:
  BrowserView* browser_view_;

  bool view_bounds_changed_ = false;

  std::unique_ptr<base::RunLoop> run_loop_;

  DISALLOW_COPY_AND_ASSIGN(BrowserViewLayoutWaiter);
};

IN_PROC_BROWSER_TEST_F(TopControlsSlideControllerTest, DisplayRotation) {
  ToggleTabletMode();
  ASSERT_TRUE(GetTabletModeEnabled());
  EXPECT_TRUE(top_controls_slide_controller()->IsEnabled());
  EXPECT_FLOAT_EQ(top_controls_slide_controller()->GetShownRatio(), 1.f);

  // Maximizing the browser window makes the browser view layout more
  // predictable with display rotation, as it's just resized to match the
  // display bounds.
  browser_view()->frame()->Maximize();

  // Navigate to our scrollable test page, scroll with touch gestures so that
  // top-chrome is fully hidden.
  OpenUrlAtIndex(embedded_test_server()->GetURL("/top_controls_scroll.html"),
                 0);
  aura::Window* browser_window = browser()->window()->GetNativeWindow();
  ui::test::EventGenerator event_generator(browser_window->GetRootWindow(),
                                           browser_window);
  gfx::Point start_point = event_generator.current_location();
  gfx::Point end_point = start_point + gfx::Vector2d(0, -100);
  GenerateGestureFlingScrollSequence(&event_generator, start_point, end_point);
  TopControlsShownRatioWaiter waiter(top_controls_slide_controller());
  waiter.WaitForRatio(0.f);
  EXPECT_FLOAT_EQ(top_controls_slide_controller()->GetShownRatio(), 0);
  CheckBrowserLayout(browser_view(), TopChromeShownState::kFullyHidden);

  // Try all possible rotations. Changing display rotation should *not* unhide
  // top chrome.
  const std::vector<display::Display::Rotation> rotations_to_try = {
      display::Display::ROTATE_90, display::Display::ROTATE_180,
      display::Display::ROTATE_270, display::Display::ROTATE_0,
  };

  ash::mojom::CrosDisplayConfigControllerPtr cros_display_config;
  content::ServiceManagerConnection::GetForProcess()
      ->GetConnector()
      ->BindInterface(ash::mojom::kServiceName, &cros_display_config);
  ash::mojom::CrosDisplayConfigControllerAsyncWaiter waiter_for(
      cros_display_config.get());
  std::vector<ash::mojom::DisplayUnitInfoPtr> info_list;
  waiter_for.GetDisplayUnitInfoList(false /* single_unified */, &info_list);
  for (const ash::mojom::DisplayUnitInfoPtr& display_unit_info : info_list) {
    const std::string display_id = display_unit_info->id;
    for (const auto& rotation : rotations_to_try) {
      BrowserViewLayoutWaiter browser_view_layout_waiter(browser_view());
      auto config_properties = ash::mojom::DisplayConfigProperties::New();
      config_properties->rotation = ash::mojom::DisplayRotation::New(rotation);
      ash::mojom::DisplayConfigResult result;
      waiter_for.SetDisplayProperties(display_id, std::move(config_properties),
                                      &result);
      EXPECT_EQ(result, ash::mojom::DisplayConfigResult::kSuccess);

      // Wait for the browser view to change its bounds as a result of display
      // rotation.
      browser_view_layout_waiter.Wait();

      // Make sure top-chrome is still hidden.
      waiter.WaitForRatio(0.f);
      EXPECT_FLOAT_EQ(top_controls_slide_controller()->GetShownRatio(), 0);
      CheckBrowserLayout(browser_view(), TopChromeShownState::kFullyHidden);

      // Now perform gesture scrolls to show top-chrome, make sure everything
      // looks good in this rotation.
      // Update the start and end points after rotation.
      start_point = browser_window->GetBoundsInRootWindow().CenterPoint();
      end_point = start_point + gfx::Vector2d(0, -100);

      // Make sure to send them in screen coordinates to make sure rotation
      // is taken into consideration.
      browser_window->GetRootWindow()->GetHost()->ConvertDIPToScreenInPixels(
          &start_point);
      browser_window->GetRootWindow()->GetHost()->ConvertDIPToScreenInPixels(
          &end_point);
      GenerateGestureFlingScrollSequence(&event_generator, end_point,
                                         start_point);
      waiter.WaitForRatio(1.f);
      EXPECT_FLOAT_EQ(top_controls_slide_controller()->GetShownRatio(), 1.f);
      CheckBrowserLayout(browser_view(), TopChromeShownState::kFullyShown);

      // Scroll again to hide top-chrome in preparation for the next rotation
      // iteration.
      GenerateGestureFlingScrollSequence(&event_generator, start_point,
                                         end_point);
      waiter.WaitForRatio(0.f);
      EXPECT_FLOAT_EQ(top_controls_slide_controller()->GetShownRatio(), 0);
      CheckBrowserLayout(browser_view(), TopChromeShownState::kFullyHidden);
    }
  }
}

}  // namespace
