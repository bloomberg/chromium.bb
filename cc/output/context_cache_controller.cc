// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/output/context_cache_controller.h"

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "gpu/command_buffer/client/context_support.h"
#include "third_party/skia/include/gpu/GrContext.h"

namespace cc {
ContextCacheController::ScopedVisibility::ScopedVisibility() = default;

ContextCacheController::ScopedVisibility::~ScopedVisibility() {
  DCHECK(released_);
}

void ContextCacheController::ScopedVisibility::Release() {
  DCHECK(!released_);
  released_ = true;
}

ContextCacheController::ContextCacheController(
    gpu::ContextSupport* context_support)
    : context_support_(context_support) {}

ContextCacheController::~ContextCacheController() = default;

void ContextCacheController::SetGrContext(GrContext* gr_context) {
  gr_context_ = gr_context;
}

std::unique_ptr<ContextCacheController::ScopedVisibility>
ContextCacheController::ClientBecameVisible() {
  bool became_visible = num_clients_visible_ == 0;
  ++num_clients_visible_;

  if (became_visible)
    context_support_->SetAggressivelyFreeResources(false);

  return base::WrapUnique(new ScopedVisibility());
}

void ContextCacheController::ClientBecameNotVisible(
    std::unique_ptr<ScopedVisibility> scoped_visibility) {
  DCHECK(scoped_visibility);
  scoped_visibility->Release();

  DCHECK_GT(num_clients_visible_, 0u);
  --num_clients_visible_;

  if (num_clients_visible_ == 0) {
    if (gr_context_)
      gr_context_->freeGpuResources();
    context_support_->SetAggressivelyFreeResources(true);
  }
}

std::unique_ptr<ContextCacheController::ScopedVisibility>
ContextCacheController::CreateScopedVisibilityForTesting() const {
  return base::WrapUnique(new ScopedVisibility());
}

void ContextCacheController::ReleaseScopedVisibilityForTesting(
    std::unique_ptr<ScopedVisibility> scoped_visibility) const {
  scoped_visibility->Release();
}

}  // namespace cc
