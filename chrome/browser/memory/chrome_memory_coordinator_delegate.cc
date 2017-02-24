// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/memory/chrome_memory_coordinator_delegate.h"

#include "base/memory/ptr_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/memory/tab_manager.h"

namespace memory {

// static
std::unique_ptr<content::MemoryCoordinatorDelegate>
ChromeMemoryCoordinatorDelegate::Create() {
  return base::WrapUnique(new ChromeMemoryCoordinatorDelegate);
}

ChromeMemoryCoordinatorDelegate::ChromeMemoryCoordinatorDelegate() {}

ChromeMemoryCoordinatorDelegate::~ChromeMemoryCoordinatorDelegate() {}

bool ChromeMemoryCoordinatorDelegate::CanSuspendBackgroundedRenderer(
    int render_process_id) {
#if defined(OS_ANDROID)
  // TODO(bashi): Implement
  return true;
#else
  return g_browser_process->GetTabManager()->CanSuspendBackgroundedRenderer(
      render_process_id);
#endif
}

void ChromeMemoryCoordinatorDelegate::DiscardTab() {
#if !defined(OS_ANDROID)
  if (g_browser_process->GetTabManager())
    g_browser_process->GetTabManager()->DiscardTab();
#endif
}

}  // namespace memory
