// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/browser_non_client_frame_view_ash.h"

#include <string>

#include "ash/ash_constants.h"
#include "ash/frame/caption_buttons/frame_caption_button.h"
#include "ash/frame/caption_buttons/frame_caption_button_container_view.h"
#include "ash/frame/default_frame_header.h"
#include "ash/frame/frame_header.h"
#include "ash/public/cpp/ash_switches.h"
#include "ash/public/cpp/immersive/immersive_fullscreen_controller_test_api.h"
#include "ash/public/cpp/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/wm/overview/window_selector_controller.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/scoped_observer.h"
#include "base/test/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/command_updater.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_util.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_window_manager_test.h"
#include "chrome/browser/ui/ash/tablet_mode_client.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/exclusive_access/fullscreen_controller.h"
#include "chrome/browser/ui/exclusive_access/fullscreen_controller_test.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/toolbar/browser_actions_bar_browsertest.h"
#include "chrome/browser/ui/views/bookmarks/bookmark_bar_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/hosted_app_button_container.h"
#include "chrome/browser/ui/views/frame/immersive_mode_controller.h"
#include "chrome/browser/ui/views/frame/immersive_mode_controller_ash.h"
#include "chrome/browser/ui/views/profiles/profile_indicator_icon.h"
#include "chrome/browser/ui/views/tabs/tab.h"
#include "chrome/browser/ui/views/toolbar/app_menu.h"
#include "chrome/browser/ui/views/toolbar/extension_toolbar_menu_view.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/web_application_info.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/signin/core/account_id/account_id.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "services/ui/public/interfaces/window_manager_constants.mojom.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/test/env_test_helper.h"
#include "ui/base/class_property.h"
#include "ui/base/hit_test.h"
#include "ui/base/ui_base_switches.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event.h"
#include "ui/gfx/color_palette.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/window_util.h"

namespace {

// Toggles fullscreen mode and waits for the notification.
void ToggleFullscreenModeAndWait(Browser* browser) {
  FullscreenNotificationObserver waiter;
  chrome::ToggleFullscreenMode(browser);
  waiter.Wait();
}

// Enters fullscreen mode for tab and waits for the notification.
void EnterFullscreenModeForTabAndWait(Browser* browser,
                                      content::WebContents* web_contents) {
  FullscreenNotificationObserver waiter;
  browser->exclusive_access_manager()
      ->fullscreen_controller()
      ->EnterFullscreenModeForTab(web_contents, GURL());
  waiter.Wait();
}

// Exits fullscreen mode for tab and waits for the notification.
void ExitFullscreenModeForTabAndWait(Browser* browser,
                                     content::WebContents* web_contents) {
  FullscreenNotificationObserver waiter;
  browser->exclusive_access_manager()
      ->fullscreen_controller()
      ->ExitFullscreenModeForTab(web_contents);
  waiter.Wait();
}

// Exits fullscreen mode and waits for the notification.
void ExitFullscreenModeAndWait(BrowserView* browser_view) {
  FullscreenNotificationObserver waiter;
  browser_view->ExitFullscreen();
  waiter.Wait();
}

// Generates the test names suffixes based on the value of the test param.
std::string TouchOptimizedUiStatusToString(
    const ::testing::TestParamInfo<bool>& info) {
  return info.param ? "TouchOptimizedUiEnabled" : "TouchOptimizedUiDisabled";
}

// Template to be used as a base class for touch-optimized UI parameterized test
// fixtures.
template <class BaseTest>
class TouchOptimizedUiParamTest : public BaseTest,
                                  public ::testing::WithParamInterface<bool> {
 public:
  TouchOptimizedUiParamTest() = default;
  ~TouchOptimizedUiParamTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    if (GetParam()) {
      command_line->AppendSwitchASCII(
          switches::kTopChromeMD, switches::kTopChromeMDMaterialTouchOptimized);
    }

    BaseTest::SetUpCommandLine(command_line);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TouchOptimizedUiParamTest);
};

}  // namespace

using views::Widget;

using BrowserNonClientFrameViewAshTest =
    TouchOptimizedUiParamTest<InProcessBrowserTest>;
using HostedAppNonClientFrameViewAshTest =
    TouchOptimizedUiParamTest<BrowserActionsBarBrowserTest>;

IN_PROC_BROWSER_TEST_P(BrowserNonClientFrameViewAshTest, NonClientHitTest) {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  Widget* widget = browser_view->GetWidget();
  // We know we're using Ash, so static cast.
  BrowserNonClientFrameViewAsh* frame_view =
      static_cast<BrowserNonClientFrameViewAsh*>(
          widget->non_client_view()->frame_view());

  // Click on the top edge of a restored window hits the top edge resize handle.
  const int kWindowWidth = 300;
  const int kWindowHeight = 290;
  widget->SetBounds(gfx::Rect(10, 10, kWindowWidth, kWindowHeight));
  gfx::Point top_edge(kWindowWidth / 2, 0);
  EXPECT_EQ(HTTOP, frame_view->NonClientHitTest(top_edge));

  // Click just below the resize handle hits the caption.
  gfx::Point below_resize(kWindowWidth / 2, ash::kResizeInsideBoundsSize);
  EXPECT_EQ(HTCAPTION, frame_view->NonClientHitTest(below_resize));

  // Click in the top edge of a maximized window now hits the client area,
  // because we want it to fall through to the tab strip and select a tab.
  widget->Maximize();
  int expected_value = HTCLIENT;
  EXPECT_EQ(expected_value, frame_view->NonClientHitTest(top_edge));
}

