// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMPONENT_UPDATER_SW_REPORTER_INSTALLER_WIN_H_
#define CHROME_BROWSER_COMPONENT_UPDATER_SW_REPORTER_INSTALLER_WIN_H_

class PrefRegistrySimple;
class PrefService;

namespace component_updater {

class ComponentUpdateService;

// Execute the SwReporter if one is available, otherwise 1) register with the
// component updater, 2) save a preference in |prefs| identifying that an
// attempt to execute the reporter tool was made, so that calls to
// ExecutePending below will be able to continue the work if it doesn't complete
// before the Chrome session ends. It will also allow the component updater to
// know if there is a pending execution or not when an update happens. This must
// be called from the UI thread.
void ExecuteSwReporter(ComponentUpdateService* cus, PrefService* prefs);

// ExecutePending will only register/execute the SwReporter if a preference is
// set in |prefs|, identifying that an attempt to register/execute the
// SwReporter has already been made by calling ExecuteSwReporter above. But only
// if the max number of retries has not been reached yet. This must be called
// from the UI thread.
void ExecutePendingSwReporter(ComponentUpdateService* cus, PrefService* prefs);

// Register user preferences related to the SwReporter.
void RegisterPrefsForSwReporter(PrefRegistrySimple* registry);

}  // namespace component_updater

#endif  // CHROME_BROWSER_COMPONENT_UPDATER_SW_REPORTER_INSTALLER_WIN_H_
