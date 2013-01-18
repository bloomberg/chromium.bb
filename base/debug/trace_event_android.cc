// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/debug/trace_event_impl.h"

#include <fcntl.h>

#include "base/debug/trace_event.h"
#include "base/logging.h"
#include "base/stringprintf.h"

namespace {

int g_atrace_fd = -1;
const char* kATraceMarkerFile = "/sys/kernel/debug/tracing/trace_marker";

}  // namespace

namespace base {
namespace debug {

// static
void TraceLog::InitATrace() {
  DCHECK(g_atrace_fd == -1);
  g_atrace_fd = open(kATraceMarkerFile, O_WRONLY | O_APPEND);
  if (g_atrace_fd == -1)
    LOG(WARNING) << "Couldn't open " << kATraceMarkerFile;
}

void TraceLog::SendToATrace(char phase,
                            const char* category,
                            const char* name,
                            int num_args,
                            const char** arg_names,
                            const unsigned char* arg_types,
                            const unsigned long long* arg_values) {
  if (g_atrace_fd == -1)
    return;

  switch (phase) {
    case TRACE_EVENT_PHASE_BEGIN:
    case TRACE_EVENT_PHASE_INSTANT: {
      std::string out = StringPrintf("B|%d|%s-%s", getpid(), category, name);
      for (int i = 0; i < num_args; ++i) {
        out += (i == 0 ? '|' : ';');
        out += arg_names[i];
        out += '=';
        TraceEvent::TraceValue value;
        value.as_uint = arg_values[i];
        std::string::size_type value_start = out.length();
        TraceEvent::AppendValueAsJSON(arg_types[i], value, &out);
        // Remove the quotes which may confuse the atrace script.
        ReplaceSubstringsAfterOffset(&out, value_start, "\\\"", "'");
        ReplaceSubstringsAfterOffset(&out, value_start, "\"", "");
        // Replace chars used for separators with similar chars in the value.
        std::replace(out.begin() + value_start, out.end(), ';', ',');
        std::replace(out.begin() + value_start, out.end(), '|', '!');
      }
      write(g_atrace_fd, out.c_str(), out.size());

      if (phase != TRACE_EVENT_PHASE_INSTANT)
        break;
      // Fall through. Simulate an instance event with a pair of begin/end.
    }
    case TRACE_EVENT_PHASE_END: {
      // Though a single 'E' is enough, here append pid and name so that
      // unpaired events can be found easily.
      std::string out = StringPrintf("E|%d|%s-%s", getpid(), category, name);
      write(g_atrace_fd, out.c_str(), out.size());
      break;
    }
    case TRACE_EVENT_PHASE_COUNTER:
      for (int i = 0; i < num_args; ++i) {
        DCHECK(arg_types[i] == TRACE_VALUE_TYPE_INT);
        std::string out = StringPrintf(
            "C|%d|%s-%s-%s|%d",
            getpid(), category, name,
            arg_names[i], static_cast<int>(arg_values[i]));
        write(g_atrace_fd, out.c_str(), out.size());
      }
      break;

    default:
      // Do nothing.
      break;
  }
}

// Must be called with lock_ locked.
void TraceLog::ApplyATraceEnabledFlag(unsigned char* category_enabled) {
  if (g_atrace_fd != -1)
    *category_enabled |= ATRACE_ENABLED;
}

}  // namespace debug
}  // namespace base
