// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/power/brightness_observer.h"

#include "chrome/browser/chromeos/ui/brightness_bubble.h"
#include "chrome/browser/chromeos/ui/volume_bubble.h"
#include "chrome/browser/extensions/system/system_api.h"
#include "chromeos/dbus/dbus_thread_manager.h"

namespace chromeos {

BrightnessObserver::BrightnessObserver() {
  DBusThreadManager::Get()->GetPowerManagerClient()->AddObserver(this);
}

BrightnessObserver::~BrightnessObserver() {
  DBusThreadManager::Get()->GetPowerManagerClient()->RemoveObserver(this);
}

void BrightnessObserver::BrightnessChanged(int level, bool user_initiated) {
  if (user_initiated)
    BrightnessBubble::GetInstance()->ShowBubble(level, true);
  else
    BrightnessBubble::GetInstance()->UpdateWithoutShowingBubble(level, true);

  extensions::DispatchBrightnessChangedEvent(level, user_initiated);

  VolumeBubble::GetInstance()->HideBubble();
}

}  // namespace chromeos
