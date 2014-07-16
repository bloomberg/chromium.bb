// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_SESSION_KIOSK_AUTO_LAUNCHER_SESSION_MANAGER_DELEGATE_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_SESSION_KIOSK_AUTO_LAUNCHER_SESSION_MANAGER_DELEGATE_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "components/session_manager/core/session_manager.h"

namespace chromeos {

class KioskAutoLauncherSessionManagerDelegate
    : public session_manager::SessionManagerDelegate {
 public:
  KioskAutoLauncherSessionManagerDelegate();
  virtual ~KioskAutoLauncherSessionManagerDelegate();

 private:
  // session_manager::SessionManagerDelegate implementation:
  virtual void Start() OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(KioskAutoLauncherSessionManagerDelegate);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_SESSION_KIOSK_AUTO_LAUNCHER_SESSION_MANAGER_DELEGATE_H_
