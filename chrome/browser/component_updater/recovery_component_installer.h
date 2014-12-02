// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMPONENT_UPDATER_RECOVERY_COMPONENT_INSTALLER_H_
#define CHROME_BROWSER_COMPONENT_UPDATER_RECOVERY_COMPONENT_INSTALLER_H_

#include "base/callback_forward.h"

class PrefRegistrySimple;
class PrefService;

namespace component_updater {

class ComponentUpdateService;

// Component update registration for the recovery component. The job of the
// recovery component is to repair the chrome installation or repair the Google
// update installation. This is a last resort safety mechanism.
void RegisterRecoveryComponent(ComponentUpdateService* cus, PrefService* prefs);

// Registers user preferences related to the recovery component.
void RegisterPrefsForRecoveryComponent(PrefRegistrySimple* registry);

// Notifies recovery component that agreed elevated install, so that it
// can launches an elevated install process. After that, clears
// preference flag prefs::kRecoveryComponentNeedsElevation.
// Note: the function is yet to be implemented.
void AcceptedElevatedRecoveryInstall(PrefService* prefs);

// Notifies recovery component that elevated install is declined, so that it can
// removes the setup files. After that, clears preference flag
// prefs::kRecoveryComponentNeedsElevation.
// Note: the function is yet to be implemented.
void DeclinedElevatedRecoveryInstall(PrefService* prefs);

}  // namespace component_updater

#endif  // CHROME_BROWSER_COMPONENT_UPDATER_RECOVERY_COMPONENT_INSTALLER_H_
