// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/fullscreen/fullscreen_controller.h"
#include "chrome/browser/ui/fullscreen/fullscreen_controller_state_test.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

// The FullscreenControllerStateUnitTest unit test suite exhastively tests
// the FullscreenController through all permutations of events. The behavior
// of the BrowserWindow is mocked via FullscreenControllerTestWindow.
//
// FullscreenControllerStateInteractiveTest is an interactive test suite
// used to verify that the FullscreenControllerTestWindow models the behavior
// of actual windows accurately. The interactive tests are too flaky to run
// on infrastructure, and so those tests are disabled. Run them with:
//     interactive_ui_tests
//         --gtest_filter="FullscreenControllerStateInteractiveTest.*"
//         --gtest_also_run_disabled_tests

// A BrowserWindow used for testing FullscreenController. ----------------------
class FullscreenControllerTestWindow : public TestBrowserWindow {
 public:
  // Simulate the window state with an enumeration.
  enum WindowState {
    NORMAL,
    FULLSCREEN,
    // No TO_ state for METRO_SNAP, the windows implementation is synchronous.
    METRO_SNAP,
    TO_NORMAL,
    TO_FULLSCREEN,
  };

  FullscreenControllerTestWindow();
  virtual ~FullscreenControllerTestWindow() {}

  // BrowserWindow Interface:
  virtual void EnterFullscreen(const GURL& url,
                               FullscreenExitBubbleType type) OVERRIDE;
  virtual void EnterFullscreen();
  virtual void ExitFullscreen() OVERRIDE;
  virtual bool IsFullscreen() const OVERRIDE;
#if defined(OS_WIN)
  virtual void SetMetroSnapMode(bool enable) OVERRIDE;
  virtual bool IsInMetroSnapMode() const OVERRIDE;
#endif
#if defined(OS_MACOSX)
  virtual void EnterPresentationMode(
      const GURL& url,
      FullscreenExitBubbleType bubble_type) OVERRIDE;
  virtual void ExitPresentationMode() OVERRIDE;
  virtual bool InPresentationMode() OVERRIDE;
#endif

  static const char* GetWindowStateString(WindowState state);
  WindowState state() const { return state_; }
  void set_browser(Browser* browser) { browser_ = browser; }
  void set_reentrant(bool value) { reentrant_ = value; }
  bool reentrant() const { return reentrant_; }

  // Simulates the window changing state.
  void ChangeWindowFullscreenState();
  // Calls ChangeWindowFullscreenState() if |reentrant_| is true.
  void ChangeWindowFullscreenStateIfReentrant();

 private:
  WindowState state_;
  bool mac_presentation_mode_;
  Browser* browser_;

  // Causes reentrant calls to be made by calling
  // browser_->WindowFullscreenStateChanged() from the BrowserWindow
  // interface methods.
  bool reentrant_;
};

FullscreenControllerTestWindow::FullscreenControllerTestWindow()
    : state_(NORMAL),
      mac_presentation_mode_(false),
      browser_(NULL),
      reentrant_(false) {
}

void FullscreenControllerTestWindow::EnterFullscreen(
    const GURL& url, FullscreenExitBubbleType type) {
  EnterFullscreen();
}

void FullscreenControllerTestWindow::EnterFullscreen() {
  if (!IsFullscreen()) {
    state_ = TO_FULLSCREEN;
    ChangeWindowFullscreenStateIfReentrant();
  }
}

void FullscreenControllerTestWindow::ExitFullscreen() {
  if (IsFullscreen()) {
    state_ = TO_NORMAL;
    mac_presentation_mode_ = false;
    ChangeWindowFullscreenStateIfReentrant();
  }
}

bool FullscreenControllerTestWindow::IsFullscreen() const {
#if defined(OS_MACOSX)
  return state_ == FULLSCREEN || state_ == TO_FULLSCREEN;
#else
  return state_ == FULLSCREEN || state_ == TO_NORMAL;
#endif
}

#if defined(OS_WIN)
void FullscreenControllerTestWindow::SetMetroSnapMode(bool enable) {
  if (enable != IsInMetroSnapMode()) {
    if (enable)
      state_ = METRO_SNAP;
    else
      state_ = NORMAL;
  }
  ChangeWindowFullscreenStateIfReentrant();
}

bool FullscreenControllerTestWindow::IsInMetroSnapMode() const {
  return state_ == METRO_SNAP;
}
#endif

#if defined(OS_MACOSX)
void FullscreenControllerTestWindow::EnterPresentationMode(
    const GURL& url,
    FullscreenExitBubbleType bubble_type) {
  mac_presentation_mode_ = true;
  EnterFullscreen();
}

