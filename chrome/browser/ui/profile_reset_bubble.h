// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PROFILE_RESET_BUBBLE_H_
#define CHROME_BROWSER_UI_PROFILE_RESET_BUBBLE_H_

#include "base/memory/weak_ptr.h"

class Browser;
class GlobalErrorBubbleViewBase;
class ProfileResetGlobalError;

// Returns whether or not the profile reset bubble is supported on this
// platform.
bool IsProfileResetBubbleSupported();

// Shows the profile reset bubble on the platforms that support it.
GlobalErrorBubbleViewBase* ShowProfileResetBubble(
    const base::WeakPtr<ProfileResetGlobalError>& global_error,
    Browser* browser);

#endif  // CHROME_BROWSER_UI_PROFILE_RESET_BUBBLE_H_
