// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/fullscreen/fullscreen_controller.h"
#include "chrome/browser/ui/fullscreen/fullscreen_controller_state_test.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

// The FullscreenControllerStateUnitTest unit test suite exhastively tests
// the FullscreenController through all permutations of events. The behavior
// of the BrowserWindow is mocked via FullscreenControllerTestWindow.


// FullscreenControllerTestWindow ----------------------------------------------

// A BrowserWindow used for testing FullscreenController. The behavior of this
// mock is verfied manually by running FullscreenControllerStateInteractiveTest.
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
  virtual void ExitFullscreen() OVERRIDE;
  virtual bool ShouldHideUIForFullscreen() const OVERRIDE;
  virtual bool IsFullscreen() const OVERRIDE;
#if defined(OS_WIN)
  virtual void SetMetroSnapMode(bool enable) OVERRIDE;
  virtual bool IsInMetroSnapMode() const OVERRIDE;
#endif
#if defined(OS_MACOSX)
  virtual void EnterFullscreenWithChrome() OVERRIDE;
  virtual bool IsFullscreenWithChrome() OVERRIDE;
  virtual bool IsFullscreenWithoutChrome() OVERRIDE;
#endif

  static const char* GetWindowStateString(WindowState state);
  WindowState state() const { return state_; }
  void set_browser(Browser* browser) { browser_ = browser; }

  // Simulates the window changing state.
  void ChangeWindowFullscreenState();

 private:
  // Enters fullscreen with |new_mac_with_chrome_mode|.
  void EnterFullscreen(bool new_mac_with_chrome_mode);

  // Returns true if ChangeWindowFullscreenState() should be called as a result
  // of updating the current fullscreen state to the passed in state.
  bool IsTransitionReentrant(bool new_fullscreen,
                             bool new_mac_with_chrome_mode);

  WindowState state_;
  bool mac_with_chrome_mode_;
  Browser* browser_;
};

FullscreenControllerTestWindow::FullscreenControllerTestWindow()
    : state_(NORMAL),
      mac_with_chrome_mode_(false),
      browser_(NULL) {
}

void FullscreenControllerTestWindow::EnterFullscreen(
    const GURL& url, FullscreenExitBubbleType type) {
  EnterFullscreen(false);
}

void FullscreenControllerTestWindow::ExitFullscreen() {
  if (IsFullscreen()) {
    state_ = TO_NORMAL;
    mac_with_chrome_mode_ = false;

    if (IsTransitionReentrant(false, false))
      ChangeWindowFullscreenState();
  }
}

bool FullscreenControllerTestWindow::ShouldHideUIForFullscreen() const {
  return IsFullscreen();
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
  if (enable != IsInMetroSnapMode())
    state_ = enable ? METRO_SNAP : NORMAL;

  if (FullscreenControllerStateTest::IsWindowFullscreenStateChangedReentrant())
    ChangeWindowFullscreenState();
}

bool FullscreenControllerTestWindow::IsInMetroSnapMode() const {
  return state_ == METRO_SNAP;
}
#endif

#if defined(OS_MACOSX)
void FullscreenControllerTestWindow::EnterFullscreenWithChrome() {
  EnterFullscreen(true);
}

bool FullscreenControllerTestWindow::IsFullscreenWithChrome() {
  return IsFullscreen() && mac_with_chrome_mode_;
}

bool FullscreenControllerTestWindow::IsFullscreenWithoutChrome() {
  return IsFullscreen() && !mac_with_chrome_mode_;
}
#endif

// static
const char* FullscreenControllerTestWindow::GetWindowStateString(
    WindowState state) {
  switch (state) {
    ENUM_TO_STRING(NORMAL);
    ENUM_TO_STRING(FULLSCREEN);
    ENUM_TO_STRING(METRO_SNAP);
    ENUM_TO_STRING(TO_FULLSCREEN);
    ENUM_TO_STRING(TO_NORMAL);
    default:
      NOTREACHED() << "No string for state " << state;
      return "WindowState-Unknown";
  }
}

