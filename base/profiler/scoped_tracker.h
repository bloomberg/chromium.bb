// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_PROFILER_SCOPED_TRACKER_H_
#define BASE_PROFILER_SCOPED_TRACKER_H_

//------------------------------------------------------------------------------
// Utilities for temporarily instrumenting code to dig into issues that were
// found using profiler data.

#include "base/base_export.h"
#include "base/callback_forward.h"
#include "base/location.h"
#include "base/profiler/scoped_profile.h"

namespace tracked_objects {

// ScopedTracker instruments a region within the code if the instrumentation is
// enabled. It can be used, for example, to find out if a source of jankiness is
// inside the instrumented code region.
class BASE_EXPORT ScopedTracker {
 public:
  ScopedTracker(const Location& location);

  // Enables instrumentation for the remainder of the current process' life. If
  // this function is not called, all profiler instrumentations are no-ops.
  static void Enable();

  // Augments a |callback| with provided |location|. This is useful for
  // instrumenting cases when we know that a jank is in a callback and there are
  // many possible callbacks, but they come from a relatively small number of
  // places. We can instrument these few places and at least know which one
  // passes the janky callback.
  static base::Closure TrackCallback(const Location& location,
                                     const base::Closure& callback);

 private:
  const ScopedProfile scoped_profile_;

  DISALLOW_COPY_AND_ASSIGN(ScopedTracker);
};

}  // namespace tracked_objects

#endif  // BASE_PROFILER_SCOPED_TRACKER_H_