// Test that the frame view does not do any painting in non-immersive
// fullscreen.
IN_PROC_BROWSER_TEST_P(BrowserNonClientFrameViewAshTest,
                       NonImmersiveFullscreen) {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  content::WebContents* web_contents = browser_view->GetActiveWebContents();
  Widget* widget = browser_view->GetWidget();
  // We know we're using Ash, so static cast.
  BrowserNonClientFrameViewAsh* frame_view =
      static_cast<BrowserNonClientFrameViewAsh*>(
          widget->non_client_view()->frame_view());

  // Frame paints by default.
  EXPECT_TRUE(frame_view->ShouldPaint());

  // No painting should occur in non-immersive fullscreen. (We enter into tab
  // fullscreen here because tab fullscreen is non-immersive even on ChromeOS).
  {
    // NOTIFICATION_FULLSCREEN_CHANGED is sent asynchronously.
    std::unique_ptr<FullscreenNotificationObserver> waiter(
        new FullscreenNotificationObserver());
    browser()
        ->exclusive_access_manager()
        ->fullscreen_controller()
        ->EnterFullscreenModeForTab(web_contents, GURL());
    waiter->Wait();
  }
  EXPECT_FALSE(browser_view->immersive_mode_controller()->IsEnabled());
  EXPECT_FALSE(frame_view->ShouldPaint());

  // The client view abuts top of the window.
  EXPECT_EQ(0, frame_view->GetBoundsForClientView().y());

  // The frame should be painted again when fullscreen is exited and the caption
  // buttons should be visible.
  ToggleFullscreenModeAndWait(browser());
  EXPECT_TRUE(frame_view->ShouldPaint());
  EXPECT_TRUE(frame_view->caption_button_container_->visible());
}

IN_PROC_BROWSER_TEST_P(BrowserNonClientFrameViewAshTest, ImmersiveFullscreen) {
  aura::test::EnvTestHelper().SetAlwaysUseLastMouseLocation(true);
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  content::WebContents* web_contents = browser_view->GetActiveWebContents();
  Widget* widget = browser_view->GetWidget();
  // We know we're using Ash, so static cast.
  BrowserNonClientFrameViewAsh* frame_view =
      static_cast<BrowserNonClientFrameViewAsh*>(
          widget->non_client_view()->frame_view());

  ImmersiveModeController* immersive_mode_controller =
      browser_view->immersive_mode_controller();
  ASSERT_EQ(ImmersiveModeController::Type::ASH,
            immersive_mode_controller->type());

  ash::ImmersiveFullscreenControllerTestApi(
      static_cast<ImmersiveModeControllerAsh*>(immersive_mode_controller)
          ->controller())
      .SetupForTest();

  // Immersive fullscreen starts disabled.
  ASSERT_FALSE(widget->IsFullscreen());
  EXPECT_FALSE(immersive_mode_controller->IsEnabled());

  // Frame paints by default.
  EXPECT_TRUE(frame_view->ShouldPaint());
  EXPECT_LT(0, frame_view->frame_header_->GetHeaderHeightForPainting());

  // Enter both browser fullscreen and tab fullscreen. Entering browser
  // fullscreen should enable immersive fullscreen.
  ToggleFullscreenModeAndWait(browser());
  EnterFullscreenModeForTabAndWait(browser(), web_contents);
  EXPECT_TRUE(immersive_mode_controller->IsEnabled());

  // An immersive reveal shows the buttons and the top of the frame.
  std::unique_ptr<ImmersiveRevealedLock> revealed_lock(
      immersive_mode_controller->GetRevealedLock(
          ImmersiveModeController::ANIMATE_REVEAL_NO));
  EXPECT_TRUE(immersive_mode_controller->IsRevealed());
  EXPECT_TRUE(frame_view->ShouldPaint());
  EXPECT_TRUE(frame_view->caption_button_container_->visible());

  // End the reveal. When in both immersive browser fullscreen and tab
  // fullscreen.
  revealed_lock.reset();
  EXPECT_FALSE(immersive_mode_controller->IsRevealed());
  EXPECT_FALSE(frame_view->ShouldPaint());
  EXPECT_EQ(0, frame_view->frame_header_->GetHeaderHeightForPainting());

  // Repeat test but without tab fullscreen.
  ExitFullscreenModeForTabAndWait(browser(), web_contents);

  // Immersive reveal should have same behavior as before.
  revealed_lock.reset(immersive_mode_controller->GetRevealedLock(
      ImmersiveModeController::ANIMATE_REVEAL_NO));
  EXPECT_TRUE(immersive_mode_controller->IsRevealed());
  EXPECT_TRUE(frame_view->ShouldPaint());
  EXPECT_TRUE(frame_view->caption_button_container_->visible());
  EXPECT_LT(0, frame_view->frame_header_->GetHeaderHeightForPainting());

  // Ending the reveal. Immersive browser should have the same behavior as full
  // screen, i.e., having an origin of (0,0).
  revealed_lock.reset();
  EXPECT_FALSE(frame_view->ShouldPaint());
  EXPECT_EQ(0, frame_view->frame_header_->GetHeaderHeightForPainting());

  // Exiting immersive fullscreen should make the caption buttons and the frame
  // visible again.
  ExitFullscreenModeAndWait(browser_view);
  EXPECT_FALSE(immersive_mode_controller->IsEnabled());
  EXPECT_TRUE(frame_view->ShouldPaint());
  EXPECT_TRUE(frame_view->caption_button_container_->visible());
  EXPECT_LT(0, frame_view->frame_header_->GetHeaderHeightForPainting());
}

