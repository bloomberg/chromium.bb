// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_PROFILING_JSON_EXPORTER_H_
#define CHROME_PROFILING_JSON_EXPORTER_H_

#include <iosfwd>
#include <vector>

#include "base/values.h"
#include "chrome/profiling/allocation_event.h"
#include "services/resource_coordinator/public/interfaces/memory_instrumentation/memory_instrumentation.mojom.h"

namespace profiling {

// Creates a JSON-encoded string that is similar in form to traces created by
// TracingControllerImpl.
void ExportAllocationEventSetToJSON(
    int pid,
    const AllocationEventSet& set,
    const std::vector<memory_instrumentation::mojom::VmRegionPtr>& maps,
    std::ostream& out,
    std::unique_ptr<base::DictionaryValue> metadata,
    size_t min_size_threshold,
    size_t min_count_threshold);

// Creates a JSON string representing a JSON dictionary that contains memory
// maps and v2 format stack traces.
void ExportMemoryMapsAndV2StackTraceToJSON(
    const AllocationEventSet& set,
    const std::vector<memory_instrumentation::mojom::VmRegionPtr>& maps,
    std::ostream& out,
    size_t min_size_threshold,
    size_t min_count_threshold);

}  // namespace profiling

#endif  // CHROME_PROFILING_JSON_EXPORTER_H_
