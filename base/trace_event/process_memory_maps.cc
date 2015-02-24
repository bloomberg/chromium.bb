// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/trace_event/process_memory_maps.h"

#include "base/trace_event/trace_event_argument.h"

namespace base {
namespace trace_event {

// static
const uint32 ProcessMemoryMaps::VMRegion::kProtectionFlagsRead = 4;
const uint32 ProcessMemoryMaps::VMRegion::kProtectionFlagsWrite = 2;
const uint32 ProcessMemoryMaps::VMRegion::kProtectionFlagsExec = 1;

ProcessMemoryMaps::ProcessMemoryMaps() {
}

ProcessMemoryMaps::~ProcessMemoryMaps() {
}

void ProcessMemoryMaps::AsValueInto(TracedValue* value) const {
  value->BeginArray("vm_regions");
  for (const auto& region : vm_regions_) {
    value->BeginDictionary();

    value->SetDouble("start_address", region.start_address);
    value->SetDouble("size_in_bytes", region.size_in_bytes);
    value->SetInteger("protection_flags", region.protection_flags);
    value->SetString("mapped_file", region.mapped_file);
    value->SetDouble("mapped_file_offset", region.mapped_file_offset);

    value->BeginDictionary("byte_stats");
    value->SetDouble("resident", region.byte_stats_resident);
    value->SetDouble("anonymous", region.byte_stats_anonymous);
    value->EndDictionary();

    value->EndDictionary();
  }
  value->EndArray();
}

}  // namespace trace_event
}  // namespace base
