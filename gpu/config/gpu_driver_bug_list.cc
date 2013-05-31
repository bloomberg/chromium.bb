// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/config/gpu_driver_bug_list.h"

#include "base/basictypes.h"
#include "base/logging.h"
#include "gpu/config/gpu_driver_bug_workaround_type.h"

namespace gpu {

namespace {

struct DriverBugInfo {
  int feature_type;
  std::string feature_name;
};

}  // namespace anonymous

GpuDriverBugList::GpuDriverBugList()
    : GpuControlList() {
}

GpuDriverBugList::~GpuDriverBugList() {
}

// static
GpuDriverBugList* GpuDriverBugList::Create() {
  GpuDriverBugList* list = new GpuDriverBugList();

  const DriverBugInfo kFeatureList[] = {
#define GPU_OP(type, name) { type, #name },
    GPU_DRIVER_BUG_WORKAROUNDS(GPU_OP)
#undef GPU_OP
  };
  DCHECK_EQ(static_cast<int>(arraysize(kFeatureList)),
            NUMBER_OF_GPU_DRIVER_BUG_WORKAROUND_TYPES);
  for (int i = 0; i < NUMBER_OF_GPU_DRIVER_BUG_WORKAROUND_TYPES; ++i) {
    list->AddSupportedFeature(kFeatureList[i].feature_name,
                              kFeatureList[i].feature_type);
  }
  return list;
}

std::string GpuDriverBugWorkaroundTypeToString(
    GpuDriverBugWorkaroundType type) {
  switch (type) {
#define GPU_OP(type, name) case type: return #name;
    GPU_DRIVER_BUG_WORKAROUNDS(GPU_OP)
#undef GPU_OP
    default:
      return "unknown";
  };
}

}  // namespace gpu

