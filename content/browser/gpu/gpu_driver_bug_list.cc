// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/gpu/gpu_driver_bug_list.h"

#include "base/basictypes.h"
#include "base/logging.h"
#include "gpu/command_buffer/service/gpu_driver_bug_workaround_type.h"

namespace content {

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
#define GPU_OP(type, name) { gpu::type, #name },
    GPU_DRIVER_BUG_WORKAROUNDS(GPU_OP)
#undef GPU_OP
  };
  DCHECK_EQ(static_cast<int>(arraysize(kFeatureList)),
            gpu::NUMBER_OF_GPU_DRIVER_BUG_WORKAROUND_TYPES);
  for (int i = 0; i < gpu::NUMBER_OF_GPU_DRIVER_BUG_WORKAROUND_TYPES; ++i) {
    list->AddSupportedFeature(kFeatureList[i].feature_name,
                              kFeatureList[i].feature_type);
  }
  return list;
}

}  // namespace content

