// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/display_utils.h"

#include <gdk/gdk.h>

#include "content/browser/browser_thread.h"

// static
void DisplayUtils::GetPrimaryDisplayDimensions(int* width, int* height) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  GdkScreen* screen = gdk_screen_get_default();
  if (width)
    *width = gdk_screen_get_width(screen);
  if (height)
    *height = gdk_screen_get_height(screen);
}

// static
int DisplayUtils::GetDisplayCount() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  // This query is kinda bogus for Linux -- do we want number of X screens?
  // The number of monitors Xinerama has?  We'll just use whatever GDK uses.
  GdkScreen* screen = gdk_screen_get_default();
  return gdk_screen_get_n_monitors(screen);
}
