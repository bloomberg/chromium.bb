// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_ARC_AUTH_NOTIFICATION_H_
#define CHROME_BROWSER_CHROMEOS_ARC_ARC_AUTH_NOTIFICATION_H_

namespace arc {

// First run notification that can enable Arc.
class ArcAuthNotification {
 public:
  static void Show();
  static void Hide();
};

}  // namespace arc

#endif  // CHROME_BROWSER_CHROMEOS_ARC_ARC_AUTH_NOTIFICATION_H_
