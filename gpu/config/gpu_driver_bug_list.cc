// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/config/gpu_driver_bug_list.h"

#include "base/basictypes.h"
#include "base/logging.h"
#include "gpu/config/gpu_driver_bug_workaround_type.h"

namespace gpu {

namespace {

const DriverBugInfo kFeatureList[] = {
#define GPU_OP(type, name) { type, #name },
  GPU_DRIVER_BUG_WORKAROUNDS(GPU_OP)
#undef GPU_OP
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
  if (type < NUMBER_OF_GPU_DRIVER_BUG_WORKAROUND_TYPES)
    return kFeatureList[type].feature_name;
  else
    return "unknown";
}

const struct DriverBugInfo* GetDriverBugWorkarounds() {
  return kFeatureList;
}

}  // namespace gpu

