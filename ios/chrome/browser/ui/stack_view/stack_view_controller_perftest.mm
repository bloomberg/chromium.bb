// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UIKit/UIKit.h>

#include "base/callback_helpers.h"
#import "base/mac/bind_objc_block.h"
#include "base/mac/foundation_util.h"
#include "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "ios/chrome/browser/snapshots/snapshot_manager.h"
#import "ios/chrome/browser/tabs/tab.h"
#import "ios/chrome/browser/tabs/tab_model.h"
#include "ios/chrome/browser/test/perf_test_with_bvc_ios.h"
#import "ios/chrome/browser/ui/browser_view_controller.h"
#import "ios/chrome/browser/ui/fullscreen_controller.h"
#import "ios/chrome/browser/ui/stack_view/stack_view_controller.h"
#include "ios/chrome/browser/ui/ui_util.h"
#include "ios/web/public/referrer.h"
#import "net/base/mac/url_conversions.h"

// These tests measure the performance of opening the stack view controller on
// an iPhone. On an iPad, the tests do not run, as the iPad does not use the
// stack view controller.

// Opening the SVC smoothly and quickly is critical to the user experience, and
// any sort of pause or delay is awkward and gives a poor impression.
// The target time of opening SVC is less than 250 ms. In order to target
// optimizations and ensure that improvements actually lower the time, this test
// aims to mimic as closely as possible the application experience.

// To mimic the full experience, it is necessary to set up a browser view
// controller, with a website loaded, and use as much of the generic experience
// as possible while culling items such as PrerenderController which may
// muddy the performance analysis. In that vein, the tests wait for websites to
// load before opening the stack view controller, to avoid slowing down the
// animation run loops with networking or javascript calls. This way the tests
// provide meaningful and trackable performance numbers. The downside is that it
// means the tests do not completely mimic the user experience, as the animation
// may in fact take longer if the website is still loading, and the UIWebView
// making network calls on the main thread.

// Testing delegate to receive animation start and end notifications.
// It contains a weak pointer to the BrowserViewController so that it can
// take a snapshot of the toolbar for the toolbar animation.
@interface StackViewControllerPerfTestDelegate
    : NSObject<TabSwitcherDelegate, StackViewControllerTestDelegate> {
 @private
  BOOL showAnimationStarted_;
  BOOL showAnimationEnded_;
  BOOL dismissAnimationStarted_;
  BOOL dismissAnimationEnded_;
  BOOL preloadCardViewsEnded_;
 @public
  BrowserViewController* bvc_;  // weak
}

- (void)reinitialize;

@property(nonatomic, assign) BOOL showAnimationStarted;
@property(nonatomic, assign) BOOL showAnimationEnded;
@property(nonatomic, assign) BOOL dismissAnimationStarted;
@property(nonatomic, assign) BOOL dismissAnimationEnded;
@property(nonatomic, assign) BOOL preloadCardViewsEnded;

@end

@implementation StackViewControllerPerfTestDelegate

@synthesize showAnimationStarted = showAnimationStarted_;
@synthesize showAnimationEnded = showAnimationEnded_;
@synthesize dismissAnimationStarted = dismissAnimationStarted_;
@synthesize dismissAnimationEnded = dismissAnimationEnded_;
@synthesize preloadCardViewsEnded = preloadCardViewsEnded_;

- (id)initWithBrowserViewController:(BrowserViewController*)bvc {
  self = [super init];
  if (self)
    bvc_ = bvc;
  return self;
}

- (void)reinitialize {
  [self setShowAnimationStarted:NO];
  [self setShowAnimationEnded:NO];
  [self setDismissAnimationStarted:NO];
  [self setDismissAnimationEnded:NO];
  [self setPreloadCardViewsEnded:NO];
}

- (void)stackViewControllerShowWithSelectedTabAnimationDidStart {
  [self setShowAnimationStarted:YES];
}

- (void)stackViewControllerShowWithSelectedTabAnimationDidEnd {
  [self setShowAnimationEnded:YES];
}

