// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_ARC_AUTH_NOTIFICATION_H_
#define CHROME_BROWSER_CHROMEOS_ARC_ARC_AUTH_NOTIFICATION_H_

class Profile;

namespace arc {

// First run notification that can enable ARC.
class ArcAuthNotification {
 public:
  static void Show(Profile* profile);
  static void Hide();

  // Disables showing ArcAuthNotification to make testing easier.
  static void DisableForTesting();
};

}  // namespace arc

#endif  // CHROME_BROWSER_CHROMEOS_ARC_ARC_AUTH_NOTIFICATION_H_
