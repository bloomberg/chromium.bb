// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_PLUGIN_VM_PLUGIN_VM_UTIL_H_
#define CHROME_BROWSER_CHROMEOS_PLUGIN_VM_PLUGIN_VM_UTIL_H_

#include <string>

class Profile;

namespace plugin_vm {

// Generated as crx_file::id_util::GenerateId("org.chromium.plugin_vm");
constexpr char kPluginVmAppId[] = "lgjpclljbbmphhnalkeplcmnjpfmmaek";

// Name of the Plugin VM.
constexpr char kPluginVmName[] = "PluginVm";

// Checks if PluginVm is allowed for the current profile.
bool IsPluginVmAllowedForProfile(const Profile* profile);

// Checks if PluginVm is configured for the current profile.
bool IsPluginVmConfigured(Profile* profile);

// Returns true if PluginVm is allowed and configured for the current profile.
bool IsPluginVmEnabled(Profile* profile);

void ShowPluginVmLauncherView(Profile* profile);

// Checks if an exo window's app id is for plugin vm.
bool IsPluginVmExoApplicationId(const std::string& app_id);

// Retrieves the license key to be used for PluginVm. If
// none is set this will return an empty string.
std::string GetPluginVmLicenseKey();

}  // namespace plugin_vm

#endif  // CHROME_BROWSER_CHROMEOS_PLUGIN_VM_PLUGIN_VM_UTIL_H_
