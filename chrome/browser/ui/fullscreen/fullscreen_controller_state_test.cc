// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/fullscreen/fullscreen_controller_state_test.h"

#include <memory.h>

#include <iomanip>
#include <iostream>

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/fullscreen/fullscreen_controller.h"
#include "content/public/common/url_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

FullscreenControllerStateTest::FullscreenControllerStateTest()
    : state_(STATE_NORMAL),
      reentrant_(false) {
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
      STATE_NORMAL,                           // Event BUBBLE_EXIT_LINK
      STATE_NORMAL,                           // Event BUBBLE_ALLOW
      STATE_NORMAL,                           // Event BUBBLE_DENY
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
      STATE_TO_NORMAL,                        // Event BUBBLE_EXIT_LINK
      STATE_BROWSER_FULLSCREEN_NO_CHROME,     // Event BUBBLE_ALLOW
      STATE_BROWSER_FULLSCREEN_NO_CHROME,     // Event BUBBLE_DENY
      STATE_BROWSER_FULLSCREEN_NO_CHROME,     // Event WINDOW_CHANGE
    },
#if defined(OS_WIN)
    { // STATE_METRO_SNAP:
      STATE_METRO_SNAP,                       // Event TOGGLE_FULLSCREEN
      STATE_METRO_SNAP,                       // Event TAB_FULLSCREEN_TRUE
      STATE_METRO_SNAP,                       // Event TAB_FULLSCREEN_FALSE
      STATE_METRO_SNAP,                       // Event METRO_SNAP_TRUE
      STATE_NORMAL,                           // Event METRO_SNAP_FALSE
      STATE_METRO_SNAP,                       // Event BUBBLE_EXIT_LINK
      STATE_METRO_SNAP,                       // Event BUBBLE_ALLOW
      STATE_METRO_SNAP,                       // Event BUBBLE_DENY
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
      STATE_TO_NORMAL,                        // Event BUBBLE_EXIT_LINK
      STATE_TAB_FULLSCREEN,                   // Event BUBBLE_ALLOW
      STATE_TO_NORMAL,                        // Event BUBBLE_DENY
      STATE_TAB_FULLSCREEN,                   // Event WINDOW_CHANGE
    },
    { // STATE_TAB_BROWSER_FULLSCREEN:
      STATE_TO_NORMAL,                        // Event TOGGLE_FULLSCREEN
      STATE_TAB_BROWSER_FULLSCREEN,           // Event TAB_FULLSCREEN_TRUE
      STATE_BROWSER_FULLSCREEN_NO_CHROME,     // Event TAB_FULLSCREEN_FALSE
#if defined(OS_WIN)
      STATE_METRO_SNAP,                       // Event METRO_SNAP_TRUE
      STATE_TAB_BROWSER_FULLSCREEN,           // Event METRO_SNAP_FALSE
#endif
      STATE_BROWSER_FULLSCREEN_NO_CHROME,     // Event BUBBLE_EXIT_LINK
      STATE_TAB_BROWSER_FULLSCREEN,           // Event BUBBLE_ALLOW
      STATE_BROWSER_FULLSCREEN_NO_CHROME,     // Event BUBBLE_DENY
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
      STATE_TO_NORMAL,                        // Event BUBBLE_EXIT_LINK
      STATE_TO_NORMAL,                        // Event BUBBLE_ALLOW
      STATE_TO_NORMAL,                        // Event BUBBLE_DENY
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
#if defined(OS_MACOSX)
      // Mac window reports fullscreen immediately and an exit triggers exit.
      STATE_TO_NORMAL,                        // Event BUBBLE_EXIT_LINK
#else
      STATE_TO_BROWSER_FULLSCREEN_NO_CHROME,  // Event BUBBLE_EXIT_LINK
#endif
      STATE_TO_BROWSER_FULLSCREEN_NO_CHROME,  // Event BUBBLE_ALLOW
      STATE_TO_BROWSER_FULLSCREEN_NO_CHROME,  // Event BUBBLE_DENY
      STATE_BROWSER_FULLSCREEN_NO_CHROME,     // Event WINDOW_CHANGE
    },
    // STATE_TO_TAB_FULLSCREEN:
    { // TODO(scheib) Should be a route to TAB_BROWSER http://crbug.com/154196
      STATE_TO_TAB_FULLSCREEN,                // Event TOGGLE_FULLSCREEN
      STATE_TO_TAB_FULLSCREEN,                // Event TAB_FULLSCREEN_TRUE
#if defined(OS_MACOSX)
      // Mac runs as expected due to a forced NotifyTabOfExitIfNecessary();
      STATE_TO_NORMAL,                        // Event TAB_FULLSCREEN_FALSE
#else
      // TODO(scheib) Should be a route back to NORMAL. http://crbug.com/154196
      STATE_TO_BROWSER_FULLSCREEN_NO_CHROME,  // Event TAB_FULLSCREEN_FALSE
#endif
#if defined(OS_WIN)
      STATE_METRO_SNAP,                       // Event METRO_SNAP_TRUE
      STATE_TO_TAB_FULLSCREEN,                // Event METRO_SNAP_FALSE
#endif
#if defined(OS_MACOSX)
      // Mac window reports fullscreen immediately and an exit triggers exit.
      STATE_TO_NORMAL,                        // Event BUBBLE_EXIT_LINK
#else
      STATE_TO_TAB_FULLSCREEN,                // Event BUBBLE_EXIT_LINK
#endif
      STATE_TO_TAB_FULLSCREEN,                // Event BUBBLE_ALLOW
#if defined(OS_MACOSX)
      // Mac window reports fullscreen immediately and an exit triggers exit.
      STATE_TO_NORMAL,                        // Event BUBBLE_DENY
#else
      STATE_TO_TAB_FULLSCREEN,                // Event BUBBLE_DENY
#endif
      STATE_TAB_FULLSCREEN,                   // Event WINDOW_CHANGE
    },
  };
  CHECK_EQ(sizeof(transition_table_data), sizeof(transition_table_));
  memcpy(transition_table_, transition_table_data,
         sizeof(transition_table_data));

  // Verify that transition_table_ has been completely defined.
  for (int source = 0; source < NUM_STATES; source++) {
    for (int event = 0; event < NUM_EVENTS; event++) {
      CHECK_NE(STATE_INVALID, transition_table_[source][event]);
      CHECK_LE(0, transition_table_[source][event]);
      CHECK_GT(NUM_STATES, transition_table_[source][event]);
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

FullscreenControllerStateTest::~FullscreenControllerStateTest() {
}

// static
const char* FullscreenControllerStateTest::GetStateString(State state) {
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
const char* FullscreenControllerStateTest::GetEventString(Event event) {
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
    case BUBBLE_EXIT_LINK:
      return "BUBBLE_EXIT_LINK";
    case BUBBLE_ALLOW:
      return "BUBBLE_ALLOW";
    case BUBBLE_DENY:
      return "BUBBLE_DENY";
    case WINDOW_CHANGE:
      return "WINDOW_CHANGE";
    case EVENT_INVALID:
      return "EVENT_INVALID";
    default:
      NOTREACHED() << "No string for event " << event;
      return "Event-Unknown";
  }
}

void FullscreenControllerStateTest::TransitionToState(State final_state) {
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

bool FullscreenControllerStateTest::TransitionAStepTowardState(
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

const char* FullscreenControllerStateTest::GetWindowStateString() {
  return NULL;
}

bool FullscreenControllerStateTest::InvokeEvent(Event event) {
  State source_state = state_;
  State next_state = transition_table_[source_state][event];

  // When simulating reentrant window change calls, expect the next state
  // automatically.
  if (reentrant_)
    next_state = transition_table_[next_state][WINDOW_CHANGE];

  debugging_log_ << "  InvokeEvent(" << std::left
      << std::setw(MAX_EVENT_NAME_LENGTH) << GetEventString(event)
      << ") to "
      << std::setw(MAX_STATE_NAME_LENGTH) << GetStateString(next_state);

  state_ = next_state;

  switch (event) {
    case TOGGLE_FULLSCREEN:
      GetFullscreenController()->ToggleFullscreenMode();
      break;
    case TAB_FULLSCREEN_TRUE:
      GetFullscreenController()->ToggleFullscreenModeForTab(
           chrome::GetActiveWebContents(GetBrowser()), true);
      break;
    case TAB_FULLSCREEN_FALSE:
      GetFullscreenController()->ToggleFullscreenModeForTab(
           chrome::GetActiveWebContents(GetBrowser()), false);
      break;
#if defined(OS_WIN)
    case METRO_SNAP_TRUE:
      GetFullscreenController()->SetMetroSnapMode(true);
      break;
    case METRO_SNAP_FALSE:
      GetFullscreenController()->SetMetroSnapMode(false);
      break;
#endif
    case BUBBLE_EXIT_LINK:
      GetFullscreenController()->ExitTabOrBrowserFullscreenToPreviousState();
      break;
    case BUBBLE_ALLOW:
      GetFullscreenController()->OnAcceptFullscreenPermission();
      break;
    case BUBBLE_DENY:
      GetFullscreenController()->OnDenyFullscreenPermission();
      break;
    case WINDOW_CHANGE:
      ChangeWindowFullscreenState();
      break;
    default:
      NOTREACHED() << "InvokeEvent needs a handler for event "
          << GetEventString(event) << GetAndClearDebugLog();
      return false;
  }

  if (GetWindowStateString())
    debugging_log_ << " Window state now " << GetWindowStateString() << "\n";
  else
    debugging_log_ << "\n";

  VerifyWindowState();

  return true;
}

void FullscreenControllerStateTest::VerifyWindowState() {
  switch (state_) {
    case STATE_NORMAL:
#if defined(OS_MACOSX)
      EXPECT_FALSE(GetBrowser()->window()->InPresentationMode())
          << GetAndClearDebugLog();
#endif
      EXPECT_FALSE(GetFullscreenController()->IsFullscreenForBrowser())
          << GetAndClearDebugLog();
      EXPECT_FALSE(GetFullscreenController()->IsFullscreenForTabOrPending())
          << GetAndClearDebugLog();
      EXPECT_FALSE(GetFullscreenController()->IsInMetroSnapMode())
          << GetAndClearDebugLog();
      break;
    case STATE_BROWSER_FULLSCREEN_NO_CHROME:
#if defined(OS_MACOSX)
      EXPECT_FALSE(GetBrowser()->window()->InPresentationMode())
          << GetAndClearDebugLog();
#endif
      EXPECT_TRUE(GetFullscreenController()->IsFullscreenForBrowser())
          << GetAndClearDebugLog();
      EXPECT_FALSE(GetFullscreenController()->IsFullscreenForTabOrPending())
          << GetAndClearDebugLog();
      EXPECT_FALSE(GetFullscreenController()->IsInMetroSnapMode())
          << GetAndClearDebugLog();
      break;
#if defined(OS_WIN)
    case STATE_METRO_SNAP:
      // No expectation for InPresentationMode.

      // TODO(scheib) IsFullscreenForBrowser and IsFullscreenForTabOrPending
      // are returning true and false in interactive tests with real window.
      // With only a single Metro Snap state in this test framework it isn't
      // fair to try to have an expectation anyway.
      //
      // No expectation for IsFullscreenForBrowser.
      // No expectation for IsFullscreenForTabOrPending.
      EXPECT_TRUE(GetFullscreenController()->IsInMetroSnapMode())
          << GetAndClearDebugLog();
      break;
#endif
    case STATE_TAB_FULLSCREEN:
#if defined(OS_MACOSX)
      EXPECT_TRUE(GetBrowser()->window()->InPresentationMode())
          << GetAndClearDebugLog();
#endif
      EXPECT_FALSE(GetFullscreenController()->IsFullscreenForBrowser())
          << GetAndClearDebugLog();
      EXPECT_TRUE(GetFullscreenController()->IsFullscreenForTabOrPending())
          << GetAndClearDebugLog();
      EXPECT_FALSE(GetFullscreenController()->IsInMetroSnapMode())
          << GetAndClearDebugLog();
      break;
    case STATE_TAB_BROWSER_FULLSCREEN:
#if defined(OS_MACOSX)
      EXPECT_FALSE(GetBrowser()->window()->InPresentationMode())
          << GetAndClearDebugLog();
#endif
      EXPECT_TRUE(GetFullscreenController()->IsFullscreenForBrowser())
          << GetAndClearDebugLog();
      EXPECT_TRUE(GetFullscreenController()->IsFullscreenForTabOrPending())
          << GetAndClearDebugLog();
      EXPECT_FALSE(GetFullscreenController()->IsInMetroSnapMode())
          << GetAndClearDebugLog();
      break;
    case STATE_TO_NORMAL:
#if defined(OS_MACOSX)
      EXPECT_FALSE(GetBrowser()->window()->InPresentationMode())
          << GetAndClearDebugLog();
#endif
      // No expectation for IsFullscreenForBrowser.
      // No expectation for IsFullscreenForTabOrPending.
      EXPECT_FALSE(GetFullscreenController()->IsInMetroSnapMode())
          << GetAndClearDebugLog();
      break;
    case STATE_TO_BROWSER_FULLSCREEN_NO_CHROME:
#if defined(OS_MACOSX)
      EXPECT_FALSE(GetBrowser()->window()->InPresentationMode())
          << GetAndClearDebugLog();
      EXPECT_TRUE(GetFullscreenController()->IsFullscreenForBrowser())
          << GetAndClearDebugLog();
#else
      EXPECT_FALSE(GetFullscreenController()->IsFullscreenForBrowser())
          << GetAndClearDebugLog();
#endif
      // No expectation for IsFullscreenForTabOrPending.
      EXPECT_FALSE(GetFullscreenController()->IsInMetroSnapMode())
          << GetAndClearDebugLog();
      break;
    case STATE_TO_TAB_FULLSCREEN:
#if defined(OS_MACOSX)
      // TODO(scheib) InPresentationMode returns false when invoking events:
      // TAB_FULLSCREEN_TRUE, TOGGLE_FULLSCREEN. http://crbug.com/156645
      // It may be that a new testing state TO_TAB_BROWSER_FULLSCREEN
      // would help work around this http://crbug.com/154196
      // Test with: STATE_TO_TAB_FULLSCREEN__TOGGLE_FULLSCREEN
      //
      // EXPECT_TRUE(GetBrowser()->window()->InPresentationMode())
      //     << GetAndClearDebugLog();
#endif
      EXPECT_FALSE(GetFullscreenController()->IsFullscreenForBrowser())
          << GetAndClearDebugLog();
      EXPECT_TRUE(GetFullscreenController()->IsFullscreenForTabOrPending())
          << GetAndClearDebugLog();
      EXPECT_FALSE(GetFullscreenController()->IsInMetroSnapMode())
          << GetAndClearDebugLog();
      break;
    default:
      NOTREACHED() << GetAndClearDebugLog();
  }
}

void FullscreenControllerStateTest::TestTransitionsForEachState() {
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
}

FullscreenControllerStateTest::StateTransitionInfo
    FullscreenControllerStateTest::NextTransitionInShortestPath(
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

std::string FullscreenControllerStateTest::GetAndClearDebugLog() {
  debugging_log_ << "(End of Debugging Log)\n";
  std::string output_log = "\nDebugging Log:\n" + debugging_log_.str();
  debugging_log_.str("");
  return output_log;
}

bool FullscreenControllerStateTest::ShouldSkipStateAndEventPair(State state,
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

bool FullscreenControllerStateTest::ShouldSkipTest(State state,
                                                  Event event,
                                                  bool reentrant) {
#if defined(OS_WIN)
  // FullscreenController verifies that WindowFullscreenStateChanged is
  // always reentrant on Windows. It will fail if we mock asynchronous calls.
  if (!reentrant) {
    debugging_log_ << "\nSkipping non-reentrant test on Windows.\n";
    return true;
  }
#else
  if (reentrant) {
    debugging_log_ << "\nSkipping reentrant test on non-Windows.\n";
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

  if (ShouldSkipStateAndEventPair(state, event)) {
    debugging_log_ << "\nSkipping test due to ShouldSkipStateAndEventPair("
        << GetStateString(state) << ", "
        << GetEventString(event) << ").\n";
    LOG(INFO) << "Skipping test due to ShouldSkipStateAndEventPair("
        << GetStateString(state) << ", "
        << GetEventString(event) << ").";
    return true;
  }

  return false;
}

void FullscreenControllerStateTest::TestStateAndEvent(State state,
                                                     Event event,
                                                     bool reentrant) {
  if (ShouldSkipTest(state, event, reentrant))
    return;

  debugging_log_ << "\nTest transition from state "
      << GetStateString(state)
      << (reentrant ? " with reentrant calls.\n" : ".\n");
  reentrant_ = reentrant;

  debugging_log_ << " First,                           from "
      << GetStateString(state_) << "\n";
  ASSERT_NO_FATAL_FAILURE(TransitionToState(state))
      << GetAndClearDebugLog();

  debugging_log_ << " Then,\n";
  ASSERT_TRUE(InvokeEvent(event)) << GetAndClearDebugLog();
}

FullscreenController* FullscreenControllerStateTest::GetFullscreenController() {
    return GetBrowser()->fullscreen_controller();
}

