// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/printing/device_external_printers_settings_bridge.h"

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/values.h"
#include "chrome/browser/chromeos/printing/device_external_printers_factory.h"
#include "chrome/browser/chromeos/printing/external_printers.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"

namespace chromeos {

namespace {

// Extracts the list of strings named |policy_name| from |settings| and returns
// it.
std::vector<std::string> FromSettings(const CrosSettings* settings,
                                      const std::string& policy_name) {
  std::vector<std::string> string_list;
  const base::ListValue* list = nullptr;
  if (!settings->GetList(policy_name, &list) || !list) {
    return string_list;
  }

  for (const base::Value& value : *list) {
    if (value.is_string()) {
      string_list.push_back(value.GetString());
    }
  }

  return string_list;
}

}  // namespace

DeviceExternalPrintersSettingsBridge::DeviceExternalPrintersSettingsBridge(
    const ExternalPrinterPolicies& policies,
    CrosSettings* settings)
    : settings_(settings), policies_(policies) {
  access_mode_subscription_ = settings->AddSettingsObserver(
      policies_.access_mode,
      base::BindRepeating(
          &DeviceExternalPrintersSettingsBridge::AccessModeUpdated,
          base::Unretained(this)));
  blacklist_subscription_ = settings->AddSettingsObserver(
      policies_.blacklist,
      base::BindRepeating(
          &DeviceExternalPrintersSettingsBridge::BlacklistUpdated,
          base::Unretained(this)));
  whitelist_subscription_ = settings->AddSettingsObserver(
      policies_.whitelist,
      base::BindRepeating(
          &DeviceExternalPrintersSettingsBridge::WhitelistUpdated,
          base::Unretained(this)));
  Initialize();
}

DeviceExternalPrintersSettingsBridge::~DeviceExternalPrintersSettingsBridge() =
    default;

void DeviceExternalPrintersSettingsBridge::Initialize() {
  BlacklistUpdated();
  WhitelistUpdated();
  AccessModeUpdated();
}

void DeviceExternalPrintersSettingsBridge::AccessModeUpdated() {
  int mode_val;
  // Settings should contain value for access mode device setting.
  // Even if it's not pushed with device policy, the default value should be
  // set.
  CHECK(settings_->GetInteger(policies_.access_mode, &mode_val));
  ExternalPrinters::AccessMode mode =
      static_cast<ExternalPrinters::AccessMode>(mode_val);

  base::WeakPtr<ExternalPrinters> printers =
      DeviceExternalPrintersFactory::Get()->GetForDevice();
  if (printers)
    printers->SetAccessMode(mode);
}

void DeviceExternalPrintersSettingsBridge::BlacklistUpdated() {
  base::WeakPtr<ExternalPrinters> printers =
      DeviceExternalPrintersFactory::Get()->GetForDevice();
  if (printers)
    printers->SetBlacklist(FromSettings(settings_, policies_.blacklist));
}

void DeviceExternalPrintersSettingsBridge::WhitelistUpdated() {
  base::WeakPtr<ExternalPrinters> printers =
      DeviceExternalPrintersFactory::Get()->GetForDevice();
  if (printers)
    printers->SetWhitelist(FromSettings(settings_, policies_.whitelist));
}

}  // namespace chromeos
