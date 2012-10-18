// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory.h>

#include <iomanip>
#include <iostream>
#include <sstream>

#include "build/build_config.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/fullscreen/fullscreen_controller.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

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
  return state_ == FULLSCREEN || state_ == TO_NORMAL;
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

// Unit test fixture for testing Fullscreen Controller. ------------------------
class FullscreenControllerUnitTest : public BrowserWithTestWindowTest {
 public:
  enum State {
    // The window is not in fullscreen.
    STATE_NORMAL,
    // User-initiated fullscreen. On Mac, this is Lion-mode for 10.7+. On 10.6,
    // this is synonymous with STATE_BROWSER_FULLSCREEN_WITH_CHROME.
    STATE_BROWSER_FULLSCREEN_NO_CHROME,
#if defined(OS_WIN)
    // Windows 8 Metro Snap mode, which puts the window at 20% screen-width.
    // No TO_ state for Metro, as the windows implementation is only reentrant.
    STATE_METRO_SNAP,
#endif
    // HTML5 tab-initiated fullscreen.
    STATE_TAB_FULLSCREEN,
    // Both tab and browser fullscreen.
    STATE_TAB_BROWSER_FULLSCREEN,
    // TO_ states are asynchronous states waiting for window state change
    // before transitioning to their named state.
    STATE_TO_NORMAL,
    STATE_TO_BROWSER_FULLSCREEN_NO_CHROME,
    STATE_TO_TAB_FULLSCREEN,
    NUM_STATES,
    STATE_INVALID,
  };

  enum Event {
    // FullscreenController::ToggleFullscreenMode()
    TOGGLE_FULLSCREEN,
    // FullscreenController::ToggleFullscreenModeForTab(, true)
    TAB_FULLSCREEN_TRUE,
    // FullscreenController::ToggleFullscreenModeForTab(, false)
    TAB_FULLSCREEN_FALSE,
#if defined(OS_WIN)
    // FullscreenController::SetMetroSnapMode(true)
    METRO_SNAP_TRUE,
    // FullscreenController::SetMetroSnapMode(flase)
    METRO_SNAP_FALSE,
#endif
    // FullscreenController::ChangeWindowFullscreenState()
    WINDOW_CHANGE,
    NUM_EVENTS,
    EVENT_INVALID,
  };

  static const int MAX_STATE_NAME_LENGTH = 37;
  static const int MAX_EVENT_NAME_LENGTH = 20;

  virtual void SetUp() OVERRIDE;

  static const char* GetStateString(State state);
  static const char* GetEventString(Event event);

  // Causes Fullscreen Controller to transition to an arbitrary state.
  void TransitionToState(State state);
  // Makes one state change to approach |destination_state| via shortest path.
  // Returns true if a state change is made.
  // Repeated calls are needed to reach the destination.
  bool TransitionAStepTowardState(State destination_state);

  // Causes the |event| to occur and return true on success.
  bool InvokeEvent(Event event);

  // Checks that window state matches the expected controller state.
  void VerifyWindowState();

 protected:
  // Generated information about the transitions between states.
  struct StateTransitionInfo {
    StateTransitionInfo()
        : event(EVENT_INVALID),
          state(STATE_INVALID),
          distance(NUM_STATES) {}
    Event event;  // The |Event| that will cause the state transition.
    State state;  // The adjacent |State| transitioned to; not the final state.
    int distance;  // Steps to final state. NUM_STATES represents unknown.
  };

  // Returns next transition info for shortest path from source to destination.
  StateTransitionInfo NextTransitionInShortestPath(State source,
                                                   State destination,
                                                   int search_limit);

  std::string GetAndClearDebugLog();

  // Avoids currently broken cases in the fullscreen controller.
  bool ShouldSkipStateAndEventPair(State state, Event event);
  // Skips reentrant situations and calls ShouldSkipStateAndEventPair.
  bool ShouldSkipTest(State state, Event event, bool reentrant);

  // Runs one test of transitioning to a state and invoking an event.
  void TestStateAndEvent(State state, Event event, bool reentrant);

  FullscreenControllerTestWindow* window_;
  FullscreenController* fullscreen_controller_;
  State state_;

  // Human defined |State| that results given each [state][event] pair.
  State transition_table_[NUM_STATES][NUM_EVENTS];

  // Generated information about the transitions between states [from][to].
  StateTransitionInfo state_transitions_[NUM_STATES][NUM_STATES];

  // Log of operations reported on errors via GetAndClearDebugLog().
  std::ostringstream debugging_log_;
};

