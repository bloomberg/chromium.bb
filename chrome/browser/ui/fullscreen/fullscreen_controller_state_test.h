// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_FULLSCREEN_FULLSCREEN_CONTROLLER_STATE_TEST_H_
#define CHROME_BROWSER_UI_FULLSCREEN_FULLSCREEN_CONTROLLER_STATE_TEST_H_

#include <sstream>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "build/build_config.h"

class Browser;
class FullscreenController;

// Test fixture testing Fullscreen Controller through its states. --------------
class FullscreenControllerStateTest {
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

  static const int MAX_STATE_NAME_LENGTH = 37;
  static const int MAX_EVENT_NAME_LENGTH = 20;

  FullscreenControllerStateTest();
  virtual ~FullscreenControllerStateTest();

  static const char* GetStateString(State state);
  static const char* GetEventString(Event event);

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

  // Tests all states with all permutations of multiple events to detect
  // lingering state issues that would bleed over to other states.
  // I.E. for each state test all combinations of events E1, E2, E3.
  //
  // This produces coverage for event sequences that may happen normally but
  // would not be exposed by traversing to each state via TransitionToState().
  // TransitionToState() always takes the same path even when multiple paths
  // exist.
  void TestTransitionsForEachState();

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
  virtual bool ShouldSkipStateAndEventPair(State state, Event event);
  // Skips reentrant situations and calls ShouldSkipStateAndEventPair.
  virtual bool ShouldSkipTest(State state, Event event, bool reentrant);

  // Runs one test of transitioning to a state and invoking an event.
  virtual void TestStateAndEvent(State state, Event event, bool reentrant);

  virtual Browser* GetBrowser() = 0;
  FullscreenController* GetFullscreenController();

  State state_;
  bool reentrant_;

  // Human defined |State| that results given each [state][event] pair.
  State transition_table_[NUM_STATES][NUM_EVENTS];

  // Generated information about the transitions between states [from][to].
  StateTransitionInfo state_transitions_[NUM_STATES][NUM_STATES];

  // Log of operations reported on errors via GetAndClearDebugLog().
  std::ostringstream debugging_log_;

  DISALLOW_COPY_AND_ASSIGN(FullscreenControllerStateTest);
};

#endif  // CHROME_BROWSER_UI_FULLSCREEN_FULLSCREEN_CONTROLLER_STATE_TEST_H_
