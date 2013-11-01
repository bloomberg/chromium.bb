// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Definitions for chrome/browser/ui/profile_reset_bubble.h, for platforms where
// the profile reset bubble is not implemented yet.

#include "chrome/browser/ui/profile_reset_bubble.h"

#include "base/logging.h"

bool IsProfileResetBubbleSupported() {
  return false;
}

GlobalErrorBubbleViewBase* ShowProfileResetBubble(
    const base::WeakPtr<ProfileResetGlobalError>& global_error,
    Browser* browser) {
  NOTREACHED();
  return NULL;
}
