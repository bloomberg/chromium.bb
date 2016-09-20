// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_SYSTEM_TRAY_CONTROLLER_MUS_H_
#define CHROME_BROWSER_UI_ASH_SYSTEM_TRAY_CONTROLLER_MUS_H_

#include "ash/public/interfaces/system_tray.mojom.h"
#include "base/macros.h"
#include "chrome/browser/chromeos/system/system_clock_observer.h"

// Controls chrome's interaction with the ash system tray menu.
class SystemTrayControllerMus : public chromeos::system::SystemClockObserver {
 public:
  SystemTrayControllerMus();
  ~SystemTrayControllerMus() override;

 private:
  // chromeos::system::SystemClockObserver:
  void OnSystemClockChanged(chromeos::system::SystemClock* clock) override;

  ash::mojom::SystemTrayPtr system_tray_;

  DISALLOW_COPY_AND_ASSIGN(SystemTrayControllerMus);
};

#endif  // CHROME_BROWSER_UI_ASH_SYSTEM_TRAY_CONTROLLER_MUS_H_