void FullscreenControllerUnitTest::SetUp() {
  window_ = new FullscreenControllerTestWindow();
  set_window(window_);  // BrowserWithTestWindowTest takes ownership.
  BrowserWithTestWindowTest::SetUp();
  window_->set_browser(browser());
  fullscreen_controller_ = browser()->fullscreen_controller();
  state_ = STATE_NORMAL;

  // Human specified state machine data.
  // For each state, for each event, define the resulting state.
  State transition_table_data[][NUM_EVENTS] = {
    { // STATE_NORMAL:
      STATE_TO_BROWSER_FULLSCREEN_NO_CHROME,  // Event TOGGLE_FULLSCREEN
      STATE_TO_TAB_FULLSCREEN,                // Event TAB_FULLSCREEN_TRUE
      STATE_NORMAL,                           // Event TAB_FULLSCREEN_FALSE
#if defined(OS_WIN)
      STATE_METRO_SNAP,                       // Event METRO_SNAP_TRUE
      STATE_NORMAL,                           // Event METRO_SNAP_FALSE
#endif
      STATE_NORMAL,                           // Event WINDOW_CHANGE
    },
    { // STATE_BROWSER_FULLSCREEN_NO_CHROME:
      STATE_TO_NORMAL,                        // Event TOGGLE_FULLSCREEN
      STATE_TAB_BROWSER_FULLSCREEN,           // Event TAB_FULLSCREEN_TRUE
      STATE_BROWSER_FULLSCREEN_NO_CHROME,     // Event TAB_FULLSCREEN_FALSE
#if defined(OS_WIN)
      STATE_METRO_SNAP,                       // Event METRO_SNAP_TRUE
      STATE_BROWSER_FULLSCREEN_NO_CHROME,     // Event METRO_SNAP_FALSE
#endif
      STATE_BROWSER_FULLSCREEN_NO_CHROME,     // Event WINDOW_CHANGE
    },
#if defined(OS_WIN)
    { // STATE_METRO_SNAP:
      STATE_METRO_SNAP,                       // Event TOGGLE_FULLSCREEN
      STATE_METRO_SNAP,                       // Event TAB_FULLSCREEN_TRUE
      STATE_METRO_SNAP,                       // Event TAB_FULLSCREEN_FALSE
      STATE_METRO_SNAP,                       // Event METRO_SNAP_TRUE
      STATE_NORMAL,                           // Event METRO_SNAP_FALSE
      STATE_METRO_SNAP,                       // Event WINDOW_CHANGE
    },
#endif
    { // STATE_TAB_FULLSCREEN:
      STATE_TO_NORMAL,                        // Event TOGGLE_FULLSCREEN
      STATE_TAB_FULLSCREEN,                   // Event TAB_FULLSCREEN_TRUE
      STATE_TO_NORMAL,                        // Event TAB_FULLSCREEN_FALSE
#if defined(OS_WIN)
      STATE_METRO_SNAP,                       // Event METRO_SNAP_TRUE
      STATE_TAB_FULLSCREEN,                   // Event METRO_SNAP_FALSE
#endif
      STATE_TAB_FULLSCREEN,                   // Event WINDOW_CHANGE
    },
    { // STATE_TAB_BROWSER_FULLSCREEN:
      STATE_TO_NORMAL,                        // Event TOGGLE_FULLSCREEN
      STATE_TAB_BROWSER_FULLSCREEN,           // Event TAB_FULLSCREEN_TRUE
#if defined(OS_MACOSX)
      // TODO(scheib) Mac exits browser mode too http://crbug.com/155642
      STATE_TO_NORMAL,                        // Event TAB_FULLSCREEN_FALSE
#else
      STATE_BROWSER_FULLSCREEN_NO_CHROME,     // Event TAB_FULLSCREEN_FALSE
#endif
#if defined(OS_WIN)
      STATE_METRO_SNAP,                       // Event METRO_SNAP_TRUE
      STATE_TAB_BROWSER_FULLSCREEN,           // Event METRO_SNAP_FALSE
#endif
      STATE_TAB_BROWSER_FULLSCREEN,           // Event WINDOW_CHANGE
    },
    // STATE_TO_NORMAL:
    { STATE_TO_NORMAL,                        // Event TOGGLE_FULLSCREEN
      // TODO(scheib) Should be a route back to TAB. http://crbug.com/154196
      STATE_TO_NORMAL,                        // Event TAB_FULLSCREEN_TRUE
      STATE_TO_NORMAL,                        // Event TAB_FULLSCREEN_FALSE
#if defined(OS_WIN)
      STATE_METRO_SNAP,                       // Event METRO_SNAP_TRUE
      STATE_TO_NORMAL,                        // Event METRO_SNAP_FALSE
#endif
      STATE_NORMAL,                           // Event WINDOW_CHANGE
    },
    // STATE_TO_BROWSER_FULLSCREEN_NO_CHROME:
    { STATE_TO_BROWSER_FULLSCREEN_NO_CHROME,  // Event TOGGLE_FULLSCREEN
      // TODO(scheib) Should be a route to TAB_BROWSER http://crbug.com/154196
      STATE_TO_BROWSER_FULLSCREEN_NO_CHROME,  // Event TAB_FULLSCREEN_TRUE
      STATE_TO_BROWSER_FULLSCREEN_NO_CHROME,  // Event TAB_FULLSCREEN_FALSE
#if defined(OS_WIN)
      STATE_METRO_SNAP,                       // Event METRO_SNAP_TRUE
      STATE_TO_BROWSER_FULLSCREEN_NO_CHROME,  // Event METRO_SNAP_FALSE
#endif
      STATE_BROWSER_FULLSCREEN_NO_CHROME,     // Event WINDOW_CHANGE
    },
    // STATE_TO_TAB_FULLSCREEN:
    { // TODO(scheib) Should be a route to TAB_BROWSER http://crbug.com/154196
      STATE_TO_TAB_FULLSCREEN,                // Event TOGGLE_FULLSCREEN
      STATE_TO_TAB_FULLSCREEN,                // Event TAB_FULLSCREEN_TRUE
      // TODO(scheib) Should be a route back to NORMAL. http://crbug.com/154196
      STATE_TO_BROWSER_FULLSCREEN_NO_CHROME,  // Event TAB_FULLSCREEN_FALSE
#if defined(OS_WIN)
      STATE_METRO_SNAP,                       // Event METRO_SNAP_TRUE
      STATE_TO_TAB_FULLSCREEN,                // Event METRO_SNAP_FALSE
#endif
      STATE_TAB_FULLSCREEN,                   // Event WINDOW_CHANGE
    },
  };
  ASSERT_EQ(sizeof(transition_table_data), sizeof(transition_table_));
  memcpy(transition_table_, transition_table_data,
         sizeof(transition_table_data));

  // Verify that transition_table_ has been completely defined.
  for (int source = 0; source < NUM_STATES; source++) {
    for (int event = 0; event < NUM_EVENTS; event++) {
      ASSERT_NE(STATE_INVALID, transition_table_[source][event]);
      ASSERT_LE(0, transition_table_[source][event]);
      ASSERT_GT(NUM_STATES, transition_table_[source][event]);
    }
  }

  // Copy transition_table_ data into state_transitions_ table.
  for (int source = 0; source < NUM_STATES; source++) {
    for (int event = 0; event < NUM_EVENTS; event++) {
      State destination = transition_table_[source][event];
      state_transitions_[source][destination].event = static_cast<Event>(event);
      state_transitions_[source][destination].state = destination;
      state_transitions_[source][destination].distance = 1;
    }
  }
}

