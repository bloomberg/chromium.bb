// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory.h>

#include <sstream>

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/fullscreen/fullscreen_controller.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "content/public/browser/web_contents.h"
#include "testing/gtest/include/gtest/gtest.h"

// A BrowserWindow used for testing FullscreenController. ----------------------
class FullscreenControllerTestWindow : public TestBrowserWindow {
 public:
  // Simulate the window state with an enumeration.
  enum WindowState {
    NORMAL,
    FULLSCREEN,
    TO_NORMAL,
    TO_FULLSCREEN
  };

  FullscreenControllerTestWindow();
  virtual ~FullscreenControllerTestWindow() {}

  // BrowserWindow Interface:
  virtual void EnterFullscreen(const GURL& url,
                               FullscreenExitBubbleType type) OVERRIDE;
  virtual void ExitFullscreen() OVERRIDE;
  virtual bool IsFullscreen() const OVERRIDE;
#if defined(OS_WIN)
  virtual void SetMetroSnapMode(bool enable) OVERRIDE;
  virtual bool IsInMetroSnapMode() const OVERRIDE;
#endif

  static const char* GetWindowStateString(WindowState state);
  WindowState state() const { return state_; }
  void set_browser(Browser* browser) { browser_ = browser; }

  // Simulates the window changing state.
  void ChangeWindowFullscreenState();

 private:
  WindowState state_;
  Browser* browser_;
};

FullscreenControllerTestWindow::FullscreenControllerTestWindow()
    : state_(NORMAL),
      browser_(NULL) {
}

void FullscreenControllerTestWindow::EnterFullscreen(
    const GURL& url, FullscreenExitBubbleType type) {
  if (!IsFullscreen())
    state_ = TO_FULLSCREEN;
}

void FullscreenControllerTestWindow::ExitFullscreen() {
  if (IsFullscreen())
    state_ = TO_NORMAL;
}

bool FullscreenControllerTestWindow::IsFullscreen() const {
  return state_ == FULLSCREEN || state_ == TO_NORMAL;
}

#if defined(OS_WIN)
void FullscreenControllerTestWindow::SetMetroSnapMode(bool enable) {
  NOTREACHED(); // TODO(scheib) Determine how to model Metro Snap.
}

bool FullscreenControllerTestWindow::IsInMetroSnapMode() const {
  return false; // TODO(scheib) Determine how to model Metro Snap.
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


// Unit test fixture for testing Fullscreen Controller. ------------------------
class FullscreenControllerUnitTest : public BrowserWithTestWindowTest {
 public:
  enum State {
    // The window is not in fullscreen.
    STATE_NORMAL,
    // User-initiated fullscreen. On Mac, this is Lion-mode for 10.7+. On 10.6,
    // this is synonymous with STATE_BROWSER_FULLSCREEN_WITH_CHROME.
    STATE_BROWSER_FULLSCREEN_NO_CHROME,
    // TO_ states are asynchronous states waiting for window state change
    // before transitioning to their named state.
    STATE_TO_NORMAL,
    STATE_TO_BROWSER_FULLSCREEN_NO_CHROME,
    NUM_STATES,
    STATE_INVALID
  };

  enum Event {
    // FullscreenController::ToggleFullscreenMode()
    TOGGLE_FULLSCREEN,
    // FullscreenController::ChangeWindowFullscreenState()
    WINDOW_CHANGE,
    NUM_EVENTS,
    EVENT_INVALID
  };

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
  State transition_table_data[NUM_STATES][NUM_EVENTS] = {
    // STATE_NORMAL:
    { STATE_TO_BROWSER_FULLSCREEN_NO_CHROME,  // Event TOGGLE_FULLSCREEN
      STATE_NORMAL },                         // Event WINDOW_CHANGE
    // STATE_BROWSER_FULLSCREEN_NO_CHROME:
    { STATE_TO_NORMAL,                        // Event TOGGLE_FULLSCREEN
      STATE_BROWSER_FULLSCREEN_NO_CHROME },   // Event WINDOW_CHANGE
    // STATE_TO_NORMAL:
    { STATE_TO_NORMAL,                        // Event TOGGLE_FULLSCREEN
      STATE_NORMAL },                         // Event WINDOW_CHANGE
    // STATE_TO_BROWSER_FULLSCREEN_NO_CHROME:
    { STATE_TO_BROWSER_FULLSCREEN_NO_CHROME,  // Event TOGGLE_FULLSCREEN
      STATE_BROWSER_FULLSCREEN_NO_CHROME },   // Event WINDOW_CHANGE
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
    case STATE_TO_NORMAL:
      return "STATE_TO_NORMAL";
    case STATE_TO_BROWSER_FULLSCREEN_NO_CHROME:
      return "STATE_TO_BROWSER_FULLSCREEN_NO_CHROME";
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

  debugging_log_ << "  Transitioning state " << GetStateString(source_state)
      << " -> " << GetStateString(next_state) << " with event "
      << GetEventString(event) << std::endl;

  switch (event) {
    case TOGGLE_FULLSCREEN:
      fullscreen_controller_->ToggleFullscreenMode();
      break;
    case WINDOW_CHANGE:
      window_->ChangeWindowFullscreenState();
      break;
    default:
      NOTREACHED() << "InvokeEvent needs a handler for event "
          << GetEventString(event) << GetAndClearDebugLog();
      return false;
  }

  state_ = next_state;

  VerifyWindowState();

  debugging_log_ << "   Window state now "
      << FullscreenControllerTestWindow::GetWindowStateString(window_->state())
      << std::endl;

  return true;
}

void FullscreenControllerUnitTest::VerifyWindowState() {
  switch (state_) {
    case STATE_NORMAL:
      EXPECT_EQ(FullscreenControllerTestWindow::NORMAL,
                window_->state()) << GetAndClearDebugLog();
      break;
    case STATE_BROWSER_FULLSCREEN_NO_CHROME:
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
    for (int event = 0; event < NUM_EVENTS; event++) {
      State next_state_candidate = transition_table_[source][event];

      // Recurse.
      StateTransitionInfo candidate = NextTransitionInShortestPath(
          next_state_candidate, destination, search_limit - 1);

      if (candidate.distance + 1 < result.distance) {
        result.event = static_cast<Event>(event);
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

// Tests -----------------------------------------------------------------------

TEST_F(FullscreenControllerUnitTest, TransitionsForEachState) {
  for (int source_int = 0; source_int < NUM_STATES; source_int++) {
    for (int event_int = 0; event_int < NUM_EVENTS; event_int++) {
      State state = static_cast<State>(source_int);
      Event event = static_cast<Event>(event_int);

      debugging_log_ << "\nTest transition from state " << GetStateString(state)
          << " via event " << GetEventString(event) << std::endl;

      debugging_log_ << " First, transition to " << GetStateString(state)
          << std::endl;
      ASSERT_NO_FATAL_FAILURE(TransitionToState(state))
          << GetAndClearDebugLog();

      debugging_log_ << " Then, invoke " << GetEventString(event) << "\n";
      ASSERT_TRUE(InvokeEvent(event)) << GetAndClearDebugLog();
    }
  }
  // Progress of test can be examined via LOG(INFO) << GetAndClearDebugLog();
}

