// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/render_process_visibility_manager.h"

#include "base/logging.h"
#include "base/memory/memory_pressure_listener.h"

namespace content {

RenderProcessVisibilityManager::RenderProcessVisibilityManager()
    : num_visible_render_widgets_(0) {
}

RenderProcessVisibilityManager::~RenderProcessVisibilityManager() {
}

// static
RenderProcessVisibilityManager* RenderProcessVisibilityManager::GetInstance() {
  return Singleton<RenderProcessVisibilityManager>::get();
}

void RenderProcessVisibilityManager::WidgetVisibilityChanged(bool visible) {
#if !defined(SYSTEM_NATIVELY_SIGNALS_MEMORY_PRESSURE)
  num_visible_render_widgets_ += visible ? 1 : -1;
  DCHECK_LE(0, num_visible_render_widgets_);
  if (num_visible_render_widgets_ == 0) {
    // TODO(vollick): Remove this this heavy-handed approach once we're polling
    // the real system memory pressure.
    base::MemoryPressureListener::NotifyMemoryPressure(
        base::MemoryPressureListener::MEMORY_PRESSURE_MODERATE);
  }
#endif
}

}  // namespace content
