// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TASK_PROFILER_TASK_PROFILER_DATA_SERIALIZER_H_
#define CHROME_BROWSER_TASK_PROFILER_TASK_PROFILER_DATA_SERIALIZER_H_
#pragma once

class FilePath;

namespace task_profiler {

// This class collects task profiler data and serializes it to a file.  The file
// format is compatible with the about:profiler UI.
class TaskProfilerDataSerializer {
 public:
  bool WriteToFile(const FilePath &path);
};

}  // namespace task_profiler

#endif  // CHROME_BROWSER_TASK_PROFILER_TASK_PROFILER_DATA_SERIALIZER_H_