// static
const char* FullscreenControllerUnitTest::GetStateString(State state) {
  switch (state) {
    case STATE_NORMAL:
      return "STATE_NORMAL";
    case STATE_BROWSER_FULLSCREEN_NO_CHROME:
      return "STATE_BROWSER_FULLSCREEN_NO_CHROME";
#if defined(OS_WIN)
    case STATE_METRO_SNAP:
      return "STATE_METRO_SNAP";
#endif
    case STATE_TAB_FULLSCREEN:
      return "STATE_TAB_FULLSCREEN";
    case STATE_TAB_BROWSER_FULLSCREEN:
      return "STATE_TAB_BROWSER_FULLSCREEN";
    case STATE_TO_NORMAL:
      return "STATE_TO_NORMAL";
    case STATE_TO_BROWSER_FULLSCREEN_NO_CHROME:
      return "STATE_TO_BROWSER_FULLSCREEN_NO_CHROME";
    case STATE_TO_TAB_FULLSCREEN:
      return "STATE_TO_TAB_FULLSCREEN";
    case STATE_INVALID:
      return "STATE_INVALID";
    default:
      NOTREACHED() << "No string for state " << state;
      return "State-Unknown";
  }
}

// static
const char* FullscreenControllerUnitTest::GetEventString(Event event) {
  switch (event) {
    case TOGGLE_FULLSCREEN:
      return "TOGGLE_FULLSCREEN";
    case TAB_FULLSCREEN_TRUE:
      return "TAB_FULLSCREEN_TRUE";
    case TAB_FULLSCREEN_FALSE:
      return "TAB_FULLSCREEN_FALSE";
#if defined(OS_WIN)
    case METRO_SNAP_TRUE:
      return "METRO_SNAP_TRUE";
    case METRO_SNAP_FALSE:
      return "METRO_SNAP_FALSE";
#endif
    case WINDOW_CHANGE:
      return "WINDOW_CHANGE";
    case EVENT_INVALID:
      return "EVENT_INVALID";
    default:
      NOTREACHED() << "No string for event " << event;
      return "Event-Unknown";
  }
}

