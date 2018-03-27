// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/crostini/crostini_util.h"

#include "base/feature_list.h"
#include "chrome/browser/chromeos/virtual_machines/virtual_machines_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_features.h"

namespace {

constexpr char kCrostiniAppIdPrefix[] = "crostini:";

}  // namespace

bool IsCrostiniAllowed() {
  return base::FeatureList::IsEnabled(features::kCrostini) &&
         virtual_machines::AreVirtualMachinesAllowedByPolicy();
}

bool IsExperimentalCrostiniUIAvailable() {
  return IsCrostiniAllowed() &&
         base::FeatureList::IsEnabled(features::kExperimentalCrostiniUI);
}

bool IsCrostiniInstalled() {
  return false;
}

bool IsCrostiniRunning() {
  return false;
}

std::string CreateCrostiniAppId(const std::string& window_app_id) {
  DCHECK(!IsCrostiniAppId(window_app_id));
  return kCrostiniAppIdPrefix + window_app_id;
}

bool IsCrostiniAppId(const std::string& app_id) {
  return strncmp(app_id.c_str(), kCrostiniAppIdPrefix,
                 sizeof(kCrostiniAppIdPrefix)) == 0;
}
