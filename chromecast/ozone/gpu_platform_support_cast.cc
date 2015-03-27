// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/ozone/gpu_platform_support_cast.h"

#include "chromecast/ozone/surface_factory_cast.h"

namespace chromecast {
namespace ozone {

bool GpuPlatformSupportCast::OnMessageReceived(const IPC::Message& msg) {
  return false;
}

void GpuPlatformSupportCast::RelinquishGpuResources(
    const base::Closure& callback) {
  parent_->SetToRelinquishDisplay(callback);
}

IPC::MessageFilter* GpuPlatformSupportCast::GetMessageFilter() {
  return nullptr;
}

}  // namespace ozone
}  // namespace chromecast
