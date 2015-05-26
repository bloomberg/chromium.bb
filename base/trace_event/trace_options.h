// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TRACE_EVENT_TRACE_OPTIONS_H_
#define BASE_TRACE_EVENT_TRACE_OPTIONS_H_

#include <string>

#include "base/base_export.h"

namespace base {
namespace trace_event {

// Options determines how the trace buffer stores data.
enum TraceRecordMode {
  // Record until the trace buffer is full.
  RECORD_UNTIL_FULL,

  // Record until the user ends the trace. The trace buffer is a fixed size
  // and we use it as a ring buffer during recording.
  RECORD_CONTINUOUSLY,

  // Echo to console. Events are discarded.
  ECHO_TO_CONSOLE,

  // Record until the trace buffer is full, but with a huge buffer size.
  RECORD_AS_MUCH_AS_POSSIBLE
};

struct BASE_EXPORT TraceOptions {
  TraceOptions()
      : record_mode(RECORD_UNTIL_FULL),
        enable_sampling(false),
        enable_systrace(false),
        enable_argument_filter(false) {}

  explicit TraceOptions(TraceRecordMode record_mode)
      : record_mode(record_mode),
        enable_sampling(false),
        enable_systrace(false),
        enable_argument_filter(false) {}

  // |options_string| is a comma-delimited list of trace options.
  // Possible options are: "record-until-full", "record-continuously",
  // "trace-to-console", "enable-sampling" and "enable-systrace".
  // The first 3 options are trace recoding modes and hence
  // mutually exclusive. If more than one trace recording modes appear in the
  // options_string, the last one takes precedence. If none of the trace
  // recording mode is specified, recording mode is RECORD_UNTIL_FULL.
  //
  // The trace option will first be reset to the default option
  // (record_mode set to RECORD_UNTIL_FULL, enable_sampling and enable_systrace
  // set to false) before options parsed from |options_string| are applied on
  // it.
  // If |options_string| is invalid, the final state of trace_options is
  // undefined.
  //
  // Example: trace_options.SetFromString("record-until-full")
  // Example: trace_options.SetFromString(
  //              "record-continuously, enable-sampling")
  // Example: trace_options.SetFromString("record-until-full, trace-to-console")
  // will set ECHO_TO_CONSOLE as the recording mode.
  //
  // Returns true on success.
  bool SetFromString(const std::string& options_string);

  std::string ToString() const;

  TraceRecordMode record_mode;
  bool enable_sampling;
  bool enable_systrace;
  bool enable_argument_filter;
};

}  // namespace trace_event
}  // namespace base

#endif  // BASE_TRACE_EVENT_TRACE_OPTIONS_H_
