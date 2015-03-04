// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/exclusive_access/exclusive_access_context.h"

#include "base/logging.h"

bool ExclusiveAccessContext::SupportsFullscreenWithToolbar() const {
  return false;
}

void ExclusiveAccessContext::UpdateFullscreenWithToolbar(bool with_toolbar) {
  NOTIMPLEMENTED();
}

#if defined(OS_WIN)
void ExclusiveAccessContext::SetMetroSnapMode(bool enable) {
  NOTIMPLEMENTED();
}

bool ExclusiveAccessContext::IsInMetroSnapMode() const {
  return false;
}
#endif  // defined(OS_WIN)

void ExclusiveAccessContext::UnhideDownloadShelf() {
  // NOOP implementation.
}

void ExclusiveAccessContext::HideDownloadShelf() {
  // NOOP implementation.
}
