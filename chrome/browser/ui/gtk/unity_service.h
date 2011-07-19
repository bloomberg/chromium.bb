// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GTK_UNITY_SERVICE_H_
#define CHROME_BROWSER_UI_GTK_UNITY_SERVICE_H_

#include <gtk/gtk.h>

namespace unity {

// Returns whether unity is currently running.
bool IsRunning();

// If unity is running, sets the download counter in the dock icon. Any value
// other than 0 displays the badge.
void SetDownloadCount(int count);

// If unity is running, sets the download progress bar in the dock icon. Any
// value between 0.0 and 1.0 (exclusive) shows the progress bar.
void SetProgressFraction(float percentage);

}  // namespace unity

#endif  // CHROME_BROWSER_UI_GTK_UNITY_SERVICE_H_
