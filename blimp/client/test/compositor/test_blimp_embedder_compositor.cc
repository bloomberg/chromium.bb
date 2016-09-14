// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/client/test/compositor/test_blimp_embedder_compositor.h"

#include <memory>

#include "base/synchronization/waitable_event.h"
#include "blimp/client/public/compositor/compositor_dependencies.h"
#include "blimp/client/support/compositor/blimp_context_provider.h"
#include "cc/layers/layer.h"
#include "cc/output/copy_output_request.h"
#include "cc/output/copy_output_result.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/native_widget_types.h"

namespace blimp {
namespace client {

TestBlimpEmbedderCompositor::TestBlimpEmbedderCompositor(
    CompositorDependencies* compositor_dependencies)
    : BlimpEmbedderCompositor(compositor_dependencies) {
  SetContextProvider(BlimpContextProvider::Create(
      gfx::kNullAcceleratedWidget,
      compositor_dependencies->GetGpuMemoryBufferManager()));
}

TestBlimpEmbedderCompositor::~TestBlimpEmbedderCompositor() = default;

}  // namespace client
}  // namespace blimp