// Tests that Avatar icon should show on the top left corner of the teleported
// browser window on ChromeOS.
IN_PROC_BROWSER_TEST_P(BrowserNonClientFrameViewAshTest,
                       AvatarDisplayOnTeleportedWindow) {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  Widget* widget = browser_view->GetWidget();
  // We know we're using Ash, so static cast.
  BrowserNonClientFrameViewAsh* frame_view =
      static_cast<BrowserNonClientFrameViewAsh*>(
          widget->non_client_view()->frame_view());
  aura::Window* window = browser()->window()->GetNativeWindow();

  EXPECT_FALSE(MultiUserWindowManager::ShouldShowAvatar(window));
  EXPECT_FALSE(frame_view->profile_indicator_icon());

  const AccountId current_account_id =
      multi_user_util::GetAccountIdFromProfile(browser()->profile());
  TestMultiUserWindowManager* manager =
      new TestMultiUserWindowManager(browser(), current_account_id);

  // Teleport the window to another desktop.
  const AccountId account_id2(AccountId::FromUserEmail("user2"));
  manager->ShowWindowForUser(window, account_id2);
  EXPECT_TRUE(MultiUserWindowManager::ShouldShowAvatar(window));

  // An icon should show on the top left corner of the teleported browser
  // window.
  EXPECT_TRUE(frame_view->profile_indicator_icon());
}

// Hit Test for Avatar Menu Button on ChromeOS.
IN_PROC_BROWSER_TEST_P(BrowserNonClientFrameViewAshTest,
                       AvatarMenuButtonHitTestOnChromeOS) {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  Widget* widget = browser_view->GetWidget();
  // We know we're using Ash, so static cast.
  BrowserNonClientFrameViewAsh* frame_view =
      static_cast<BrowserNonClientFrameViewAsh*>(
          widget->non_client_view()->frame_view());

  gfx::Point avatar_center(profiles::kAvatarIconWidth / 2,
                           profiles::kAvatarIconHeight / 2);
  // The increased header height in the touch-optimized UI affects the expected
  // result.
  int expected_value = GetParam() ? HTCAPTION : HTCLIENT;
  EXPECT_EQ(expected_value, frame_view->NonClientHitTest(avatar_center));
  EXPECT_FALSE(frame_view->profile_indicator_icon());

  const AccountId current_user =
      multi_user_util::GetAccountIdFromProfile(browser()->profile());
  TestMultiUserWindowManager* manager =
      new TestMultiUserWindowManager(browser(), current_user);

  // Teleport the window to another desktop.
  const AccountId account_id2(AccountId::FromUserEmail("user2"));
  manager->ShowWindowForUser(browser()->window()->GetNativeWindow(),
                             account_id2);
  // Clicking on the avatar icon should have same behaviour like clicking on
  // the caption area, i.e., allow the user to drag the browser window around.
  EXPECT_EQ(HTCAPTION, frame_view->NonClientHitTest(avatar_center));
  EXPECT_TRUE(frame_view->profile_indicator_icon());
}

// Tests that for an incognito browser, there is an avatar icon view, unless in
// touch-optimized mode.
IN_PROC_BROWSER_TEST_P(BrowserNonClientFrameViewAshTest, IncognitoAvatar) {
  Browser* incognito_browser = CreateIncognitoBrowser();
  BrowserView* browser_view =
      BrowserView::GetBrowserViewForBrowser(incognito_browser);
  Widget* widget = browser_view->GetWidget();
  // We know we're using Ash, so static cast.
  BrowserNonClientFrameViewAsh* frame_view =
      static_cast<BrowserNonClientFrameViewAsh*>(
          widget->non_client_view()->frame_view());

  const bool should_have_avatar = !GetParam();
  const bool has_avatar = !!frame_view->profile_indicator_icon();
  EXPECT_EQ(should_have_avatar, has_avatar);
}