void FullscreenControllerTestWindow::ExitPresentationMode() {
  if (InPresentationMode()) {
    mac_presentation_mode_ = false;
    ExitFullscreen();
  }
}

bool FullscreenControllerTestWindow::InPresentationMode() {
  return mac_presentation_mode_;
}
#endif

// static
const char* FullscreenControllerTestWindow::GetWindowStateString(
    WindowState state) {
  switch (state) {
    case NORMAL:
      return "NORMAL";
    case FULLSCREEN:
      return "FULLSCREEN";
    case METRO_SNAP:
      return "METRO_SNAP";
    case TO_FULLSCREEN:
      return "TO_FULLSCREEN";
    case TO_NORMAL:
      return "TO_NORMAL";
    default:
      NOTREACHED() << "No string for state " << state;
      return "WindowState-Unknown";
  }
}

void FullscreenControllerTestWindow::ChangeWindowFullscreenState() {
  // Several states result in "no operation" intentionally. The tests
  // assume that all possible states and event pairs can be tested, even
  // though window managers will not generate all of these.
  switch (state_) {
    case NORMAL:
      break;
    case FULLSCREEN:
      break;
    case METRO_SNAP:
      break;
    case TO_FULLSCREEN:
      state_ = FULLSCREEN;
      break;
    case TO_NORMAL:
      state_ = NORMAL;
      break;
    default:
      NOTREACHED();
  }
  // Emit a change event from every state to ensure the Fullscreen Controller
  // handles it in all circumstances.
  browser_->WindowFullscreenStateChanged();
}

void FullscreenControllerTestWindow::ChangeWindowFullscreenStateIfReentrant() {
  if (reentrant_)
    ChangeWindowFullscreenState();
}

// Unit test fixture testing Fullscreen Controller through its states. ---------
class FullscreenControllerStateUnitTest : public BrowserWithTestWindowTest,
                                          public FullscreenControllerStateTest {
 public:
  FullscreenControllerStateUnitTest();

  // FullscreenControllerStateTest:
  virtual void SetUp() OVERRIDE;
  virtual void ChangeWindowFullscreenState() OVERRIDE;
  virtual const char* GetWindowStateString() OVERRIDE;
  virtual void VerifyWindowState() OVERRIDE;

 protected:
  // FullscreenControllerStateTest:
  virtual bool ShouldSkipStateAndEventPair(State state, Event event) OVERRIDE;
  virtual void TestStateAndEvent(State state,
                                 Event event,
                                 bool reentrant) OVERRIDE;
  virtual Browser* GetBrowser() OVERRIDE;
  FullscreenControllerTestWindow* window_;
};

FullscreenControllerStateUnitTest::FullscreenControllerStateUnitTest ()
    : window_(NULL) {
}

void FullscreenControllerStateUnitTest::SetUp() {
  window_ = new FullscreenControllerTestWindow();
  set_window(window_);  // BrowserWithTestWindowTest takes ownership.
  BrowserWithTestWindowTest::SetUp();
  window_->set_browser(browser());
}

void FullscreenControllerStateUnitTest::ChangeWindowFullscreenState() {
  window_->ChangeWindowFullscreenState();
}

const char* FullscreenControllerStateUnitTest::GetWindowStateString() {
  return FullscreenControllerTestWindow::GetWindowStateString(window_->state());
}

void FullscreenControllerStateUnitTest::VerifyWindowState() {
  switch (state_) {
    case STATE_NORMAL:
      EXPECT_EQ(FullscreenControllerTestWindow::NORMAL,
                window_->state()) << GetAndClearDebugLog();
      break;
    case STATE_BROWSER_FULLSCREEN_NO_CHROME:
      EXPECT_EQ(FullscreenControllerTestWindow::FULLSCREEN,
                window_->state()) << GetAndClearDebugLog();
      break;
#if defined(OS_WIN)
    case STATE_METRO_SNAP:
      EXPECT_EQ(FullscreenControllerTestWindow::METRO_SNAP,
                window_->state()) << GetAndClearDebugLog();
      break;
#endif
    case STATE_TAB_FULLSCREEN:
      EXPECT_EQ(FullscreenControllerTestWindow::FULLSCREEN,
                window_->state()) << GetAndClearDebugLog();
      break;
    case STATE_TAB_BROWSER_FULLSCREEN:
      EXPECT_EQ(FullscreenControllerTestWindow::FULLSCREEN,
                window_->state()) << GetAndClearDebugLog();
      break;
    case STATE_TO_NORMAL:
      EXPECT_EQ(FullscreenControllerTestWindow::TO_NORMAL,
                window_->state()) << GetAndClearDebugLog();
      break;
    case STATE_TO_BROWSER_FULLSCREEN_NO_CHROME:
      EXPECT_EQ(FullscreenControllerTestWindow::TO_FULLSCREEN,
                window_->state()) << GetAndClearDebugLog();
      break;
    case STATE_TO_TAB_FULLSCREEN:
      EXPECT_EQ(FullscreenControllerTestWindow::TO_FULLSCREEN,
                window_->state()) << GetAndClearDebugLog();
      break;
    default:
      NOTREACHED() << GetAndClearDebugLog();
  }

  FullscreenControllerStateTest::VerifyWindowState();
}