- (void)tabSwitcher:(id<TabSwitcher>)tabSwitcher
    dismissTransitionWillStartWithActiveModel:(TabModel*)tabModel {
  [self setDismissAnimationStarted:YES];
}

- (void)tabSwitcherDismissTransitionDidEnd:(id<TabSwitcher>)tabSwitcher {
  [self setDismissAnimationEnded:YES];
}

- (void)stackViewControllerPreloadCardViewsDidEnd {
  [self setPreloadCardViewsEnded:YES];
}

- (void)tabSwitcherPresentationTransitionDidEnd:(id<TabSwitcher>)tabSwitcher {
  [self stackViewControllerShowWithSelectedTabAnimationDidEnd];
}

- (id<ToolbarOwner>)tabSwitcherTransitionToolbarOwner {
  return bvc_;
}

@end

#pragma mark -

namespace {

#define ARRAYSIZE(a)            \
  ((sizeof(a) / sizeof(*(a))) / \
   static_cast<size_t>(!(sizeof(a) % sizeof(*(a)))))

// Use multiple URLs in the test in case the complexity of a website has an
// effect on the performance of UIWebView -renderInContext.
static const char* url_list[] = {
    // TODO(crbug.com/546315): Create static websites for these.
    "https://www.google.com", "https://news.google.com",
    "https://browsingtest.appspot.com",
};

// The maximum delay of a single spin of the run loop in seconds.
// This is very small to ensure that we are spining fast enough.
const NSTimeInterval kSpinDelay = 0.01;  // seconds
// The total maximum delay of all spins of the run loop while
// waiting for the stack view to appear and disappear.
const NSTimeInterval kTotalSpinDelay = 20;  // seconds
// The maximum time to wait for a website to load.
const NSTimeInterval kMaxPageLoadDelay = 20;  // seconds
// The time UI gets to catch up after a website has loaded.  Give a full second
// to make sure the progress bar's finish delay has completed and the toolbar
// snapshot taken.
const NSTimeInterval kMaxUICatchupDelay = 1.0;  // seconds

class StackViewControllerPerfTest : public PerfTestWithBVC {
 public:
  StackViewControllerPerfTest() : PerfTestWithBVC("Stack View") {}

  void SetUp() override {
    // Opening a StackViewController is done only on iPhones, not on iPads.
    // This test is meaningless on an iPad.
    if (IsIPadIdiom())
      return;

    // Base class does most of the setup.
    PerfTestWithBVC::SetUp();

    current_url_index_ = 0;
    reuse_svc_ = false;

    // The testing delegate will receive stack view animation notifications.
    delegate_.reset([[StackViewControllerPerfTestDelegate alloc]
        initWithBrowserViewController:bvc_]);
  }
  void TearDown() override {
    // Opening a StackViewController is done only on iPhones, not on iPads.
    // This test is meaningless on an iPad.
    if (IsIPadIdiom())
      return;

    PerfTestWithBVC::TearDown();
  }

 protected:
  // Stack view controller & delegate.
  base::scoped_nsobject<StackViewControllerPerfTestDelegate> delegate_;
  base::scoped_nsobject<StackViewController> view_controller_;

  int current_url_index_;
  BOOL reuse_svc_;

  // Utility function to print out timing information for testing
  // with |number_of_tabs| tabs opened.
  void PrintTestResult(std::string test_description,
                       int number_of_tabs,
                       base::TimeDelta elapsed) {
    std::stringstream test_name;
    test_name << test_description << " - " << number_of_tabs << " Tab"
              << (number_of_tabs == 1 ? "" : "s");
    LogPerfTiming(test_name.str(), elapsed);
  }

  // Creates and adds |number_of_tabs| tabs to the tab model. If |same_url|
  // is true, always uses "www.google.com", otherwise iterates through url_list.
  void CreateTabs(int number_of_tabs, bool same_url);
  // Navigates to the next URL in the current tab.
  void LoadNextURL();

