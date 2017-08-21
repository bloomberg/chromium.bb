// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_ARC_AUTH_NOTIFICATION_H_
#define CHROME_BROWSER_CHROMEOS_ARC_ARC_AUTH_NOTIFICATION_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/session_manager/core/session_manager_observer.h"

class Profile;

namespace arc {

// First run notification that can enable ARC. It is shown when Chrome session
// is in active state to prevent quick disappearing after session switches from
// login screen to active state.
class ArcAuthNotification : public session_manager::SessionManagerObserver {
 public:
  explicit ArcAuthNotification(Profile* profile);
  ~ArcAuthNotification() override;

  static void DisableForTesting();

 private:
  void Show();
  void Hide();

  // session_manager::SessionManagerObserver:
  void OnSessionStateChanged() override;

  Profile* const profile_;

  // Keep last.
  base::WeakPtrFactory<ArcAuthNotification> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ArcAuthNotification);
};

}  // namespace arc

#endif  // CHROME_BROWSER_CHROMEOS_ARC_ARC_AUTH_NOTIFICATION_H_
