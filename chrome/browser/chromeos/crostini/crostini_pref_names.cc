// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/crostini/crostini_pref_names.h"

#include <memory>
#include <utility>

#include "base/values.h"
#include "chrome/browser/chromeos/crostini/crostini_util.h"
#include "components/prefs/pref_registry_simple.h"

namespace crostini {
namespace prefs {

// A boolean preference representing whether a user has opted in to use
// Crostini (Called "Linux Apps" in UI).
const char kCrostiniEnabled[] = "crostini.enabled";
const char kCrostiniMimeTypes[] = "crostini.mime_types";
const char kCrostiniRegistry[] = "crostini.registry";
// List of filesystem paths that are shared with the crostini container.
// TODO(crbug.com/946273): Remove crostini.shared_paths and migration code after
// M77.
const char kCrostiniSharedPaths[] = "crostini.shared_paths";
// List of USB devices with their system guid, a name/description and their
// enabled state for use with Crostini.
const char kCrostiniSharedUsbDevices[] = "crostini.shared_usb_devices";
const char kCrostiniContainers[] = "crostini.containers";
const char kVmKey[] = "vm_name";
const char kContainerKey[] = "container_name";

// A boolean preference representing a user level enterprise policy to enable
// Crostini use.
const char kUserCrostiniAllowedByPolicy[] = "crostini.user_allowed_by_policy";
// A boolean preference representing a user level enterprise policy to enable
// the crostini export / import UI.
const char kUserCrostiniExportImportUIAllowedByPolicy[] =
    "crostini.user_export_import_ui_allowed_by_policy";

// A boolean preference controlling Crostini usage reporting.
const char kReportCrostiniUsageEnabled[] = "crostini.usage_reporting_enabled";
// Preferences used to store last launch information for reporting:
const char kCrostiniLastLaunchVersion[] = "crostini.last_launch.version";
// The start of a three day window of the last app launch
// stored as Java time (ms since epoch).
const char kCrostiniLastLaunchTimeWindowStart[] =
    "crostini.last_launch.time_window_start";
// The value of the last sample of the disk space used by Crostini.
const char kCrostiniLastDiskSize[] = "crostini.last_disk_size";

// Dictionary of filesystem paths mapped to the list of VMs that the paths are
// shared with.
const char kGuestOSPathsSharedToVms[] = "guest_os.paths_shared_to_vms";

void RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(kCrostiniEnabled, false);
  registry->RegisterDictionaryPref(kCrostiniMimeTypes);
  registry->RegisterDictionaryPref(kCrostiniRegistry);
  registry->RegisterListPref(kCrostiniSharedPaths);
  registry->RegisterListPref(kCrostiniSharedUsbDevices);
  registry->RegisterDictionaryPref(kGuestOSPathsSharedToVms);

  // Set a default value for crostini.containers to ensure that we track the
  // default container even if its creation predates this preference. This
  // preference should not be accessed unless crostini is installed
  // (i.e. kCrostiniEnabled is true).
  base::Value default_container(base::Value::Type::DICTIONARY);
  default_container.SetKey(kVmKey, base::Value(kCrostiniDefaultVmName));
  default_container.SetKey(kContainerKey,
                           base::Value(kCrostiniDefaultContainerName));

  base::Value::ListStorage default_containers_list;
  default_containers_list.push_back(std::move(default_container));
  registry->RegisterListPref(kCrostiniContainers,
                             base::Value(std::move(default_containers_list)));

  registry->RegisterBooleanPref(crostini::prefs::kReportCrostiniUsageEnabled,
                                false);
  registry->RegisterStringPref(kCrostiniLastLaunchVersion, std::string());
  registry->RegisterInt64Pref(kCrostiniLastLaunchTimeWindowStart, 0u);
  registry->RegisterInt64Pref(kCrostiniLastDiskSize, 0u);
  registry->RegisterBooleanPref(kUserCrostiniAllowedByPolicy, true);
  registry->RegisterBooleanPref(kUserCrostiniExportImportUIAllowedByPolicy,
                                true);
}

}  // namespace prefs
}  // namespace crostini
