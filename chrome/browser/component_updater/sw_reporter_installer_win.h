// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMPONENT_UPDATER_SW_REPORTER_INSTALLER_WIN_H_
#define CHROME_BROWSER_COMPONENT_UPDATER_SW_REPORTER_INSTALLER_WIN_H_

class PrefRegistrySimple;
class PrefService;

namespace component_updater {

class ComponentUpdateService;

// Call once during startup to make the component update service aware of the
// SwReporter.
void RegisterSwReporterComponent(ComponentUpdateService* cus,
                                 PrefService* prefs);

// Register user preferences related to the SwReporter.
void RegisterPrefsForSwReporter(PrefRegistrySimple* registry);

}  // namespace component_updater

#endif  // CHROME_BROWSER_COMPONENT_UPDATER_SW_REPORTER_INSTALLER_WIN_H_
