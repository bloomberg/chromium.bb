// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_BASE_TRACING_H_
#define CHROME_TEST_BASE_TRACING_H_
#pragma once

#include <string>

#include "base/compiler_specific.h"

namespace tracing {

// Begin tracing specified categories on the browser.
// |categories| is a comma-delimited list of category wildcards.
// A category can have an optional '-' prefix to make it an excluded category.
// Either all categories must be included or all must be excluded.
//
// Example: BeginTracing("test_MyTest*");
// Example: BeginTracing("test_MyTest*,test_OtherStuff");
// Example: BeginTracing("-excluded_category1,-excluded_category2");
//
// See base/debug/trace_event.h for documentation of included and excluded
// categories.
bool BeginTracing(const std::string& categories) WARN_UNUSED_RESULT;

// End trace and collect the trace output as a json string.
bool EndTracing(std::string* json_trace_output) WARN_UNUSED_RESULT;

}  // namespace tracing

#endif  // CHROME_TEST_BASE_TRACING_H_
