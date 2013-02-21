// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMPONENT_UPDATER_RECOVERY_COMPONENT_INSTALLER_H_
#define CHROME_BROWSER_COMPONENT_UPDATER_RECOVERY_COMPONENT_INSTALLER_H_

class ComponentUpdateService;
class PrefRegistrySimple;
class PrefService;

// Component update registration for the recovery component. The job of the
// recovery component is to repair the chrome installation or repair the Google
// update installation. This is a last resort safety mechanism.
void RegisterRecoveryComponent(ComponentUpdateService* cus,
                               PrefService* prefs);


// Register user preferences related to the recovery component.
void RegisterPrefsForRecoveryComponent(PrefRegistrySimple* registry);

#endif  // CHROME_BROWSER_COMPONENT_UPDATER_RECOVERY_COMPONENT_INSTALLER_H_

