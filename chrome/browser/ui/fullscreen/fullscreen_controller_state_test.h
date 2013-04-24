// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_FULLSCREEN_FULLSCREEN_CONTROLLER_STATE_TEST_H_
#define CHROME_BROWSER_UI_FULLSCREEN_FULLSCREEN_CONTROLLER_STATE_TEST_H_

#include <sstream>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "build/build_config.h"

class Browser;
class FullscreenController;
class FullscreenNotificationObserver;

// Test fixture testing Fullscreen Controller through its states. --------------
class FullscreenControllerStateTest {
 public:
  enum State {
    // The window is not in fullscreen.
    STATE_NORMAL,
    // User-initiated fullscreen.
    STATE_BROWSER_FULLSCREEN_NO_CHROME,
    // Mac User-initiated 'Lion Fullscreen' with browser chrome. OSX 10.7+ only.
    STATE_BROWSER_FULLSCREEN_WITH_CHROME,
    // Windows 8 Metro Snap mode, which puts the window at 20% screen-width.
    // No TO_ state for Metro, as the windows implementation is only reentrant.
    STATE_METRO_SNAP,
    // HTML5 tab-initiated fullscreen.
    STATE_TAB_FULLSCREEN,
    // Both tab and browser fullscreen.
    STATE_TAB_BROWSER_FULLSCREEN,
    // Both tab and browser fullscreen, displayed without chrome, but exits tab
    // fullscreen to STATE_BROWSER_FULLSCREEN_WITH_CHROME.
    STATE_TAB_BROWSER_FULLSCREEN_CHROME,
    // TO_ states are asynchronous states waiting for window state change
    // before transitioning to their named state.
    STATE_TO_NORMAL,
    STATE_TO_BROWSER_FULLSCREEN_NO_CHROME,
    STATE_TO_BROWSER_FULLSCREEN_WITH_CHROME,
    STATE_TO_TAB_FULLSCREEN,
    NUM_STATES,
    STATE_INVALID,
  };

  enum Event {
    // FullscreenController::ToggleFullscreenMode()
    TOGGLE_FULLSCREEN,
    // FullscreenController::ToggleFullscreenWithChrome()
    TOGGLE_FULLSCREEN_CHROME,
    // FullscreenController::ToggleFullscreenModeForTab(, true)
    TAB_FULLSCREEN_TRUE,
    // FullscreenController::ToggleFullscreenModeForTab(, false)
    TAB_FULLSCREEN_FALSE,
    // FullscreenController::SetMetroSnapMode(true)
    METRO_SNAP_TRUE,
    // FullscreenController::SetMetroSnapMode(flase)
    METRO_SNAP_FALSE,
    // FullscreenController::ExitTabOrBrowserFullscreenToPreviousState
    BUBBLE_EXIT_LINK,
    // FullscreenController::OnAcceptFullscreenPermission
    BUBBLE_ALLOW,
    // FullscreenController::OnDenyFullscreenPermission
    BUBBLE_DENY,
    // FullscreenController::ChangeWindowFullscreenState()
    WINDOW_CHANGE,
    NUM_EVENTS,
    EVENT_INVALID,
  };

  static const int MAX_STATE_NAME_LENGTH = 39;
  static const int MAX_EVENT_NAME_LENGTH = 24;

  FullscreenControllerStateTest();
  virtual ~FullscreenControllerStateTest();

  static const char* GetStateString(State state);
  static const char* GetEventString(Event event);

  // Returns true if WindowFullscreenStateChanged() is called synchronously as a
  // result of calling BrowserWindow's fullscreen related modifiers.
  static bool IsReentrant();

  // Returns true if |state| can be persistent. This is true for all of the
  // states without "_TO_" in their name.
  static bool IsPersistentState(State state);

  // Causes Fullscreen Controller to transition to an arbitrary state.
  void TransitionToState(State state);
  // Makes one state change to approach |destination_state| via shortest path.
  // Returns true if a state change is made.
  // Repeated calls are needed to reach the destination.
  bool TransitionAStepTowardState(State destination_state);

  virtual void ChangeWindowFullscreenState() {}
  virtual const char* GetWindowStateString();

  // Causes the |event| to occur and return true on success.
  virtual bool InvokeEvent(Event event);

  // Checks that window state matches the expected controller state.
  virtual void VerifyWindowState();

  // Wait for NOTIFICATION_FULLSCREEN_CHANGED if a notification should have been
  // sent in transitioning to |state_| from the previous persistent state.
  void MaybeWaitForNotification();

  // Tests all states with all permutations of multiple events to detect
  // lingering state issues that would bleed over to other states.
  // I.E. for each state test all combinations of events E1, E2, E3.
  //
  // This produces coverage for event sequences that may happen normally but
  // would not be exposed by traversing to each state via TransitionToState().
  // TransitionToState() always takes the same path even when multiple paths
  // exist.
  void TestTransitionsForEachState();

  // Log transition_table_ to a string for debugging.
  std::string GetTransitionTableAsString() const;
  // Log state_transitions_ to a string for debugging.
  std::string GetStateTransitionsAsString() const;

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

  // Returns true if the |state| & |event| pair should be skipped.
  virtual bool ShouldSkipStateAndEventPair(State state, Event event);

  // Returns true if a test should be skipped entirely, e.g. due to platform.
  virtual bool ShouldSkipTest(State state, Event event);

  // Runs one test of transitioning to a state and invoking an event.
  virtual void TestStateAndEvent(State state, Event event);

  virtual Browser* GetBrowser() = 0;
  FullscreenController* GetFullscreenController();

  State state_;

  // The state when the previous NOTIFICATION_FULLSCREEN_CHANGED notification
  // was received.
  State last_notification_received_state_;

  // Listens for the NOTIFICATION_FULLSCREEN_CHANGED notification.
  scoped_ptr<FullscreenNotificationObserver> fullscreen_notification_observer_;

  // Human defined |State| that results given each [state][event] pair.
  State transition_table_[NUM_STATES][NUM_EVENTS];

  // Generated information about the transitions between states [from][to].
  // View generated data with: out/Release/unit_tests
  //     --gtest_filter="FullscreenController*DebugLogStateTables"
  //     --gtest_also_run_disabled_tests
  StateTransitionInfo state_transitions_[NUM_STATES][NUM_STATES];

  // Log of operations reported on errors via GetAndClearDebugLog().
  std::ostringstream debugging_log_;

  DISALLOW_COPY_AND_ASSIGN(FullscreenControllerStateTest);
};

#endif  // CHROME_BROWSER_UI_FULLSCREEN_FULLSCREEN_CONTROLLER_STATE_TEST_H_
