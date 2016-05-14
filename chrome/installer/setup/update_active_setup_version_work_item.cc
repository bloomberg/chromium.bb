// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/setup/update_active_setup_version_work_item.h"

#include <stdint.h>

#include <vector>

#include "base/bind.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"

namespace {

// The major version and first component of the version identifying the work
// done by setup.exe --configure-user-settings on user login by way of Active
// Setup.  Increase this value if the work done when handling Active Setup
// should be executed again for all existing users.
const base::char16 kActiveSetupMajorVersion[] = L"43";

}  // namespace

UpdateActiveSetupVersionWorkItem::UpdateActiveSetupVersionWorkItem(
    const base::string16& active_setup_path,
    Operation operation)
    : set_reg_value_work_item_(
          HKEY_LOCAL_MACHINE,
          active_setup_path,
          WorkItem::kWow64Default,
          L"Version",
          base::Bind(
              &UpdateActiveSetupVersionWorkItem::GetUpdatedActiveSetupVersion,
              base::Unretained(this))),
      operation_(operation) {
}

bool UpdateActiveSetupVersionWorkItem::Do() {
  return set_reg_value_work_item_.Do();
}

void UpdateActiveSetupVersionWorkItem::Rollback() {
  set_reg_value_work_item_.Rollback();
}

base::string16 UpdateActiveSetupVersionWorkItem::GetUpdatedActiveSetupVersion(
    const base::string16& existing_version) {
  std::vector<base::string16> version_components = base::SplitString(
      existing_version, L",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

  // If |existing_version| was empty or otherwise corrupted, turn it into a
  // valid one.
  if (version_components.size() != 4U)
    version_components.assign(4U, L"0");

  // Unconditionally update the major version.
  version_components[MAJOR] = kActiveSetupMajorVersion;

  if (operation_ == UPDATE_AND_BUMP_OS_UPGRADES_COMPONENT) {
    uint32_t previous_value;
    if (!base::StringToUint(version_components[OS_UPGRADES], &previous_value)) {
      LOG(WARNING) << "Couldn't process previous OS_UPGRADES Active Setup "
                      "version component: " << version_components[OS_UPGRADES];
      previous_value = 0;
    }
    version_components[OS_UPGRADES] = base::UintToString16(previous_value + 1);
  }

  return base::JoinString(version_components, L",");
}
