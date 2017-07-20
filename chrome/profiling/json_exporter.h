// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_PROFILING_JSON_EXPORTER_H_
#define CHROME_PROFILING_JSON_EXPORTER_H_

#include <iosfwd>

#include "chrome/profiling/allocation_event.h"

namespace profiling {

class BacktraceStorage;

void ExportAllocationEventSetToJSON(int pid,
                                    const BacktraceStorage* backtrace_storage,
                                    const AllocationEventSet& set,
                                    std::ostream& out);

}  // namespace profiling

#endif  // CHROME_PROFILING_JSON_EXPORTER_H_
