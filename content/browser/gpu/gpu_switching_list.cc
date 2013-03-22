// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/gpu/gpu_switching_list.h"

#include "content/public/common/gpu_switching_option.h"

namespace content {

GpuSwitchingList::GpuSwitchingList()
    : GpuControlList() {
}

GpuSwitchingList::~GpuSwitchingList() {
}

// static
GpuSwitchingList* GpuSwitchingList::Create() {
  GpuSwitchingList* list = new GpuSwitchingList();
  list->AddFeature("force_integrated",
                   GPU_SWITCHING_OPTION_FORCE_INTEGRATED);
  list->AddFeature("force_discrete",
                   GPU_SWITCHING_OPTION_FORCE_DISCRETE);
  return list;
}

}  // namespace content

