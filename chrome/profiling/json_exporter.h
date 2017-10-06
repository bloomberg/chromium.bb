// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_PROFILING_JSON_EXPORTER_H_
#define CHROME_PROFILING_JSON_EXPORTER_H_

#include <iosfwd>
#include <vector>

#include "base/values.h"
#include "chrome/common/profiling/memlog_stream.h"
#include "chrome/profiling/allocation_event.h"
#include "services/resource_coordinator/public/interfaces/memory_instrumentation/memory_instrumentation.mojom.h"

namespace profiling {

// Configuration passed to the export functions because they take many
// arguments. All parameters must be set. The pointers are not managed by this
// class and must outlive it.
//
// Whether something is a pointer or a value is determined by what makes the
// call site nicer without introducing unnecessary copies.
struct ExportParams {
  ExportParams();
  ~ExportParams();

  // Allocation events to export.
  AllocationCountMap allocs;

  // VM map of all regions in the process.
  const std::vector<memory_instrumentation::mojom::VmRegionPtr>* maps = nullptr;

  // Map from context string to context ID. A reverse-mapping will tell you
  // what the context_id in the allocation mean.
  const std::map<std::string, int>* context_map = nullptr;

  // Only allocations exceeding this size or count will be exported.
  size_t min_size_threshold = 0;
  size_t min_count_threshold = 0;
};

// Creates a JSON-encoded string that is similar in form to traces created by
// TracingControllerImpl. Metadata can be null.
void ExportAllocationEventSetToJSON(
    int pid,
    const ExportParams& params,
    std::unique_ptr<base::DictionaryValue> metadata,
    std::ostream& out);

// Creates a JSON string representing a JSON dictionary that contains memory
// maps and v2 format stack traces.
void ExportMemoryMapsAndV2StackTraceToJSON(const ExportParams& params,
                                           std::ostream& out);

}  // namespace profiling

#endif  // CHROME_PROFILING_JSON_EXPORTER_H_