// Tests that FrameCaptionButtonContainer has been relaid out in response to
// tablet mode being toggled.
IN_PROC_BROWSER_TEST_P(BrowserNonClientFrameViewAshTest,
                       ToggleTabletModeRelayout) {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  Widget* widget = browser_view->GetWidget();
  // We know we're using Ash, so static cast.
  BrowserNonClientFrameViewAsh* frame_view =
      static_cast<BrowserNonClientFrameViewAsh*>(
          widget->non_client_view()->frame_view());

  const gfx::Rect initial = frame_view->caption_button_container_->bounds();
  ash::TabletModeController* tablet_mode_controller =
      ash::Shell::Get()->tablet_mode_controller();
  tablet_mode_controller->EnableTabletModeWindowManager(true);
  tablet_mode_controller->FlushForTesting();
  ash::FrameCaptionButtonContainerView::TestApi test(frame_view->
                                                     caption_button_container_);
  test.EndAnimations();
  const gfx::Rect during_maximize = frame_view->caption_button_container_->
      bounds();
  EXPECT_GT(initial.width(), during_maximize.width());
  tablet_mode_controller->EnableTabletModeWindowManager(false);
  tablet_mode_controller->FlushForTesting();
  test.EndAnimations();
  const gfx::Rect after_restore = frame_view->caption_button_container_->
      bounds();
  EXPECT_EQ(initial, after_restore);
}

// Tests that browser frame minimum size constraint is updated in response to
// browser view layout.
IN_PROC_BROWSER_TEST_P(BrowserNonClientFrameViewAshTest,
                       FrameMinSizeIsUpdated) {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  Widget* widget = browser_view->GetWidget();
  // We know we're using Ash, so static cast.
  BrowserNonClientFrameViewAsh* frame_view =
      static_cast<BrowserNonClientFrameViewAsh*>(
          widget->non_client_view()->frame_view());

  BookmarkBarView* bookmark_bar = browser_view->GetBookmarkBarView();
  EXPECT_FALSE(bookmark_bar->visible());
  const int min_height_no_bookmarks = frame_view->GetMinimumSize().height();

  // Setting non-zero bookmark bar preferred size forces it to be visible and
  // triggers BrowserView layout update.
  bookmark_bar->SetPreferredSize(gfx::Size(50, 5));
  EXPECT_TRUE(bookmark_bar->visible());

  // Minimum window size should grow with the bookmark bar shown.
  // kMinimumSize window property should get updated.
  aura::Window* window = browser()->window()->GetNativeWindow();
  const gfx::Size* min_window_size =
      window->GetProperty(aura::client::kMinimumSize);
  ASSERT_NE(nullptr, min_window_size);
  EXPECT_GT(min_window_size->height(), min_height_no_bookmarks);
  EXPECT_EQ(*min_window_size, frame_view->GetMinimumSize());
}

// Tests that when browser frame is minimized, toggling tablet mode doesn't
// trigger caption button update (https://crbug.com/822890).
IN_PROC_BROWSER_TEST_P(BrowserNonClientFrameViewAshTest,
                       ToggleTabletModeOnMinimizedWindow) {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  Widget* widget = browser_view->GetWidget();
  // We know we're using Ash, so static cast.
  BrowserNonClientFrameViewAsh* frame_view =
      static_cast<BrowserNonClientFrameViewAsh*>(
          widget->non_client_view()->frame_view());
  ash::FrameCaptionButtonContainerView::TestApi test(
      frame_view->caption_button_container_);
  widget->Maximize();
  // Restore icon for size button in maximized window state.
  EXPECT_EQ(&ash::kWindowControlRestoreIcon,
            test.size_button()->icon_definition_for_test());
  widget->Minimize();
  // When entering tablet mode in minimized window state, size button should not
  // get updated.
  TabletModeClient::Get()->OnTabletModeToggled(true);
  EXPECT_EQ(&ash::kWindowControlRestoreIcon,
            test.size_button()->icon_definition_for_test());
  // When leaving tablet mode in minimized window state, size button should not
  // get updated.
  TabletModeClient::Get()->OnTabletModeToggled(false);
  EXPECT_EQ(&ash::kWindowControlRestoreIcon,
            test.size_button()->icon_definition_for_test());
  // When unminimizing in non-tablet mode, size button should match with
  // maximized window state, which is restore icon.
  ::wm::Unminimize(widget->GetNativeWindow());
  EXPECT_EQ(&ash::kWindowControlRestoreIcon,
            test.size_button()->icon_definition_for_test());
}

