// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CROSTINI_CROSTINI_UTIL_H_
#define CHROME_BROWSER_CHROMEOS_CROSTINI_CROSTINI_UTIL_H_

#include <string>

class Profile;

// Returns true if crostini is allowed to run.
// Otherwise, returns false, e.g. if crostini is not available on the device,
// or it is in the flow to set up managed account creation.
bool IsCrostiniAllowed();

// Returns true if crostini UI can be shown. Implies crostini is allowed to run.
bool IsExperimentalCrostiniUIAvailable();

// |app_id| should be a valid Crostini app list id.
void LaunchCrostiniApp(Profile* profile, const std::string& app_id);

constexpr char kCrostiniTerminalAppName[] = "Terminal";
// We can use any arbitrary well-formed extension id for the Terminal app, this
// is equal to GenerateId("Terminal").
constexpr char kCrostiniTerminalId[] = "oajcgpnkmhaalajejhlfpacbiokdnnfe";

constexpr char kCrostiniDefaultVmName[] = "termina";
constexpr char kCrostiniDefaultContainerName[] = "penguin";
constexpr char kCrostiniCroshBuiltinAppId[] =
    "nkoccljplnhpfnfiajclkommnmllphnl";

#endif  // CHROME_BROWSER_CHROMEOS_CROSTINI_CROSTINI_UTIL_H_
