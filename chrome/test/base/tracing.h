// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_BASE_TRACING_H_
#define CHROME_TEST_BASE_TRACING_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/time/time.h"

namespace tracing {

// Called from UI thread.
// Begin tracing specified category_patterns on the browser.
// |category_patterns| is a comma-delimited list of category wildcards.
// A category pattern can have an optional '-' prefix to make  categories with
// matching categorys excluded. Either all category_patterns must be included
// or all must be excluded.
//
// Example: BeginTracing("test_MyTest*");
// Example: BeginTracing("test_MyTest*,test_OtherStuff");
// Example: BeginTracing("-excluded_category1,-excluded_category2");
//
// See base/debug/trace_event.h for documentation of included and excluded
// category_patterns.
bool BeginTracing(const std::string& category_patterns) WARN_UNUSED_RESULT;

// Called from UI thread.
// Specify a watch event in order to use the WaitForWatchEvent function.
// After |num_occurrences| of the given event have been seen on a particular
// process, WaitForWatchEvent will return.
bool BeginTracingWithWatch(const std::string& category_patterns,
                           const std::string& category_name,
                           const std::string& event_name,
                           int num_occurrences) WARN_UNUSED_RESULT;

// Called from UI thread.
// Wait on the event set with BeginTracingWithWatch. If non-zero, return after
// |timeout| regardless of watch event notification. Returns true if watch event
// occurred, false if it timed out.
bool WaitForWatchEvent(base::TimeDelta timeout) WARN_UNUSED_RESULT;

// Called from UI thread.
// End trace and collect the trace output as a json string.
bool EndTracing(std::string* json_trace_output) WARN_UNUSED_RESULT;

}  // namespace tracing

#endif  // CHROME_TEST_BASE_TRACING_H_