void FullscreenControllerTestWindow::ChangeWindowFullscreenState() {
  // Most states result in "no operation" intentionally. The tests
  // assume that all possible states and event pairs can be tested, even
  // though window managers will not generate all of these.
  if (state_ == TO_FULLSCREEN)
      state_ = FULLSCREEN;
  else if (state_ == TO_NORMAL)
      state_ = NORMAL;

  // Emit a change event from every state to ensure the Fullscreen Controller
  // handles it in all circumstances.
  browser_->WindowFullscreenStateChanged();
}

void FullscreenControllerTestWindow::EnterFullscreen(
    bool new_mac_with_chrome_mode) {
  bool reentrant = IsTransitionReentrant(true, new_mac_with_chrome_mode);

  mac_with_chrome_mode_ = new_mac_with_chrome_mode;
  if (!IsFullscreen())
    state_ = TO_FULLSCREEN;

  if (reentrant)
    ChangeWindowFullscreenState();
}

bool FullscreenControllerTestWindow::IsTransitionReentrant(
    bool new_fullscreen,
    bool new_mac_with_chrome_mode) {
#if defined(OS_MACOSX)
  bool mac_with_chrome_mode_changed = new_mac_with_chrome_mode ?
      IsFullscreenWithoutChrome() : IsFullscreenWithChrome();
#else
  bool mac_with_chrome_mode_changed = false;
#endif
  bool fullscreen_changed = (new_fullscreen != IsFullscreen());

  if (!fullscreen_changed && !mac_with_chrome_mode_changed)
    return false;

  if (FullscreenControllerStateTest::IsWindowFullscreenStateChangedReentrant())
    return true;

  // BrowserWindowCocoa::EnterFullscreen() and
  // BrowserWindowCocoa::EnterFullscreenWithChrome() are reentrant when
  // switching between fullscreen with chrome and fullscreen without chrome.
  return state_ == FULLSCREEN &&
      !fullscreen_changed &&
      mac_with_chrome_mode_changed;
}


// FullscreenControllerStateUnitTest -------------------------------------------

// Unit test fixture testing Fullscreen Controller through its states. Most of
// the test logic comes from FullscreenControllerStateTest.
class FullscreenControllerStateUnitTest : public BrowserWithTestWindowTest,
                                          public FullscreenControllerStateTest {
 public:
  FullscreenControllerStateUnitTest();

  // FullscreenControllerStateTest:
  virtual void SetUp() OVERRIDE;
  virtual BrowserWindow* CreateBrowserWindow() OVERRIDE;
  virtual void ChangeWindowFullscreenState() OVERRIDE;
  virtual const char* GetWindowStateString() OVERRIDE;
  virtual void VerifyWindowState() OVERRIDE;

 protected:
  // FullscreenControllerStateTest:
  virtual bool ShouldSkipStateAndEventPair(State state, Event event) OVERRIDE;
  virtual Browser* GetBrowser() OVERRIDE;
  FullscreenControllerTestWindow* window_;
};

FullscreenControllerStateUnitTest::FullscreenControllerStateUnitTest ()
    : window_(NULL) {
}

void FullscreenControllerStateUnitTest::SetUp() {
  BrowserWithTestWindowTest::SetUp();
  window_->set_browser(browser());
}

BrowserWindow* FullscreenControllerStateUnitTest::CreateBrowserWindow() {
  window_ = new FullscreenControllerTestWindow();
  return window_;  // BrowserWithTestWindowTest takes ownership.
}

void FullscreenControllerStateUnitTest::ChangeWindowFullscreenState() {
  window_->ChangeWindowFullscreenState();
}

const char* FullscreenControllerStateUnitTest::GetWindowStateString() {
  return FullscreenControllerTestWindow::GetWindowStateString(window_->state());
}

