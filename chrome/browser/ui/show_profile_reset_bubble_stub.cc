// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/show_profile_reset_bubble.h"

#include "base/logging.h"

// This is for the code in ProfileResetGlobalError to compile on platforms where
// the profile reset bubble is not implemented yet.
void ShowProfileResetBubble(
    Browser* browser,
    const ProfileResetCallback& reset_callback) {
  NOTREACHED();
}
