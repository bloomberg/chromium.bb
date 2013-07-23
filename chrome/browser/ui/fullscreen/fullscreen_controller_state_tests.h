// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_FULLSCREEN_FULLSCREEN_CONTROLLER_STATE_TESTS_H_
#define CHROME_BROWSER_UI_FULLSCREEN_FULLSCREEN_CONTROLLER_STATE_TESTS_H_

// Macros used to create individual tests for all state and event pairs.
// To be included in the middle of a test .cc file just after a definition for
// TEST_EVENT in order to instantiate all the necessary actual tests.  See
// fullscreen_controller_state_interactive_browsertest.cc and
// fullscreen_controller_state_unittest.cc.

#define TEST_ALL_EVENTS_NON_METRO(state) \
    TEST_EVENT(state, TOGGLE_FULLSCREEN) \
    TEST_EVENT(state, TOGGLE_FULLSCREEN_CHROME) \
    TEST_EVENT(state, TAB_FULLSCREEN_TRUE) \
    TEST_EVENT(state, TAB_FULLSCREEN_FALSE) \
    TEST_EVENT(state, BUBBLE_EXIT_LINK) \
    TEST_EVENT(state, BUBBLE_ALLOW) \
    TEST_EVENT(state, BUBBLE_DENY) \
    TEST_EVENT(state, WINDOW_CHANGE)

#if defined(OS_WIN)
#define TEST_ALL_EVENTS(state) \
    TEST_ALL_EVENTS_NON_METRO(state) \
    TEST_EVENT(state, METRO_SNAP_TRUE) \
    TEST_EVENT(state, METRO_SNAP_FALSE)
#else
#define TEST_ALL_EVENTS(state) TEST_ALL_EVENTS_NON_METRO(state)
#endif

TEST_ALL_EVENTS(STATE_NORMAL);
TEST_ALL_EVENTS(STATE_BROWSER_FULLSCREEN_NO_CHROME);
TEST_ALL_EVENTS(STATE_BROWSER_FULLSCREEN_WITH_CHROME);
#if defined(OS_WIN)
TEST_ALL_EVENTS(STATE_METRO_SNAP);
#endif
TEST_ALL_EVENTS(STATE_TAB_FULLSCREEN);
TEST_ALL_EVENTS(STATE_TAB_BROWSER_FULLSCREEN);
TEST_ALL_EVENTS(STATE_TAB_BROWSER_FULLSCREEN_CHROME);
TEST_ALL_EVENTS(STATE_TO_NORMAL);
TEST_ALL_EVENTS(STATE_TO_BROWSER_FULLSCREEN_NO_CHROME);
TEST_ALL_EVENTS(STATE_TO_BROWSER_FULLSCREEN_WITH_CHROME);
TEST_ALL_EVENTS(STATE_TO_TAB_FULLSCREEN);

#endif  // CHROME_BROWSER_UI_FULLSCREEN_FULLSCREEN_CONTROLLER_STATE_TESTS_H_