namespace {

class ImmersiveModeBrowserViewTest
    : public TouchOptimizedUiParamTest<InProcessBrowserTest>,
      public ImmersiveModeController::Observer {
 public:
  ImmersiveModeBrowserViewTest() = default;
  ~ImmersiveModeBrowserViewTest() override = default;

  BrowserView* browser_view() {
    return BrowserView::GetBrowserViewForBrowser(browser());
  }

  void PreRunTestOnMainThread() override {
    InProcessBrowserTest::PreRunTestOnMainThread();
    aura::test::EnvTestHelper().SetAlwaysUseLastMouseLocation(true);
    auto* immersive_mode_controller =
        browser_view()->immersive_mode_controller();
    scoped_observer_.Add(immersive_mode_controller);

    ash::ImmersiveFullscreenControllerTestApi(
        static_cast<ImmersiveModeControllerAsh*>(immersive_mode_controller)
            ->controller())
        .SetupForTest();
    BrowserView::SetDisableRevealerDelayForTesting(true);
  }

  void RunTest(int command, int expected_index) {
    reveal_started_ = reveal_ended_ = false;
    expected_index_ = expected_index;
    browser()->command_controller()->ExecuteCommand(command);
    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(reveal_ended_);
  }

  // ImmersiveModeController::Observer:
  void OnImmersiveRevealStarted() override {
    EXPECT_FALSE(reveal_started_);
    EXPECT_FALSE(reveal_ended_);
    reveal_started_ = true;
    EXPECT_TRUE(browser_view()->immersive_mode_controller()->IsRevealed());
  }

  void OnImmersiveRevealEnded() override {
    EXPECT_TRUE(reveal_started_);
    EXPECT_FALSE(reveal_ended_);
    reveal_started_ = false;
    reveal_ended_ = true;
    EXPECT_FALSE(browser_view()->immersive_mode_controller()->IsRevealed());
    EXPECT_EQ(expected_index_, browser()->tab_strip_model()->active_index());
  }

  void OnImmersiveModeControllerDestroyed() override {
    scoped_observer_.RemoveAll();
  }

 private:
  ScopedObserver<ImmersiveModeController, ImmersiveModeController::Observer>
      scoped_observer_{this};
  int expected_index_ = false;
  bool reveal_started_ = false;
  bool reveal_ended_ = false;

  DISALLOW_COPY_AND_ASSIGN(ImmersiveModeBrowserViewTest);
};

}  // namespace

// Tests IDC_SELECT_TAB_0, IDC_SELECT_NEXT_TAB, IDC_SELECT_PREVIOUS_TAB and
// IDC_SELECT_LAST_TAB when the browser is in immersive fullscreen mode.
IN_PROC_BROWSER_TEST_P(ImmersiveModeBrowserViewTest,
                       TabNavigationAcceleratorsFullscreenBrowser) {
  // Make sure that the focus is on the webcontents rather than on the omnibox,
  // because if the focus is on the omnibox, the tab strip will remain revealed
  // in the immerisve fullscreen mode and will interfere with this test waiting
  // for the revealer to be dismissed.
  browser()->tab_strip_model()->GetActiveWebContents()->Focus();

  // Create three more tabs plus the existing one that browser tests start with.
  const GURL about_blank(url::kAboutBlankURL);
  AddTabAtIndex(0, about_blank, ui::PAGE_TRANSITION_TYPED);
  browser()->tab_strip_model()->GetActiveWebContents()->Focus();
  AddTabAtIndex(0, about_blank, ui::PAGE_TRANSITION_TYPED);
  browser()->tab_strip_model()->GetActiveWebContents()->Focus();
  AddTabAtIndex(0, about_blank, ui::PAGE_TRANSITION_TYPED);
  browser()->tab_strip_model()->GetActiveWebContents()->Focus();

  // Toggle fullscreen mode.
  chrome::ToggleFullscreenMode(browser());
  EXPECT_TRUE(browser_view()->immersive_mode_controller()->IsEnabled());
  // Wait for the end of the initial reveal which results from adding the new
  // tabs and changing the focused tab.
  base::RunLoop().RunUntilIdle();

  // Groups the browser command ID and its corresponding active tab index that
  // will result when the command is executed in this test.
  struct TestData {
    int command;
    int expected_index;
  };
  constexpr TestData test_data[] = {{IDC_SELECT_LAST_TAB, 3},
                                    {IDC_SELECT_TAB_0, 0},
                                    {IDC_SELECT_NEXT_TAB, 1},
                                    {IDC_SELECT_PREVIOUS_TAB, 0}};
  for (const auto& datum : test_data)
    RunTest(datum.command, datum.expected_index);
}

