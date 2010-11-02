// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Defines functions/macros that can and/or need to be defined by the
// user of this library, and a unit test function that can optionally
// be called from whichever unit test harness the user of the library
// is using.

#ifndef CEEE_TESTING_SIDESTEP_INTEGRATION_H_
#define CEEE_TESTING_SIDESTEP_INTEGRATION_H_

#ifndef SIDESTEP_ASSERT  // you may define it explicitly yourself
#if defined(ENABLE_SIDESTEP_ASSERTIONS) || defined(_DEBUG) || defined(DEBUG)
#define SIDESTEP_ASSERT(x) sidestep::AssertImpl(x, #x)
#else
#define SIDESTEP_ASSERT(x)
#endif
#endif

#ifndef SIDESTEP_LOG  // you may define it explicitly yourself
#if defined(ENABLE_SIDESTEP_LOGGING) || defined(_DEBUG) || defined(DEBUG)
#define SIDESTEP_LOG(x) sidestep::LogImpl(x)
#else
#define SIDESTEP_LOG(x)
#endif
#endif

// You may define SIDESTEP_CHK yourself to make it fail your unit test
// harness when the condition is not met (see auto_testing_hook.h).
#ifndef SIDESTEP_CHK
#define SIDESTEP_CHK(x) if (x) {}
#endif

// You may define SIDESTEP_EXPECT_TRUE to override the behavior of
// the SideStep unit test in preamble_patcher_test.cc
#ifndef SIDESTEP_EXPECT_TRUE
#define SIDESTEP_EXPECT_TRUE(x) \
  do { if (!(x)) { \
      SIDESTEP_ASSERT(false && #x); \
      return false; } } while (false)
#endif

namespace sidestep {

// This is the function the macro SIDESTEP_ASSERT calls in debug
// builds by default.  Users of the library must link in its
// implementation.
// @param assertion_is_true True if the condition of the assertion was met.
// @param message A stringified version of the condition of the assertion.
void AssertImpl(bool assertion_is_true, const char* message);

// This is the function the macro SIDESTEP_LOG calls in debug builds,
// by default.  Users of the library must link in its implementation.
// @param message The message to be logged.
void LogImpl(const char* message);

// Runs the SideStep unit tests and returns true on success.
bool UnitTests();

};  // namespace sidestep

#endif  // CEEE_TESTING_SIDESTEP_INTEGRATION_H_