void FullscreenControllerUnitTest::TransitionToState(State final_state) {
  int max_steps = NUM_STATES;
  while (max_steps-- && TransitionAStepTowardState(final_state))
    continue;
  ASSERT_GE(max_steps, 0) << "TransitionToState was unable to achieve desired "
      << "target state. TransitionAStepTowardState iterated too many times."
      << GetAndClearDebugLog();
  ASSERT_EQ(final_state, state_) << "TransitionToState was unable to achieve "
      << "desired target state. TransitionAStepTowardState returned false."
      << GetAndClearDebugLog();
}

bool FullscreenControllerUnitTest::TransitionAStepTowardState(
    State destination_state) {
  State source_state = state_;
  if (source_state == destination_state)
    return false;

  StateTransitionInfo next = NextTransitionInShortestPath(source_state,
                                                          destination_state,
                                                          NUM_STATES);
  if (next.state == STATE_INVALID) {
    NOTREACHED() << "TransitionAStepTowardState unable to transition. "
        << "NextTransitionInShortestPath("
        << GetStateString(source_state) << ", "
        << GetStateString(destination_state) << ") returned STATE_INVALID."
        << GetAndClearDebugLog();
    return false;
  }

  return InvokeEvent(next.event);
}

bool FullscreenControllerUnitTest::InvokeEvent(Event event) {
  State source_state = state_;
  State next_state = transition_table_[source_state][event];

  // When simulating reentrant window change calls, expect the next state
  // automatically.
  if (window_->reentrant())
    next_state = transition_table_[next_state][WINDOW_CHANGE];

  debugging_log_ << "  InvokeEvent(" << std::left
      << std::setw(MAX_EVENT_NAME_LENGTH) << GetEventString(event)
      << ") to "
      << std::setw(MAX_STATE_NAME_LENGTH) << GetStateString(next_state)
      << " ";

  state_ = next_state;

  switch (event) {
    case TOGGLE_FULLSCREEN:
      fullscreen_controller_->ToggleFullscreenMode();
      break;
    case TAB_FULLSCREEN_TRUE:
      fullscreen_controller_->ToggleFullscreenModeForTab(
           chrome::GetActiveWebContents(browser()), true);
      break;
    case TAB_FULLSCREEN_FALSE:
      fullscreen_controller_->ToggleFullscreenModeForTab(
           chrome::GetActiveWebContents(browser()), false);
      break;
#if defined(OS_WIN)
    case METRO_SNAP_TRUE:
      fullscreen_controller_->SetMetroSnapMode(true);
      break;
    case METRO_SNAP_FALSE:
      fullscreen_controller_->SetMetroSnapMode(false);
      break;
#endif
    case WINDOW_CHANGE:
      window_->ChangeWindowFullscreenState();
      break;
    default:
      NOTREACHED() << "InvokeEvent needs a handler for event "
          << GetEventString(event) << GetAndClearDebugLog();
      return false;
  }

  debugging_log_ << "Window state now "
      << FullscreenControllerTestWindow::GetWindowStateString(window_->state())
      << std::endl;

  VerifyWindowState();

  return true;
}

