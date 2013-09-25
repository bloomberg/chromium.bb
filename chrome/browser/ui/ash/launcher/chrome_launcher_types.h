// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_LAUNCHER_CHROME_LAUNCHER_TYPES_H_
#define CHROME_BROWSER_UI_ASH_LAUNCHER_CHROME_LAUNCHER_TYPES_H_

namespace ash {

// Source of the launch or activation request, for tracking.
enum LaunchSource {
  LAUNCH_FROM_UNKNOWN,
  LAUNCH_FROM_APP_LIST,
  LAUNCH_FROM_APP_LIST_SEARCH
};

}  // namespace ash

#endif  // CHROME_BROWSER_UI_ASH_LAUNCHER_CHROME_LAUNCHER_TYPES_H_