  // Gets the next URL in the list.
  const GURL NextURL();
  // Gets "google.com"
  static const GURL GoogleURL();
  // Waits for the page to load in the given tab.
  static void WaitForPageLoad(Tab* tab);
  // Creates the stack view and adds it to the main window.
  base::TimeDelta OpenStackView();
  // Copy of MainController's -showTabSwitcher function. Pulling in all of
  // MainController is not practical for unit tests, nor necessary.
  void MainControllerShowTabSwitcher();
  // Dismisses the stack view and removes it from the main window.
  base::TimeDelta CloseStackView();

  // Time how long it takes BVC to make a snapshot of the current website.
  base::TimeDelta TakeSnapshot();
};

void StackViewControllerPerfTest::CreateTabs(int number_of_tabs,
                                             bool same_url) {
  // Create and add the tabs to the tab model.
  Tab* tab = nil;
  for (int i = 0; i < number_of_tabs; i++) {
    tab = [bvc_ addSelectedTabWithURL:(same_url ? GoogleURL() : NextURL())
                           transition:(ui::PAGE_TRANSITION_AUTO_TOPLEVEL)];
    WaitForPageLoad(tab);
  }
}

void StackViewControllerPerfTest::LoadNextURL() {
  [bvc_ loadURL:NextURL()
               referrer:web::Referrer()
             transition:(ui::PAGE_TRANSITION_TYPED)
      rendererInitiated:NO];
  Tab* tab = [tab_model_ currentTab];
  WaitForPageLoad(tab);
}

const GURL StackViewControllerPerfTest::NextURL() {
  current_url_index_++;
  if (static_cast<unsigned long>(current_url_index_) >= ARRAYSIZE(url_list))
    current_url_index_ = 0;
  return GURL(url_list[current_url_index_]);
}

const GURL StackViewControllerPerfTest::GoogleURL() {
  return GURL("http://www.google.com");
}

void StackViewControllerPerfTest::WaitForPageLoad(Tab* tab) {
  base::test::ios::WaitUntilCondition(
      ^bool() {
        return !tab.webState->IsLoading();
      },
      false, base::TimeDelta::FromSecondsD(kMaxPageLoadDelay));
  base::test::ios::WaitUntilCondition(
      nil, false, base::TimeDelta::FromSecondsD(kMaxUICatchupDelay));
}

base::TimeDelta StackViewControllerPerfTest::OpenStackView() {
  return base::test::ios::TimeUntilCondition(
      ^{
        [delegate_ reinitialize];
        MainControllerShowTabSwitcher();
      },
      ^bool() {
        return [delegate_ showAnimationEnded];
      },
      false, base::TimeDelta::FromSecondsD(kTotalSpinDelay));
}

void StackViewControllerPerfTest::MainControllerShowTabSwitcher() {
  // The code for this function is copied from MainController -showTabSwitcher.
  // Note that if the code there changes, this code should change to match.
  Tab* currentTab = [[bvc_ tabModel] currentTab];

  // In order to generate the transition between the current browser view
  // controller and the tab switcher controller it's possible that multiple
  // screenshots of the same tab are taken. Since taking a screenshot is
  // expensive we activate snapshot coalescing in the scope of this function
  // which will cache the first snapshot for the tab and reuse it instead of
  // regenerating a new one each time.
  [currentTab setSnapshotCoalescingEnabled:YES];
  base::ScopedClosureRunner runner(base::BindBlock(^{
    [currentTab setSnapshotCoalescingEnabled:NO];
  }));

  if (!view_controller_) {
    view_controller_.reset([[StackViewController alloc]
        initWithMainTabModel:tab_model_
                 otrTabModel:otr_tab_model_
              activeTabModel:tab_model_]);
  } else {
    [view_controller_ restoreInternalStateWithMainTabModel:tab_model_
                                               otrTabModel:otr_tab_model_
                                            activeTabModel:tab_model_];
  }
  [view_controller_ setDelegate:delegate_];

  // The only addition to the function for testing.
  [view_controller_ setTestDelegate:delegate_];

  [bvc_ presentViewController:view_controller_.get()
                     animated:NO
                   completion:^{
                     [view_controller_ showWithSelectedTabAnimation];
                     EXPECT_TRUE([delegate_ showAnimationStarted]);
                     EXPECT_FALSE([delegate_ showAnimationEnded]);
                   }];
}

base::TimeDelta StackViewControllerPerfTest::CloseStackView() {
  base::Time startTime = base::Time::NowFromSystemTime();
  // Spin and wait for the dismiss stack view animation to finish.
  base::test::ios::TimeUntilCondition(
      ^{
        [view_controller_ dismissWithSelectedTabAnimation];
        EXPECT_TRUE([delegate_ dismissAnimationStarted]);
        EXPECT_FALSE([delegate_ dismissAnimationEnded]);
      },
      ^bool() {
        return [delegate_ dismissAnimationEnded];
      },
      false, base::TimeDelta::FromSecondsD(kTotalSpinDelay));

  [view_controller_ dismissViewControllerAnimated:NO completion:nil];
  if (!reuse_svc_)
    view_controller_.reset();

  base::TimeDelta closeTime = base::Time::NowFromSystemTime() - startTime;

  // Run the runloop a bit longer to give time for temporary retains that happen
  // in the OS during view teardown to resolve, so that the view gets its
  // dismissal callbacks.
  base::test::ios::WaitUntilCondition(
      nil, false, base::TimeDelta::FromSecondsD(kSpinDelay));

  return closeTime;
}

base::TimeDelta StackViewControllerPerfTest::TakeSnapshot() {
  base::Time startTime = base::Time::NowFromSystemTime();
  UIImage* image = [[tab_model_ currentTab] updateSnapshotWithOverlay:YES
                                                     visibleFrameOnly:YES];
  base::TimeDelta elapsed = base::Time::NowFromSystemTime() - startTime;
  EXPECT_TRUE(image);
  return elapsed;
}

TEST_F(StackViewControllerPerfTest, WebView_Shapshot) {
  // Opening a StackViewController is done only on iPhones, not on iPads.
  // This test is meaningless on an iPad.
  if (IsIPadIdiom())
    return;
  const int kNumTests = 10;
  base::TimeDelta times[kNumTests];
  CreateTabs(1, false);
  for (int i = 0; i < kNumTests; i++) {
    times[i] = TakeSnapshot();
    LoadNextURL();
  }

  base::TimeDelta avg = CalculateAverage(times, kNumTests, NULL, NULL);
  LogPerfTiming("Snapshot", avg);
}

// TODO(crbug.com/546328): Add back in tests for checking opening & closing
// stack view controller with multiple tabs open.
TEST_F(StackViewControllerPerfTest, DISABLED_OpenAndCloseStackView_1_Tab) {
  // Opening a StackViewController is done only on iPhones, not on iPads.
  // This test is meaningless on an iPad.
  if (IsIPadIdiom())
    return;
  reuse_svc_ = true;
  const int kNumTests = 10;
  base::TimeDelta open_times[kNumTests];
  base::TimeDelta close_times[kNumTests];
  CreateTabs(1, false);
  for (int i = 0; i < kNumTests; i++) {
    open_times[i] = OpenStackView();
    close_times[i] = CloseStackView();
    LoadNextURL();
  }

  base::TimeDelta max_open;
  base::TimeDelta max_close;
  // When calculating the average, only take into account the 'warm' tests.
  // i.e. ignore the 'cold' time.
  base::TimeDelta open_avg =
      CalculateAverage(open_times + 1, kNumTests - 1, NULL, &max_open);
  base::TimeDelta close_avg =
      CalculateAverage(close_times + 1, kNumTests - 1, NULL, &max_close);
  LogPerfTiming("Open cold", open_times[0]);
  LogPerfTiming("Open warm avg", open_avg);
  LogPerfTiming("Open warm max", max_open);
  LogPerfTiming("Close cold", close_times[0]);
  LogPerfTiming("Close cold avg", close_avg);
  LogPerfTiming("Close cold max", max_close);
}

}  // anonymous namespace