void FullscreenControllerUnitTest::VerifyWindowState() {
  switch (state_) {
    case STATE_NORMAL:
      EXPECT_EQ(FullscreenControllerTestWindow::NORMAL,
                window_->state()) << GetAndClearDebugLog();
#if defined(OS_MACOSX)
      EXPECT_FALSE(window_->InPresentationMode())
          << GetAndClearDebugLog();
#endif
      EXPECT_FALSE(fullscreen_controller_->IsFullscreenForBrowser())
          << GetAndClearDebugLog();
      EXPECT_FALSE(fullscreen_controller_->IsFullscreenForTabOrPending())
          << GetAndClearDebugLog();
      EXPECT_FALSE(fullscreen_controller_->IsInMetroSnapMode())
          << GetAndClearDebugLog();
      break;
    case STATE_BROWSER_FULLSCREEN_NO_CHROME:
      EXPECT_EQ(FullscreenControllerTestWindow::FULLSCREEN,
                window_->state()) << GetAndClearDebugLog();
#if defined(OS_MACOSX)
      EXPECT_FALSE(window_->InPresentationMode())
          << GetAndClearDebugLog();
#endif
      EXPECT_TRUE(fullscreen_controller_->IsFullscreenForBrowser())
          << GetAndClearDebugLog();
      EXPECT_FALSE(fullscreen_controller_->IsFullscreenForTabOrPending())
          << GetAndClearDebugLog();
      EXPECT_FALSE(fullscreen_controller_->IsInMetroSnapMode())
          << GetAndClearDebugLog();
      break;
#if defined(OS_WIN)
    case STATE_METRO_SNAP:
      EXPECT_EQ(FullscreenControllerTestWindow::METRO_SNAP,
                window_->state()) << GetAndClearDebugLog();
      // No expectation for InPresentationMode.
      EXPECT_FALSE(fullscreen_controller_->IsFullscreenForBrowser())
          << GetAndClearDebugLog();
      EXPECT_FALSE(fullscreen_controller_->IsFullscreenForTabOrPending())
          << GetAndClearDebugLog();
      EXPECT_TRUE(fullscreen_controller_->IsInMetroSnapMode())
          << GetAndClearDebugLog();
      break;
#endif
    case STATE_TAB_FULLSCREEN:
      EXPECT_EQ(FullscreenControllerTestWindow::FULLSCREEN,
                window_->state()) << GetAndClearDebugLog();
#if defined(OS_MACOSX)
      EXPECT_TRUE(window_->InPresentationMode())
          << GetAndClearDebugLog();
#endif
      EXPECT_FALSE(fullscreen_controller_->IsFullscreenForBrowser())
          << GetAndClearDebugLog();
      EXPECT_TRUE(fullscreen_controller_->IsFullscreenForTabOrPending())
          << GetAndClearDebugLog();
      EXPECT_FALSE(fullscreen_controller_->IsInMetroSnapMode())
          << GetAndClearDebugLog();
      break;
    case STATE_TAB_BROWSER_FULLSCREEN:
      EXPECT_EQ(FullscreenControllerTestWindow::FULLSCREEN,
                window_->state()) << GetAndClearDebugLog();
#if defined(OS_MACOSX)
      EXPECT_TRUE(window_->InPresentationMode())
          << GetAndClearDebugLog();
#endif
#if defined(OS_MACOSX)
      // TODO(scheib) Mac is reporting incorrect IsFullscreenForBrowser().
      // e.g. in FullscreenControllerUnitTest.
      //     STATE_BROWSER_FULLSCREEN_NO_CHROME_TAB_FULLSCREEN_TRUE
      // At the end of ToggleFullscreenModeForTab
      // tab_caused_fullscreen_ has incorrectly been set to true even
      // though controller was already in browser fullscreen.
      // http://crbug.com/155650
      EXPECT_FALSE(fullscreen_controller_->IsFullscreenForBrowser())
          << GetAndClearDebugLog();
#else
      EXPECT_TRUE(fullscreen_controller_->IsFullscreenForBrowser())
          << GetAndClearDebugLog();
#endif
      EXPECT_TRUE(fullscreen_controller_->IsFullscreenForTabOrPending())
          << GetAndClearDebugLog();
      EXPECT_FALSE(fullscreen_controller_->IsInMetroSnapMode())
          << GetAndClearDebugLog();
      break;
    case STATE_TO_NORMAL:
      EXPECT_EQ(FullscreenControllerTestWindow::TO_NORMAL,
                window_->state()) << GetAndClearDebugLog();
#if defined(OS_MACOSX)
      EXPECT_FALSE(window_->InPresentationMode())
          << GetAndClearDebugLog();
#endif
      // No expectation for IsFullscreenForBrowser.
      // No expectation for IsFullscreenForTabOrPending.
      EXPECT_FALSE(fullscreen_controller_->IsInMetroSnapMode())
          << GetAndClearDebugLog();
      break;
    case STATE_TO_BROWSER_FULLSCREEN_NO_CHROME:
      EXPECT_EQ(FullscreenControllerTestWindow::TO_FULLSCREEN,
                window_->state()) << GetAndClearDebugLog();
#if defined(OS_MACOSX)
      EXPECT_FALSE(window_->InPresentationMode())
          << GetAndClearDebugLog();
#endif
      EXPECT_FALSE(fullscreen_controller_->IsFullscreenForBrowser())
          << GetAndClearDebugLog();
      // No expectation for IsFullscreenForTabOrPending.
      EXPECT_FALSE(fullscreen_controller_->IsInMetroSnapMode())
          << GetAndClearDebugLog();
      break;
    case STATE_TO_TAB_FULLSCREEN:
      EXPECT_EQ(FullscreenControllerTestWindow::TO_FULLSCREEN,
                window_->state()) << GetAndClearDebugLog();
#if defined(OS_MACOSX)
      EXPECT_TRUE(window_->InPresentationMode())
          << GetAndClearDebugLog();
#endif
      EXPECT_FALSE(fullscreen_controller_->IsFullscreenForBrowser())
          << GetAndClearDebugLog();
      EXPECT_TRUE(fullscreen_controller_->IsFullscreenForTabOrPending())
          << GetAndClearDebugLog();
      EXPECT_FALSE(fullscreen_controller_->IsInMetroSnapMode())
          << GetAndClearDebugLog();
      break;
    default:
      NOTREACHED() << GetAndClearDebugLog();
  }
}

