// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_FILES_FILE_TRACING_H_
#define BASE_FILES_FILE_TRACING_H_

#include "base/basictypes.h"
#include "base/macros.h"
#include "base/trace_event/trace_event.h"

#define FILE_TRACING_PREFIX "File"

#define FILE_TRACING_BEGIN() \
    TRACE_EVENT_NESTABLE_ASYNC_BEGIN0( \
        base::kFileTracingEventCategoryGroup, FILE_TRACING_PREFIX, this)

#define FILE_TRACING_END() \
    TRACE_EVENT_NESTABLE_ASYNC_END0( \
        base::kFileTracingEventCategoryGroup, FILE_TRACING_PREFIX, this)

#define SCOPED_FILE_TRACE_WITH_SIZE(name, size) \
    base::ScopedFileTrace scoped_file_trace; \
    do { \
      bool category_enabled; \
      TRACE_EVENT_CATEGORY_GROUP_ENABLED( \
          base::kFileTracingEventCategoryGroup, &category_enabled); \
      if (category_enabled) \
        scoped_file_trace.Initialize( \
            FILE_TRACING_PREFIX "::" name, this, size); \
    } while (0)

#define SCOPED_FILE_TRACE(name) SCOPED_FILE_TRACE_WITH_SIZE(name, 0)

namespace base {

class File;

extern const char kFileTracingEventCategoryGroup[];

class ScopedFileTrace {
 public:
  ScopedFileTrace();
  ~ScopedFileTrace();

  // Called only if the tracing category is enabled.
  void Initialize(const char* event, File* file, int64 size);

 private:
  // True if |Initialize()| has been called. Don't touch |path_|, |event_|,
  // or |bytes_| if |initialized_| is false.
  bool initialized_;

  // The event name to trace (e.g. "Read", "Write"). Prefixed with "base::File".
  const char* name_;

  // The file being traced. Must outlive this class.
  File* file_;

  // The size (in bytes) of this trace. Not reported if 0.
  int64 size_;

  DISALLOW_COPY_AND_ASSIGN(ScopedFileTrace);
};

}  // namespace base

#endif  // BASE_FILES_FILE_TRACING_H_
