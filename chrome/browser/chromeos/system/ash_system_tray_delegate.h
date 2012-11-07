// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_SYSTEM_ASH_SYSTEM_TRAY_DELEGATE_H_
#define CHROME_BROWSER_CHROMEOS_SYSTEM_ASH_SYSTEM_TRAY_DELEGATE_H_

namespace ash {
class SystemTray;
class SystemTrayDelegate;
}

namespace chromeos {
ash::SystemTrayDelegate* CreateSystemTrayDelegate();
}

#endif  // CHROME_BROWSER_CHROMEOS_SYSTEM_ASH_SYSTEM_TRAY_DELEGATE_H_
