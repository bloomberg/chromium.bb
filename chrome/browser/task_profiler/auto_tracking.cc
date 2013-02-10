// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_profiler/auto_tracking.h"
#include "chrome/browser/task_profiler/task_profiler_data_serializer.h"

namespace task_profiler {

AutoTracking::~AutoTracking() {
  if (!output_file_path_.empty()) {
    TaskProfilerDataSerializer output;
    output.WriteToFile(output_file_path_);
  }
}

void AutoTracking::set_output_file_path(const base::FilePath &path) {
  output_file_path_ = path;
}

}  // namespace task_profiler