void FullscreenControllerStateUnitTest::VerifyWindowState() {
  switch (state()) {
    case STATE_NORMAL:
      EXPECT_EQ(FullscreenControllerTestWindow::NORMAL,
                window_->state()) << GetAndClearDebugLog();
      break;

    case STATE_BROWSER_FULLSCREEN_NO_CHROME:
    case STATE_BROWSER_FULLSCREEN_WITH_CHROME:
    case STATE_TAB_FULLSCREEN:
    case STATE_TAB_BROWSER_FULLSCREEN:
    case STATE_TAB_BROWSER_FULLSCREEN_CHROME:
      EXPECT_EQ(FullscreenControllerTestWindow::FULLSCREEN,
                window_->state()) << GetAndClearDebugLog();
      break;

#if defined(OS_WIN)
    case STATE_METRO_SNAP:
      EXPECT_EQ(FullscreenControllerTestWindow::METRO_SNAP,
                window_->state()) << GetAndClearDebugLog();
      break;
#endif

    case STATE_TO_NORMAL:
      EXPECT_EQ(FullscreenControllerTestWindow::TO_NORMAL,
                window_->state()) << GetAndClearDebugLog();
      break;

    case STATE_TO_BROWSER_FULLSCREEN_NO_CHROME:
    case STATE_TO_BROWSER_FULLSCREEN_WITH_CHROME:
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

Browser* FullscreenControllerStateUnitTest::GetBrowser() {
  return BrowserWithTestWindowTest::browser();
}


// Soak tests ------------------------------------------------------------------

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
  AddTab(browser(), GURL(content::kAboutBlankURL));
  TestTransitionsForEachState();
  // Progress of test can be examined via LOG(INFO) << GetAndClearDebugLog();
}


// Individual tests for each pair of state and event ---------------------------

#define TEST_EVENT(state, event) \
    TEST_F(FullscreenControllerStateUnitTest, state##__##event) { \
      AddTab(browser(), GURL(content::kAboutBlankURL)); \
      ASSERT_NO_FATAL_FAILURE(TestStateAndEvent(state, event)) \
          << GetAndClearDebugLog(); \
    }
    // Progress of tests can be examined by inserting the following line:
    // LOG(INFO) << GetAndClearDebugLog(); }

#include "chrome/browser/ui/fullscreen/fullscreen_controller_state_tests.h"


// Specific one-off tests for known issues -------------------------------------

// TODO(scheib) Toggling Tab fullscreen while pending Tab or
// Browser fullscreen is broken currently http://crbug.com/154196
TEST_F(FullscreenControllerStateUnitTest,
       DISABLED_ToggleTabWhenPendingBrowser) {
  // Only possible without reentrancy.
  if (FullscreenControllerStateTest::IsWindowFullscreenStateChangedReentrant())
    return;
  AddTab(browser(), GURL(content::kAboutBlankURL));
  ASSERT_NO_FATAL_FAILURE(
      TransitionToState(STATE_TO_BROWSER_FULLSCREEN_NO_CHROME))
      << GetAndClearDebugLog();

  ASSERT_TRUE(InvokeEvent(TAB_FULLSCREEN_TRUE)) << GetAndClearDebugLog();
  ASSERT_TRUE(InvokeEvent(TAB_FULLSCREEN_FALSE)) << GetAndClearDebugLog();
  ASSERT_TRUE(InvokeEvent(WINDOW_CHANGE)) << GetAndClearDebugLog();
}

// TODO(scheib) Toggling Tab fullscreen while pending Tab or
// Browser fullscreen is broken currently http://crbug.com/154196
TEST_F(FullscreenControllerStateUnitTest, DISABLED_ToggleTabWhenPendingTab) {
  // Only possible without reentrancy.
  if (FullscreenControllerStateTest::IsWindowFullscreenStateChangedReentrant())
    return;
  AddTab(browser(), GURL(content::kAboutBlankURL));
  ASSERT_NO_FATAL_FAILURE(
      TransitionToState(STATE_TO_TAB_FULLSCREEN))
      << GetAndClearDebugLog();

  ASSERT_TRUE(InvokeEvent(TAB_FULLSCREEN_TRUE)) << GetAndClearDebugLog();
  ASSERT_TRUE(InvokeEvent(TAB_FULLSCREEN_FALSE)) << GetAndClearDebugLog();
  ASSERT_TRUE(InvokeEvent(WINDOW_CHANGE)) << GetAndClearDebugLog();
}

// Debugging utility: Display the transition tables. Intentionally disabled
TEST_F(FullscreenControllerStateUnitTest, DISABLED_DebugLogStateTables) {
  std::ostringstream output;
  output << "\n\nTransition Table:";
  output << GetTransitionTableAsString();

  output << "\n\nInitial transitions:";
  output << GetStateTransitionsAsString();

  // Calculate all transition pairs.
  for (int state1_int = 0; state1_int < NUM_STATES; ++state1_int) {
    State state1 = static_cast<State>(state1_int);
    for (int state2_int = 0; state2_int < NUM_STATES; ++state2_int) {
      State state2 = static_cast<State>(state2_int);
      if (ShouldSkipStateAndEventPair(state1, EVENT_INVALID) ||
          ShouldSkipStateAndEventPair(state2, EVENT_INVALID))
        continue;
      // Compute the transition
      if (NextTransitionInShortestPath(state1, state2, NUM_STATES).state ==
          STATE_INVALID) {
        LOG(ERROR) << "Should be skipping state transitions for: "
            << GetStateString(state1) << " " << GetStateString(state2);
      }
    }
  }

  output << "\n\nAll transitions:";
  output << GetStateTransitionsAsString();
  LOG(INFO) << output.str();
}

// Test that the fullscreen exit bubble is closed by
// WindowFullscreenStateChanged() if fullscreen is exited via BrowserWindow.
// This currently occurs when an extension exits fullscreen via changing the
// browser bounds.
TEST_F(FullscreenControllerStateUnitTest, ExitFullscreenViaBrowserWindow) {
  AddTab(browser(), GURL(content::kAboutBlankURL));
  ASSERT_TRUE(InvokeEvent(TOGGLE_FULLSCREEN));
  ASSERT_TRUE(InvokeEvent(WINDOW_CHANGE));
  ASSERT_TRUE(browser()->window()->IsFullscreen());
  // Exit fullscreen without going through fullscreen controller.
  browser()->window()->ExitFullscreen();
  ChangeWindowFullscreenState();
  EXPECT_EQ(FEB_TYPE_NONE,
            browser()->fullscreen_controller()->GetFullscreenExitBubbleType());
}

// Test that switching tabs takes the browser out of tab fullscreen.
TEST_F(FullscreenControllerStateUnitTest, ExitTabFullscreenViaSwitchingTab) {
  AddTab(browser(), GURL(content::kAboutBlankURL));
  AddTab(browser(), GURL(content::kAboutBlankURL));
  ASSERT_TRUE(InvokeEvent(TAB_FULLSCREEN_TRUE));
  ASSERT_TRUE(InvokeEvent(WINDOW_CHANGE));
  ASSERT_TRUE(browser()->window()->IsFullscreen());

  browser()->tab_strip_model()->SelectNextTab();
  ChangeWindowFullscreenState();
  EXPECT_FALSE(browser()->window()->IsFullscreen());
}

// Test that switching tabs via detaching the active tab (which is in tab
// fullscreen) takes the browser out of tab fullscreen. This case can
// occur if the user is in both tab fullscreen and immersive browser fullscreen.
TEST_F(FullscreenControllerStateUnitTest, ExitTabFullscreenViaDetachingTab) {
  AddTab(browser(), GURL(content::kAboutBlankURL));
  AddTab(browser(), GURL(content::kAboutBlankURL));
  ASSERT_TRUE(InvokeEvent(TAB_FULLSCREEN_TRUE));
  ASSERT_TRUE(InvokeEvent(WINDOW_CHANGE));
  ASSERT_TRUE(browser()->window()->IsFullscreen());

  scoped_ptr<content::WebContents> web_contents(
      browser()->tab_strip_model()->DetachWebContentsAt(0));
  ChangeWindowFullscreenState();
  EXPECT_FALSE(browser()->window()->IsFullscreen());
}

// Test that replacing the web contents for a tab which is in tab fullscreen
// takes the browser out of tab fullscreen. This can occur if the user
// navigates to a prerendered page from a page which is tab fullscreen.
TEST_F(FullscreenControllerStateUnitTest, ExitTabFullscreenViaReplacingTab) {
  AddTab(browser(), GURL(content::kAboutBlankURL));
  ASSERT_TRUE(InvokeEvent(TAB_FULLSCREEN_TRUE));
  ASSERT_TRUE(InvokeEvent(WINDOW_CHANGE));
  ASSERT_TRUE(browser()->window()->IsFullscreen());

  content::WebContents* new_web_contents = content::WebContents::Create(
      content::WebContents::CreateParams(profile()));
  scoped_ptr<content::WebContents> old_web_contents(
      browser()->tab_strip_model()->ReplaceWebContentsAt(
          0, new_web_contents));
  ChangeWindowFullscreenState();
  EXPECT_FALSE(browser()->window()->IsFullscreen());
}