bool FullscreenControllerStateUnitTest::ShouldSkipStateAndEventPair(
    State state, Event event) {
#if defined(OS_MACOSX)
  // TODO(scheib) Toggle, Window Event, Toggle, Toggle on Mac as exposed by
  // test *.STATE_TO_NORMAL__TOGGLE_FULLSCREEN runs interactively and exits to
  // Normal. This doesn't appear to be the desired result, and would add
  // too much complexity to mimic in our simple FullscreenControllerTestWindow.
  // http://crbug.com/156968
  if ((state == STATE_TO_NORMAL ||
       state == STATE_TO_BROWSER_FULLSCREEN_NO_CHROME ||
       state == STATE_TO_TAB_FULLSCREEN) &&
      event == TOGGLE_FULLSCREEN)
    return true;
#endif

  return FullscreenControllerStateTest::ShouldSkipStateAndEventPair(state,
                                                                    event);
}

void FullscreenControllerStateUnitTest::TestStateAndEvent(State state,
                                                          Event event,
                                                          bool reentrant) {
  window_->set_reentrant(reentrant);
  FullscreenControllerStateTest::TestStateAndEvent(state, event, reentrant);
}

Browser* FullscreenControllerStateUnitTest::GetBrowser() {
  return BrowserWithTestWindowTest::browser();
}

// Tests -----------------------------------------------------------------------

// Soak tests:

// Tests all states with all permutations of multiple events to detect lingering
// state issues that would bleed over to other states.
// I.E. for each state test all combinations of events E1, E2, E3.
//
// This produces coverage for event sequences that may happen normally but
// would not be exposed by traversing to each state via TransitionToState().
// TransitionToState() always takes the same path even when multiple paths
// exist.
TEST_F(FullscreenControllerStateUnitTest, TransitionsForEachState) {
  // A tab is needed for tab fullscreen.
  AddTab(browser(), GURL(chrome::kAboutBlankURL));
  TestTransitionsForEachState();
  // Progress of test can be examined via LOG(INFO) << GetAndClearDebugLog();
}


// Individual tests for each pair of state and event:

#define TEST_EVENT_INNER(state, event, reentrant, reentrant_id) \
    TEST_F(FullscreenControllerStateUnitTest, \
           state##__##event##reentrant_id) { \
      AddTab(browser(), GURL(chrome::kAboutBlankURL)); \
      ASSERT_NO_FATAL_FAILURE(TestStateAndEvent(state, event, reentrant)) \
          << GetAndClearDebugLog(); \
    }
    // Progress of tests can be examined by inserting the following line:
    // LOG(INFO) << GetAndClearDebugLog(); }

#define TEST_EVENT(state, event) \
    TEST_EVENT_INNER(state, event, false, ); \
    TEST_EVENT_INNER(state, event, true, _Reentrant);

TEST_EVENT(STATE_NORMAL, TOGGLE_FULLSCREEN);
TEST_EVENT(STATE_NORMAL, TAB_FULLSCREEN_TRUE);
TEST_EVENT(STATE_NORMAL, TAB_FULLSCREEN_FALSE);
#if defined(OS_WIN)
TEST_EVENT(STATE_NORMAL, METRO_SNAP_TRUE);
TEST_EVENT(STATE_NORMAL, METRO_SNAP_FALSE);
#endif
TEST_EVENT(STATE_NORMAL, BUBBLE_EXIT_LINK);
TEST_EVENT(STATE_NORMAL, BUBBLE_ALLOW);
TEST_EVENT(STATE_NORMAL, BUBBLE_DENY);
TEST_EVENT(STATE_NORMAL, WINDOW_CHANGE);