FullscreenControllerUnitTest::StateTransitionInfo
    FullscreenControllerUnitTest::NextTransitionInShortestPath(
        State source, State destination, int search_limit) {
  if (search_limit == 0)
    return StateTransitionInfo();  // Return a default (invalid) state.

  if (state_transitions_[source][destination].state == STATE_INVALID) {
    // Don't know the next state yet, do a depth first search.
    StateTransitionInfo result;

    // Consider all states reachable via each event from the source state.
    for (int event_int = 0; event_int < NUM_EVENTS; event_int++) {
      Event event = static_cast<Event>(event_int);
      State next_state_candidate = transition_table_[source][event];

      if (ShouldSkipStateAndEventPair(source, event))
        continue;

      // Recurse.
      StateTransitionInfo candidate = NextTransitionInShortestPath(
          next_state_candidate, destination, search_limit - 1);

      if (candidate.distance + 1 < result.distance) {
        result.event = event;
        result.state = next_state_candidate;
        result.distance = candidate.distance + 1;
      }
    }

    // Cache result so that a search is not required next time.
    state_transitions_[source][destination] = result;
  }

  return state_transitions_[source][destination];
}

std::string FullscreenControllerUnitTest::GetAndClearDebugLog() {
  debugging_log_ << "(end of log)\n";
  std::string output_log = "\nDebugging Log:\n" + debugging_log_.str();
  debugging_log_.str("");
  return output_log;
}

bool FullscreenControllerUnitTest::ShouldSkipStateAndEventPair(State state,
                                                               Event event) {
  // TODO(scheib) Toggling Tab fullscreen while pending Tab or
  // Browser fullscreen is broken currently http://crbug.com/154196
  if ((state == STATE_TO_BROWSER_FULLSCREEN_NO_CHROME ||
       state == STATE_TO_TAB_FULLSCREEN) &&
      (event == TAB_FULLSCREEN_TRUE || event == TAB_FULLSCREEN_FALSE))
    return true;
  if (state == STATE_TO_NORMAL && event == TAB_FULLSCREEN_TRUE)
    return true;

  return false;
}

bool FullscreenControllerUnitTest::ShouldSkipTest(State state,
                                                  Event event,
                                                  bool reentrant) {
#if defined(OS_WIN)
  // FullscreenController verifies that WindowFullscreenStateChanged is
  // always reentrant on Windows. It will fail if we mock asynchronous calls.
  if (!reentrant) {
    debugging_log_ << "\nSkipping non-reentrant test on Windows.\n";
    return true;
  }
#endif

  // When testing reentrancy there are states the fullscreen controller
  // will be unable to remain in, as they will progress due to the
  // reentrant window change call. Skip states that will be instantly
  // exited by the reentrant call.
  if (reentrant && (transition_table_[state][WINDOW_CHANGE] != state)) {
    debugging_log_ << "\nSkipping reentrant test for transitory source state "
        << GetStateString(state) << ".\n";
    return true;
  }

  return ShouldSkipStateAndEventPair(state, event);
}

void FullscreenControllerUnitTest::TestStateAndEvent(State state,
                                                     Event event,
                                                     bool reentrant) {
  if (ShouldSkipTest(state, event, reentrant))
    return;

  debugging_log_ << "\nTest transition from state "
      << GetStateString(state)
      << (reentrant ? " with reentrant calls.\n" : ".\n");
  window_->set_reentrant(reentrant);

  debugging_log_ << " First,                           from "
      << GetStateString(state_) << "\n";
  ASSERT_NO_FATAL_FAILURE(TransitionToState(state))
      << GetAndClearDebugLog();

  debugging_log_ << " Then,\n";
  ASSERT_TRUE(InvokeEvent(event)) << GetAndClearDebugLog();
}


