// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_ASH_UTIL_H_
#define CHROME_BROWSER_UI_ASH_ASH_UTIL_H_

namespace ui {
class Accelerator;
}  // namespace ui

namespace chrome {

// Returns true if Ash should be run at startup.
bool ShouldOpenAshOnStartup();

// Returns true if Chrome is running in the mash shell.
bool IsRunningInMash();

// Returns true if the given |accelerator| has been deprecated and hence can
// be consumed by web contents if needed.
bool IsAcceleratorDeprecated(const ui::Accelerator& accelerator);

}  // namespace chrome

#endif  // CHROME_BROWSER_UI_ASH_ASH_UTIL_H_
