// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/config/gpu_info_collector.h"

#include "base/logging.h"

namespace gpu {

CollectInfoResult CollectBasicGraphicsInfo(GPUInfo* gpu_info) {
  NOTIMPLEMENTED();  // TODO(fuchsia): https://crbug.com/707031 Some day.
  return kCollectInfoNonFatalFailure;
}

CollectInfoResult CollectDriverInfoGL(GPUInfo* gpu_info) {
  NOTIMPLEMENTED();  // TODO(fuchsia): https://crbug.com/707031 Some day.
  return kCollectInfoNonFatalFailure;
}

}  // namespace gpu