// Creates a browser for a bookmark app and verifies the window frame is
// correct.
IN_PROC_BROWSER_TEST_P(HostedAppNonClientFrameViewAshTest, HostedAppFrame) {
  const GURL kAppStartURL("http://example.org/");

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kDesktopPWAWindowing);

  WebApplicationInfo web_app_info;
  web_app_info.app_url = kAppStartURL;
  web_app_info.theme_color = SK_ColorBLUE;

  const extensions::Extension* app = InstallBookmarkApp(web_app_info);
  Browser* app_browser = LaunchAppBrowser(app);
  NavigateParams params(app_browser, kAppStartURL, ui::PAGE_TRANSITION_LINK);
  ui_test_utils::NavigateToURL(&params);

  BrowserView* browser_view =
      BrowserView::GetBrowserViewForBrowser(app_browser);

  BrowserNonClientFrameViewAsh* frame_view =
      static_cast<BrowserNonClientFrameViewAsh*>(
          browser_view->frame()->GetFrameView());

  HostedAppButtonContainer* button_container =
      frame_view->hosted_app_button_container_;
  EXPECT_TRUE(button_container->visible());

  // Ensure the theme color is set.
  auto* frame_header =
      static_cast<ash::DefaultFrameHeader*>(frame_view->frame_header_.get());
  EXPECT_EQ(SK_ColorBLUE, frame_header->GetActiveFrameColor());
  EXPECT_EQ(SK_ColorBLUE, frame_header->GetInactiveFrameColor());
  // TODO(afakhry): Figure out the right way to test this (i.e. are we testing
  // that the caption colors are light since the frame color is dark? Or are we
  // testing that the colors are contrasting?). Do this while working on themes
  // in https://crbug.com/820495.
  const SkColor expected_active_icon_color =
      GetParam() ? gfx::kGoogleGrey100 : SK_ColorWHITE;
  EXPECT_EQ(expected_active_icon_color, button_container->active_icon_color_);

  // Show a content setting icon.
  auto& content_setting_views = frame_view->hosted_app_button_container_
                                    ->GetContentSettingViewsForTesting();

  for (auto* v : content_setting_views)
    EXPECT_FALSE(v->visible());

  content::RenderFrameHost* frame =
      app_browser->tab_strip_model()->GetActiveWebContents()->GetMainFrame();
  TabSpecificContentSettings* content_settings =
      TabSpecificContentSettings::GetForFrame(frame->GetProcess()->GetID(),
                                              frame->GetRoutingID());

  content_settings->OnGeolocationPermissionSet(kAppStartURL.GetOrigin(), true);

  auto is_visible = [](const ContentSettingImageView* v) {
    return v->visible();
  };

  EXPECT_EQ(1, std::count_if(content_setting_views.begin(),
                             content_setting_views.end(), is_visible));

  // Press the content setting button.
  auto content_setting_view = std::find_if(
      content_setting_views.begin(), content_setting_views.end(), is_visible);

  base::HistogramTester histograms;

  (*content_setting_view)
      ->OnKeyPressed(
          ui::KeyEvent(ui::ET_KEY_PRESSED, ui::VKEY_SPACE, ui::EF_NONE));
  (*content_setting_view)
      ->OnKeyReleased(
          ui::KeyEvent(ui::ET_KEY_RELEASED, ui::VKEY_SPACE, ui::EF_NONE));

  histograms.ExpectBucketCount(
      "HostedAppFrame.ContentSettings.ImagePressed",
      static_cast<int>(ContentSettingImageModel::ImageType::GEOLOCATION), 1);
  histograms.ExpectBucketCount(
      "ContentSettings.ImagePressed",
      static_cast<int>(ContentSettingImageModel::ImageType::GEOLOCATION), 1);

  // Even though 2 are visible in the browser, no extension actions should show.
  BrowserActionsContainer* browser_actions =
      frame_view->hosted_app_button_container_->browser_actions_container_;
  ToolbarActionsBar* toolbar_actions_bar =
      browser_actions->toolbar_actions_bar();
  LoadExtensions();
  toolbar_model()->SetVisibleIconCount(2);
  EXPECT_EQ(0u, browser_actions->VisibleBrowserActions());

  // Show the menu.
  HostedAppButtonContainer::AppMenuButton* menu_button =
      button_container->app_menu_button_;

  ui::MouseEvent e(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                   ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON, 0);
  menu_button->OnMousePressed(e);
  EXPECT_TRUE(menu_button->menu()->IsShowing());

  // All extension actions should always be showing in the menu.
  EXPECT_EQ(3u, menu_button->menu()
                    ->extension_toolbar_for_testing()
                    ->container_for_testing()
                    ->VisibleBrowserActions());

  // Popping out an extension makes its action show in the bar.
  toolbar_actions_bar->PopOutAction(toolbar_actions_bar->GetActions()[2], false,
                                    base::DoNothing());
  EXPECT_EQ(1u, browser_actions->VisibleBrowserActions());
}

namespace {

class BrowserNonClientFrameViewAshBackButtonTest
    : public TouchOptimizedUiParamTest<InProcessBrowserTest> {
 public:
  BrowserNonClientFrameViewAshBackButtonTest() = default;
  ~BrowserNonClientFrameViewAshBackButtonTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(ash::switches::kAshEnableV1AppBackButton);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(BrowserNonClientFrameViewAshBackButtonTest);
};

}  // namespace