TEST_EVENT(STATE_BROWSER_FULLSCREEN_NO_CHROME, TOGGLE_FULLSCREEN);
TEST_EVENT(STATE_BROWSER_FULLSCREEN_NO_CHROME, TAB_FULLSCREEN_TRUE);
TEST_EVENT(STATE_BROWSER_FULLSCREEN_NO_CHROME, TAB_FULLSCREEN_FALSE);
#if defined(OS_WIN)
TEST_EVENT(STATE_BROWSER_FULLSCREEN_NO_CHROME, METRO_SNAP_TRUE);
TEST_EVENT(STATE_BROWSER_FULLSCREEN_NO_CHROME, METRO_SNAP_FALSE);
#endif
TEST_EVENT(STATE_BROWSER_FULLSCREEN_NO_CHROME, BUBBLE_EXIT_LINK);
TEST_EVENT(STATE_BROWSER_FULLSCREEN_NO_CHROME, BUBBLE_ALLOW);
TEST_EVENT(STATE_BROWSER_FULLSCREEN_NO_CHROME, BUBBLE_DENY);
TEST_EVENT(STATE_BROWSER_FULLSCREEN_NO_CHROME, WINDOW_CHANGE);

#if defined(OS_WIN)
TEST_EVENT(STATE_METRO_SNAP, TOGGLE_FULLSCREEN);
TEST_EVENT(STATE_METRO_SNAP, TAB_FULLSCREEN_TRUE);
TEST_EVENT(STATE_METRO_SNAP, TAB_FULLSCREEN_FALSE);
TEST_EVENT(STATE_METRO_SNAP, METRO_SNAP_TRUE);
TEST_EVENT(STATE_METRO_SNAP, METRO_SNAP_FALSE);
TEST_EVENT(STATE_METRO_SNAP, BUBBLE_EXIT_LINK);
TEST_EVENT(STATE_METRO_SNAP, BUBBLE_ALLOW);
TEST_EVENT(STATE_METRO_SNAP, BUBBLE_DENY);
TEST_EVENT(STATE_METRO_SNAP, WINDOW_CHANGE);
#endif

TEST_EVENT(STATE_TAB_FULLSCREEN, TOGGLE_FULLSCREEN);
TEST_EVENT(STATE_TAB_FULLSCREEN, TAB_FULLSCREEN_TRUE);
TEST_EVENT(STATE_TAB_FULLSCREEN, TAB_FULLSCREEN_FALSE);
#if defined(OS_WIN)
TEST_EVENT(STATE_TAB_FULLSCREEN, METRO_SNAP_TRUE);
TEST_EVENT(STATE_TAB_FULLSCREEN, METRO_SNAP_FALSE);
#endif
TEST_EVENT(STATE_TAB_FULLSCREEN, BUBBLE_EXIT_LINK);
TEST_EVENT(STATE_TAB_FULLSCREEN, BUBBLE_ALLOW);
TEST_EVENT(STATE_TAB_FULLSCREEN, BUBBLE_DENY);
TEST_EVENT(STATE_TAB_FULLSCREEN, WINDOW_CHANGE);

TEST_EVENT(STATE_TAB_BROWSER_FULLSCREEN, TOGGLE_FULLSCREEN);
TEST_EVENT(STATE_TAB_BROWSER_FULLSCREEN, TAB_FULLSCREEN_TRUE);
TEST_EVENT(STATE_TAB_BROWSER_FULLSCREEN, TAB_FULLSCREEN_FALSE);
#if defined(OS_WIN)
TEST_EVENT(STATE_TAB_BROWSER_FULLSCREEN, METRO_SNAP_TRUE);
TEST_EVENT(STATE_TAB_BROWSER_FULLSCREEN, METRO_SNAP_FALSE);
#endif
TEST_EVENT(STATE_TAB_BROWSER_FULLSCREEN, BUBBLE_EXIT_LINK);
TEST_EVENT(STATE_TAB_BROWSER_FULLSCREEN, BUBBLE_ALLOW);
TEST_EVENT(STATE_TAB_BROWSER_FULLSCREEN, BUBBLE_DENY);
TEST_EVENT(STATE_TAB_BROWSER_FULLSCREEN, WINDOW_CHANGE);

TEST_EVENT(STATE_TO_NORMAL, TOGGLE_FULLSCREEN);
TEST_EVENT(STATE_TO_NORMAL, TAB_FULLSCREEN_TRUE);
TEST_EVENT(STATE_TO_NORMAL, TAB_FULLSCREEN_FALSE);
#if defined(OS_WIN)
TEST_EVENT(STATE_TO_NORMAL, METRO_SNAP_TRUE);
TEST_EVENT(STATE_TO_NORMAL, METRO_SNAP_FALSE);
#endif
TEST_EVENT(STATE_TO_NORMAL, BUBBLE_EXIT_LINK);
TEST_EVENT(STATE_TO_NORMAL, BUBBLE_ALLOW);
TEST_EVENT(STATE_TO_NORMAL, BUBBLE_DENY);
TEST_EVENT(STATE_TO_NORMAL, WINDOW_CHANGE);

