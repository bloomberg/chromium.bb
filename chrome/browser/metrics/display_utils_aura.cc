// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/display_utils.h"

#include "content/browser/browser_thread.h"
#include "ui/gfx/screen.h"

// static
void DisplayUtils::GetPrimaryDisplayDimensions(int* width, int* height) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  gfx::Rect bounds =
      gfx::Screen::GetMonitorAreaNearestPoint(gfx::Point());
  *width = bounds.width();
  *height = bounds.height();
}

// static
int DisplayUtils::GetDisplayCount() {
  NOTIMPLEMENTED();
  return 1;
}