// Test if the V1 apps' frame has a back button.
IN_PROC_BROWSER_TEST_P(BrowserNonClientFrameViewAshBackButtonTest,
                       V1BackButton) {
  browser()->window()->Close();

  // Open a new app window.
  Browser::CreateParams params = Browser::CreateParams::CreateForApp(
      "test_browser_app", true /* trusted_source */, gfx::Rect(),
      browser()->profile(), true);
  params.initial_show_state = ui::SHOW_STATE_DEFAULT;
  Browser* browser = new Browser(params);
  AddBlankTabAndShow(browser);

  views::Widget* widget = views::Widget::GetWidgetForNativeWindow(
      browser->window()->GetNativeWindow());
  BrowserNonClientFrameViewAsh* frame_view =
      static_cast<BrowserNonClientFrameViewAsh*>(
          widget->non_client_view()->frame_view());
  ASSERT_TRUE(frame_view->back_button_);
  EXPECT_TRUE(frame_view->back_button_->visible());
  // The back button should be disabled initially.
  EXPECT_FALSE(frame_view->back_button_->enabled());

  // Nagivate to a page. The back button should now be enabled.
  const GURL kAppStartURL("http://example.org/");
  NavigateParams nav_params(browser, kAppStartURL, ui::PAGE_TRANSITION_LINK);
  ui_test_utils::NavigateToURL(&nav_params);
  EXPECT_TRUE(frame_view->back_button_->enabled());

  // Go back to the blank. The back button should be disabled again.
  chrome::GoBack(browser, WindowOpenDisposition::CURRENT_TAB);
  EXPECT_FALSE(frame_view->back_button_->enabled());
}

// Test the normal type browser's kTopViewInset is always 0.
IN_PROC_BROWSER_TEST_P(BrowserNonClientFrameViewAshTest, TopViewInset) {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  ImmersiveModeController* immersive_mode_controller =
      browser_view->immersive_mode_controller();
  aura::Window* window = browser()->window()->GetNativeWindow();
  EXPECT_FALSE(immersive_mode_controller->IsEnabled());
  EXPECT_EQ(0, window->GetProperty(aura::client::kTopViewInset));

  // The kTopViewInset should be 0 when in immersive mode.
  ToggleFullscreenModeAndWait(browser());
  EXPECT_TRUE(immersive_mode_controller->IsEnabled());
  EXPECT_EQ(0, window->GetProperty(aura::client::kTopViewInset));

  // An immersive reveal shows the top of the frame.
  std::unique_ptr<ImmersiveRevealedLock> revealed_lock(
      immersive_mode_controller->GetRevealedLock(
          ImmersiveModeController::ANIMATE_REVEAL_NO));
  EXPECT_TRUE(immersive_mode_controller->IsRevealed());
  EXPECT_EQ(0, window->GetProperty(aura::client::kTopViewInset));

  // End the reveal and exit immersive mode.
  // The kTopViewInset should be 0 when immersive mode is exited.
  revealed_lock.reset();
  ToggleFullscreenModeAndWait(browser());
  EXPECT_FALSE(immersive_mode_controller->IsEnabled());
  EXPECT_EQ(0, window->GetProperty(aura::client::kTopViewInset));
}

// Disabled due to high flake rate; https://crbug.com/818170.
IN_PROC_BROWSER_TEST_P(BrowserNonClientFrameViewAshTest,
                       DISABLED_HeaderVisibilityInOverviewAndSplitview) {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  Widget* widget = browser_view->GetWidget();
  BrowserNonClientFrameViewAsh* frame_view =
      static_cast<BrowserNonClientFrameViewAsh*>(
          widget->non_client_view()->frame_view());
  widget->GetNativeWindow()->SetProperty(
      aura::client::kResizeBehaviorKey,
      ui::mojom::kResizeBehaviorCanMaximize |
          ui::mojom::kResizeBehaviorCanResize);

  // Test that the header is invisible for the browser window in overview mode
  // and visible when not in overview mode.
  ash::Shell* shell = ash::Shell::Get();
  shell->window_selector_controller()->ToggleOverview();
  EXPECT_FALSE(frame_view->caption_button_container_->visible());
  shell->window_selector_controller()->ToggleOverview();
  EXPECT_TRUE(frame_view->caption_button_container_->visible());

  // Create another browser window.
  Browser::CreateParams params = Browser::CreateParams::CreateForApp(
      "test_browser_app", true /* trusted_source */, gfx::Rect(),
      browser()->profile(), true);
  params.initial_show_state = ui::SHOW_STATE_DEFAULT;
  Browser* browser2 = new Browser(params);
  AddBlankTabAndShow(browser2);
  BrowserView* browser_view2 = BrowserView::GetBrowserViewForBrowser(browser2);
  Widget* widget2 = browser_view2->GetWidget();
  BrowserNonClientFrameViewAsh* frame_view2 =
      static_cast<BrowserNonClientFrameViewAsh*>(
          widget2->non_client_view()->frame_view());
  widget2->GetNativeWindow()->SetProperty(
      aura::client::kResizeBehaviorKey,
      ui::mojom::kResizeBehaviorCanMaximize |
          ui::mojom::kResizeBehaviorCanResize);

  // Test that when one browser window is snapped, the header is visible for the
  // snapped browser window, but invisible for the browser window still in
  // overview mode.
  shell->tablet_mode_controller()->EnableTabletModeWindowManager(true);
  ash::SplitViewController* split_view_controller =
      shell->split_view_controller();
  split_view_controller->BindRequest(
      mojo::MakeRequest(&frame_view->split_view_controller_));
  split_view_controller->BindRequest(
      mojo::MakeRequest(&frame_view2->split_view_controller_));
  split_view_controller->AddObserver(
      frame_view->CreateInterfacePtrForTesting());
  split_view_controller->AddObserver(
      frame_view2->CreateInterfacePtrForTesting());
  frame_view->split_view_controller_.FlushForTesting();
  frame_view2->split_view_controller_.FlushForTesting();

  shell->window_selector_controller()->ToggleOverview();
  split_view_controller->SnapWindow(widget->GetNativeWindow(),
                                    ash::SplitViewController::LEFT);
  frame_view->split_view_controller_.FlushForTesting();
  frame_view2->split_view_controller_.FlushForTesting();
  EXPECT_TRUE(frame_view->caption_button_container_->visible());
  EXPECT_FALSE(frame_view2->caption_button_container_->visible());

  // When both browser windows are snapped, the headers are both visible.
  split_view_controller->SnapWindow(widget2->GetNativeWindow(),
                                    ash::SplitViewController::RIGHT);
  frame_view->split_view_controller_.FlushForTesting();
  frame_view2->split_view_controller_.FlushForTesting();
  EXPECT_TRUE(frame_view->caption_button_container_->visible());
  EXPECT_TRUE(frame_view2->caption_button_container_->visible());

  // Toggle overview mode while splitview mode is active. Test that the header
  // is visible for the snapped browser window but not for the other browser
  // window in overview mode.
  shell->window_selector_controller()->ToggleOverview();
  frame_view->split_view_controller_.FlushForTesting();
  frame_view2->split_view_controller_.FlushForTesting();
  EXPECT_TRUE(frame_view->caption_button_container_->visible());
  EXPECT_FALSE(frame_view2->caption_button_container_->visible());
}