// Tests -----------------------------------------------------------------------

#define TEST_EVENT_INNER(state, event, reentrant, reentrant_id) \
    TEST_F(FullscreenControllerUnitTest, state##_##event##reentrant_id) { \
      AddTab(browser(), GURL(chrome::kAboutBlankURL)); \
      ASSERT_NO_FATAL_FAILURE(TestStateAndEvent(state, event, reentrant)) \
          << GetAndClearDebugLog(); \
    }
    // Progress of tests can be examined by inserting the following line:
    // LOG(INFO) << GetAndClearDebugLog(); }

#define TEST_EVENT(state, event) \
    TEST_EVENT_INNER(state, event, false, ); \
    TEST_EVENT_INNER(state, event, true, _Reentrant);

// Individual tests for each pair of state and event:

TEST_EVENT(STATE_NORMAL, TOGGLE_FULLSCREEN);
TEST_EVENT(STATE_NORMAL, TAB_FULLSCREEN_TRUE);
TEST_EVENT(STATE_NORMAL, TAB_FULLSCREEN_FALSE);
#if defined(OS_WIN)
TEST_EVENT(STATE_NORMAL, METRO_SNAP_TRUE);
TEST_EVENT(STATE_NORMAL, METRO_SNAP_FALSE);
#endif
TEST_EVENT(STATE_NORMAL, WINDOW_CHANGE);

TEST_EVENT(STATE_BROWSER_FULLSCREEN_NO_CHROME, TOGGLE_FULLSCREEN);
TEST_EVENT(STATE_BROWSER_FULLSCREEN_NO_CHROME, TAB_FULLSCREEN_TRUE);
TEST_EVENT(STATE_BROWSER_FULLSCREEN_NO_CHROME, TAB_FULLSCREEN_FALSE);
#if defined(OS_WIN)
TEST_EVENT(STATE_BROWSER_FULLSCREEN_NO_CHROME, METRO_SNAP_TRUE);
TEST_EVENT(STATE_BROWSER_FULLSCREEN_NO_CHROME, METRO_SNAP_FALSE);
#endif
TEST_EVENT(STATE_BROWSER_FULLSCREEN_NO_CHROME, WINDOW_CHANGE);

#if defined(OS_WIN)
TEST_EVENT(STATE_METRO_SNAP, TOGGLE_FULLSCREEN);
TEST_EVENT(STATE_METRO_SNAP, TAB_FULLSCREEN_TRUE);
TEST_EVENT(STATE_METRO_SNAP, TAB_FULLSCREEN_FALSE);
TEST_EVENT(STATE_METRO_SNAP, METRO_SNAP_TRUE);
TEST_EVENT(STATE_METRO_SNAP, METRO_SNAP_FALSE);
TEST_EVENT(STATE_METRO_SNAP, WINDOW_CHANGE);
#endif

TEST_EVENT(STATE_TAB_FULLSCREEN, TOGGLE_FULLSCREEN);
TEST_EVENT(STATE_TAB_FULLSCREEN, TAB_FULLSCREEN_TRUE);
TEST_EVENT(STATE_TAB_FULLSCREEN, TAB_FULLSCREEN_FALSE);
#if defined(OS_WIN)
TEST_EVENT(STATE_TAB_FULLSCREEN, METRO_SNAP_TRUE);
TEST_EVENT(STATE_TAB_FULLSCREEN, METRO_SNAP_FALSE);
#endif
TEST_EVENT(STATE_TAB_FULLSCREEN, WINDOW_CHANGE);

TEST_EVENT(STATE_TAB_BROWSER_FULLSCREEN, TOGGLE_FULLSCREEN);
TEST_EVENT(STATE_TAB_BROWSER_FULLSCREEN, TAB_FULLSCREEN_TRUE);
TEST_EVENT(STATE_TAB_BROWSER_FULLSCREEN, TAB_FULLSCREEN_FALSE);
#if defined(OS_WIN)
TEST_EVENT(STATE_TAB_BROWSER_FULLSCREEN, METRO_SNAP_TRUE);
TEST_EVENT(STATE_TAB_BROWSER_FULLSCREEN, METRO_SNAP_FALSE);
#endif
TEST_EVENT(STATE_TAB_BROWSER_FULLSCREEN, WINDOW_CHANGE);

