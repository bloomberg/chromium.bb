// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/exclusive_access/exclusive_access_context.h"

#include "base/logging.h"
#include "build/build_config.h"

// This file provides default implementations for the ExclusiveAccessContext
// methods that only some platforms care about.

bool ExclusiveAccessContext::SupportsFullscreenWithToolbar() const {
  return false;
}

void ExclusiveAccessContext::UpdateFullscreenWithToolbar(bool with_toolbar) {
  NOTIMPLEMENTED();
}

void ExclusiveAccessContext::ToggleFullscreenToolbar() {
  NOTIMPLEMENTED();
}

bool ExclusiveAccessContext::IsFullscreenWithToolbar() const {
  return false;
}

#if defined(OS_WIN)
void ExclusiveAccessContext::SetMetroSnapMode(bool enable) {
  NOTIMPLEMENTED();
}

bool ExclusiveAccessContext::IsInMetroSnapMode() const {
  return false;
}
#endif  // defined(OS_WIN)
