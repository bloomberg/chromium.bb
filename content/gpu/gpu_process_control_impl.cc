// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/gpu/gpu_process_control_impl.h"

#include "base/logging.h"

namespace content {

GpuProcessControlImpl::GpuProcessControlImpl() {}

GpuProcessControlImpl::~GpuProcessControlImpl() {}

void GpuProcessControlImpl::RegisterApplicationLoaders(
    URLToLoaderMap* url_to_loader_map) {
  // TODO(xhwang): Support MojoMediaApplication here.
  // See http://crbug.com/521755
  NOTIMPLEMENTED();
}

}  // namespace content