TEST_EVENT(STATE_TO_BROWSER_FULLSCREEN_NO_CHROME, TOGGLE_FULLSCREEN);
TEST_EVENT(STATE_TO_BROWSER_FULLSCREEN_NO_CHROME, TAB_FULLSCREEN_TRUE);
TEST_EVENT(STATE_TO_BROWSER_FULLSCREEN_NO_CHROME, TAB_FULLSCREEN_FALSE);
#if defined(OS_WIN)
TEST_EVENT(STATE_TO_BROWSER_FULLSCREEN_NO_CHROME, METRO_SNAP_TRUE);
TEST_EVENT(STATE_TO_BROWSER_FULLSCREEN_NO_CHROME, METRO_SNAP_FALSE);
#endif
TEST_EVENT(STATE_TO_BROWSER_FULLSCREEN_NO_CHROME, BUBBLE_EXIT_LINK);
TEST_EVENT(STATE_TO_BROWSER_FULLSCREEN_NO_CHROME, BUBBLE_ALLOW);
TEST_EVENT(STATE_TO_BROWSER_FULLSCREEN_NO_CHROME, BUBBLE_DENY);
TEST_EVENT(STATE_TO_BROWSER_FULLSCREEN_NO_CHROME, WINDOW_CHANGE);

TEST_EVENT(STATE_TO_TAB_FULLSCREEN, TOGGLE_FULLSCREEN);
TEST_EVENT(STATE_TO_TAB_FULLSCREEN, TAB_FULLSCREEN_TRUE);
TEST_EVENT(STATE_TO_TAB_FULLSCREEN, TAB_FULLSCREEN_FALSE);
#if defined(OS_WIN)
TEST_EVENT(STATE_TO_TAB_FULLSCREEN, METRO_SNAP_TRUE);
TEST_EVENT(STATE_TO_TAB_FULLSCREEN, METRO_SNAP_FALSE);
#endif
TEST_EVENT(STATE_TO_TAB_FULLSCREEN, BUBBLE_EXIT_LINK);
TEST_EVENT(STATE_TO_TAB_FULLSCREEN, BUBBLE_ALLOW);
TEST_EVENT(STATE_TO_TAB_FULLSCREEN, BUBBLE_DENY);
TEST_EVENT(STATE_TO_TAB_FULLSCREEN, WINDOW_CHANGE);


// Specific one-off tests for known issues:

// TODO(scheib) Toggling Tab fullscreen while pending Tab or
// Browser fullscreen is broken currently http://crbug.com/154196
TEST_F(FullscreenControllerStateUnitTest,
       DISABLED_ToggleTabWhenPendingBrowser) {
#if !defined(OS_WIN)  // Only possible without reentrancy
  AddTab(browser(), GURL(chrome::kAboutBlankURL));
  ASSERT_NO_FATAL_FAILURE(
      TransitionToState(STATE_TO_BROWSER_FULLSCREEN_NO_CHROME))
      << GetAndClearDebugLog();

  ASSERT_TRUE(InvokeEvent(TAB_FULLSCREEN_TRUE)) << GetAndClearDebugLog();
  ASSERT_TRUE(InvokeEvent(TAB_FULLSCREEN_FALSE)) << GetAndClearDebugLog();
  ASSERT_TRUE(InvokeEvent(WINDOW_CHANGE)) << GetAndClearDebugLog();
#endif
}

// TODO(scheib) Toggling Tab fullscreen while pending Tab or
// Browser fullscreen is broken currently http://crbug.com/154196
TEST_F(FullscreenControllerStateUnitTest, DISABLED_ToggleTabWhenPendingTab) {
#if !defined(OS_WIN)  // Only possible without reentrancy
  AddTab(browser(), GURL(chrome::kAboutBlankURL));
  ASSERT_NO_FATAL_FAILURE(
      TransitionToState(STATE_TO_TAB_FULLSCREEN))
      << GetAndClearDebugLog();

  ASSERT_TRUE(InvokeEvent(TAB_FULLSCREEN_TRUE)) << GetAndClearDebugLog();
  ASSERT_TRUE(InvokeEvent(TAB_FULLSCREEN_FALSE)) << GetAndClearDebugLog();
  ASSERT_TRUE(InvokeEvent(WINDOW_CHANGE)) << GetAndClearDebugLog();
#endif
}

