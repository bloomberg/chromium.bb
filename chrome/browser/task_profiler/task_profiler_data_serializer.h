// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TASK_PROFILER_TASK_PROFILER_DATA_SERIALIZER_H_
#define CHROME_BROWSER_TASK_PROFILER_TASK_PROFILER_DATA_SERIALIZER_H_

#include "base/basictypes.h"

namespace base {
class DictionaryValue;
class FilePath;
}

namespace tracked_objects {
struct ProcessDataSnapshot;
}

namespace task_profiler {

// This class collects task profiler data and serializes it to a file.  The file
// format is compatible with the about:profiler UI.
class TaskProfilerDataSerializer {
 public:
  TaskProfilerDataSerializer() {}

  // Writes the contents of |process_data| and |process_type| into |dictionary|.
  static void ToValue(const tracked_objects::ProcessDataSnapshot& process_data,
                      int process_type,
                      base::DictionaryValue* dictionary);

  bool WriteToFile(const base::FilePath& path);

 private:
  DISALLOW_COPY_AND_ASSIGN(TaskProfilerDataSerializer);
};

}  // namespace task_profiler

#endif  // CHROME_BROWSER_TASK_PROFILER_TASK_PROFILER_DATA_SERIALIZER_H_
