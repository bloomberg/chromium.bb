// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_SYSTEM_TRAY_DELEGATE_UTILS_H_
#define CHROME_BROWSER_UI_ASH_SYSTEM_TRAY_DELEGATE_UTILS_H_

class UpgradeDetector;

namespace ash {
struct UpdateInfo;
}

// Fills |info| structure with data from |detector|.
void GetUpdateInfo(const UpgradeDetector* detector, ash::UpdateInfo* info);

#endif  // CHROME_BROWSER_UI_ASH_SYSTEM_TRAY_DELEGATE_UTILS_H_
