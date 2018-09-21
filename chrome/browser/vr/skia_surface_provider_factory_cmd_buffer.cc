// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/skia_surface_provider_factory.h"

#include "base/logging.h"

namespace vr {

std::unique_ptr<SkiaSurfaceProvider> SkiaSurfaceProviderFactory::Create() {
  // TODO(crbug/884256): Implement a surface provider using the command buffer.
  NOTIMPLEMENTED();
  return nullptr;
}

}  // namespace vr
