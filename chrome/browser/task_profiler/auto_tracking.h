// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TASK_PROFILER_AUTO_TRACKING_H_
#define CHROME_BROWSER_TASK_PROFILER_AUTO_TRACKING_H_

#include "base/files/file_path.h"
#include "base/tracked_objects.h"

//------------------------------------------------------------------------------
// Provide simple way to to start global tracking, and to tear down tracking
// when done.  The design has evolved to *not* do any teardown (and just leak
// all allocated data structures).  This class is currently used to ensure
// that the profiler data is output during shutdown, if this feature has been
// requested.

namespace task_profiler {

class AutoTracking {
 public:
  AutoTracking() {
    tracked_objects::ThreadData::Initialize();
  }

  ~AutoTracking();

  void set_output_file_path(const base::FilePath &path);

 private:
  base::FilePath output_file_path_;

  DISALLOW_COPY_AND_ASSIGN(AutoTracking);
};

}  // namespace task_profiler

#endif  // CHROME_BROWSER_TASK_PROFILER_AUTO_TRACKING_H_