// Test the V1 apps' kTopViewInset.
IN_PROC_BROWSER_TEST_P(HostedAppNonClientFrameViewAshTest, V1AppTopViewInset) {
  browser()->window()->Close();

  // Open a new app window.
  Browser::CreateParams params = Browser::CreateParams::CreateForApp(
      "test_browser_app", true /* trusted_source */, gfx::Rect(),
      browser()->profile(), true);
  params.initial_show_state = ui::SHOW_STATE_DEFAULT;
  Browser* browser = new Browser(params);
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
  ImmersiveModeController* immersive_mode_controller =
      browser_view->immersive_mode_controller();
  aura::Window* window = browser->window()->GetNativeWindow();
  window->Show();
  EXPECT_FALSE(immersive_mode_controller->IsEnabled());
  EXPECT_LT(0, window->GetProperty(aura::client::kTopViewInset));

  // The kTopViewInset should be 0 when in immersive mode.
  ToggleFullscreenModeAndWait(browser);
  EXPECT_TRUE(immersive_mode_controller->IsEnabled());
  EXPECT_EQ(0, window->GetProperty(aura::client::kTopViewInset));

  // An immersive reveal shows the top of the frame.
  std::unique_ptr<ImmersiveRevealedLock> revealed_lock(
      immersive_mode_controller->GetRevealedLock(
          ImmersiveModeController::ANIMATE_REVEAL_NO));
  EXPECT_TRUE(immersive_mode_controller->IsRevealed());
  EXPECT_EQ(0, window->GetProperty(aura::client::kTopViewInset));

  // End the reveal and exit immersive mode.
  // The kTopViewInset should be larger than 0 again when immersive mode is
  // exited.
  revealed_lock.reset();
  ToggleFullscreenModeAndWait(browser);
  EXPECT_FALSE(immersive_mode_controller->IsEnabled());
  EXPECT_LT(0, window->GetProperty(aura::client::kTopViewInset));

  // The kTopViewInset is the same as in overview mode.
  const int inset_normal = window->GetProperty(aura::client::kTopViewInset);
  EXPECT_TRUE(
      ash::Shell::Get()->window_selector_controller()->ToggleOverview());
  const int inset_in_overview_mode =
      window->GetProperty(aura::client::kTopViewInset);
  EXPECT_EQ(inset_normal, inset_in_overview_mode);
}

#define INSTANTIATE_TEST_CASE(name)                               \
  INSTANTIATE_TEST_CASE_P(, name, ::testing::Values(true, false), \
                          &TouchOptimizedUiStatusToString)

INSTANTIATE_TEST_CASE(BrowserNonClientFrameViewAshTest);
INSTANTIATE_TEST_CASE(ImmersiveModeBrowserViewTest);
INSTANTIATE_TEST_CASE(HostedAppNonClientFrameViewAshTest);
INSTANTIATE_TEST_CASE(BrowserNonClientFrameViewAshBackButtonTest);