TEST_EVENT(STATE_TO_NORMAL, TOGGLE_FULLSCREEN);
TEST_EVENT(STATE_TO_NORMAL, TAB_FULLSCREEN_TRUE);
TEST_EVENT(STATE_TO_NORMAL, TAB_FULLSCREEN_FALSE);
#if defined(OS_WIN)
TEST_EVENT(STATE_TO_NORMAL, METRO_SNAP_TRUE);
TEST_EVENT(STATE_TO_NORMAL, METRO_SNAP_FALSE);
#endif
TEST_EVENT(STATE_TO_NORMAL, WINDOW_CHANGE);

TEST_EVENT(STATE_TO_BROWSER_FULLSCREEN_NO_CHROME, TOGGLE_FULLSCREEN);
TEST_EVENT(STATE_TO_BROWSER_FULLSCREEN_NO_CHROME, TAB_FULLSCREEN_TRUE);
TEST_EVENT(STATE_TO_BROWSER_FULLSCREEN_NO_CHROME, TAB_FULLSCREEN_FALSE);
#if defined(OS_WIN)
TEST_EVENT(STATE_TO_BROWSER_FULLSCREEN_NO_CHROME, METRO_SNAP_TRUE);
TEST_EVENT(STATE_TO_BROWSER_FULLSCREEN_NO_CHROME, METRO_SNAP_FALSE);
#endif
TEST_EVENT(STATE_TO_BROWSER_FULLSCREEN_NO_CHROME, WINDOW_CHANGE);

TEST_EVENT(STATE_TO_TAB_FULLSCREEN, TOGGLE_FULLSCREEN);
TEST_EVENT(STATE_TO_TAB_FULLSCREEN, TAB_FULLSCREEN_TRUE);
TEST_EVENT(STATE_TO_TAB_FULLSCREEN, TAB_FULLSCREEN_FALSE);
#if defined(OS_WIN)
TEST_EVENT(STATE_TO_TAB_FULLSCREEN, METRO_SNAP_TRUE);
TEST_EVENT(STATE_TO_TAB_FULLSCREEN, METRO_SNAP_FALSE);
#endif
TEST_EVENT(STATE_TO_TAB_FULLSCREEN, WINDOW_CHANGE);

// Specific one-off tests for known issues:

// TODO(scheib) Toggling Tab fullscreen while pending Tab or
// Browser fullscreen is broken currently http://crbug.com/154196
TEST_F(FullscreenControllerUnitTest, DISABLED_ToggleTabWhenPendingBrowser) {
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
TEST_F(FullscreenControllerUnitTest, DISABLED_ToggleTabWhenPendingTab) {
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

// Soak tests:

// Tests all states with all permutations of multiple events to detect lingering
// state issues that would bleed over to other states.
// I.E. for each state test all combinations of events E1, E2, E3.
//
// This produces coverage for event sequences that may happen normally but
// would not be exposed by traversing to each state via TransitionToState().
// TransitionToState() always takes the same path even when multiple paths
// exist.
TEST_F(FullscreenControllerUnitTest, TransitionsForEachState) {
  // A tab is needed for tab fullscreen.
  AddTab(browser(), GURL(chrome::kAboutBlankURL));
  for (int reentrant = 0; reentrant <= 1; reentrant++) {
    for (int source_int = 0; source_int < NUM_STATES; source_int++) {
      for (int event1_int = 0; event1_int < NUM_EVENTS; event1_int++) {
        State state = static_cast<State>(source_int);
        Event event1 = static_cast<Event>(event1_int);

        // Early out if skipping all tests for this state, reduces log noise.
        if (ShouldSkipTest(state, event1, !!reentrant))
          continue;

        for (int event2_int = 0; event2_int < NUM_EVENTS; event2_int++) {
          for (int event3_int = 0; event3_int < NUM_EVENTS; event3_int++) {
            Event event2 = static_cast<Event>(event2_int);
            Event event3 = static_cast<Event>(event3_int);

            // Test each state and each event.
            ASSERT_NO_FATAL_FAILURE(TestStateAndEvent(state,
                                                      event1,
                                                      !!reentrant))
                << GetAndClearDebugLog();

            // Then, add an additional event to the sequence.
            if (ShouldSkipStateAndEventPair(state_, event2))
              continue;
            ASSERT_TRUE(InvokeEvent(event2)) << GetAndClearDebugLog();

            // Then, add an additional event to the sequence.
            if (ShouldSkipStateAndEventPair(state_, event3))
              continue;
            ASSERT_TRUE(InvokeEvent(event3)) << GetAndClearDebugLog();
          }
        }
      }
    }
  }
  // Progress of test can be examined via LOG(INFO) << GetAndClearDebugLog();
}

