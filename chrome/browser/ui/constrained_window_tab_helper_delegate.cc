// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/constrained_window_tab_helper_delegate.h"

#include <string.h>

bool ConstrainedWindowTabHelperDelegate::ShouldFocusConstrainedWindow() {
  return true;
}

void ConstrainedWindowTabHelperDelegate::SetWebContentsBlocked(
    content::WebContents* web_contents, bool blocked) {
}

ConstrainedWindowTabHelperDelegate::~ConstrainedWindowTabHelperDelegate() {}

bool ConstrainedWindowTabHelperDelegate::GetDialogTopCenter(gfx::Point* point) {
  return false;
}
