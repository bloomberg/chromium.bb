// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/device_local_account.h"

#include <set>

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chromeos/login/user_names.h"
#include "chromeos/settings/cros_settings_names.h"
#include "google_apis/gaia/gaia_auth_util.h"

namespace policy {

namespace {

const char kPublicAccountDomainPrefix[] = "public-accounts";
const char kKioskAppAccountDomainPrefix[] = "kiosk-apps";
const char kDeviceLocalAccountDomainSuffix[] = ".device-local.localhost";

}  // namespace

DeviceLocalAccount::DeviceLocalAccount(Type type,
                                       const std::string& account_id,
                                       const std::string& kiosk_app_id)
    : type(type),
      account_id(account_id),
      user_id(GenerateDeviceLocalAccountUserId(account_id, type)),
      kiosk_app_id(kiosk_app_id) {
}

DeviceLocalAccount::~DeviceLocalAccount() {
}

std::string GenerateDeviceLocalAccountUserId(const std::string& account_id,
                                             DeviceLocalAccount::Type type) {
  std::string domain_prefix;
  switch (type) {
    case DeviceLocalAccount::TYPE_PUBLIC_SESSION:
      domain_prefix = kPublicAccountDomainPrefix;
      break;
    case DeviceLocalAccount::TYPE_KIOSK_APP:
      domain_prefix = kKioskAppAccountDomainPrefix;
      break;
    case DeviceLocalAccount::TYPE_COUNT:
      NOTREACHED();
      break;
  }
  return gaia::CanonicalizeEmail(
      base::HexEncode(account_id.c_str(), account_id.size()) + "@" +
      domain_prefix + kDeviceLocalAccountDomainSuffix);
}

bool IsDeviceLocalAccountUser(const std::string& user_id,
                              DeviceLocalAccount::Type* type) {
  // For historical reasons, the guest user ID does not contain an @ symbol and
  // therefore, cannot be parsed by gaia::ExtractDomainName().
  if (user_id == chromeos::login::kGuestUserName)
    return false;
  const std::string domain = gaia::ExtractDomainName(user_id);
  if (!EndsWith(domain, kDeviceLocalAccountDomainSuffix, true))
    return false;

  const std::string domain_prefix = domain.substr(
      0, domain.size() - arraysize(kDeviceLocalAccountDomainSuffix) + 1);

  if (domain_prefix == kPublicAccountDomainPrefix) {
    if (type)
      *type = DeviceLocalAccount::TYPE_PUBLIC_SESSION;
    return true;
  }
  if (domain_prefix == kKioskAppAccountDomainPrefix) {
    if (type)
      *type = DeviceLocalAccount::TYPE_KIOSK_APP;
    return true;
  }

  // |user_id| is a device-local account but its type is not recognized.
  NOTREACHED();
  if (type)
    *type = DeviceLocalAccount::TYPE_COUNT;
  return true;
}

void SetDeviceLocalAccounts(
    chromeos::CrosSettings* cros_settings,
    const std::vector<DeviceLocalAccount>& accounts) {
  base::ListValue list;
  for (std::vector<DeviceLocalAccount>::const_iterator it = accounts.begin();
       it != accounts.end(); ++it) {
    scoped_ptr<base::DictionaryValue> entry(new base::DictionaryValue);
    entry->SetStringWithoutPathExpansion(
        chromeos::kAccountsPrefDeviceLocalAccountsKeyId,
        it->account_id);
    entry->SetIntegerWithoutPathExpansion(
        chromeos::kAccountsPrefDeviceLocalAccountsKeyType,
        it->type);
    if (it->type == DeviceLocalAccount::TYPE_KIOSK_APP) {
      entry->SetStringWithoutPathExpansion(
          chromeos::kAccountsPrefDeviceLocalAccountsKeyKioskAppId,
          it->kiosk_app_id);
    }
    list.Append(entry.release());
  }

  cros_settings->Set(chromeos::kAccountsPrefDeviceLocalAccounts, list);
}

std::vector<DeviceLocalAccount> GetDeviceLocalAccounts(
    chromeos::CrosSettings* cros_settings) {
  std::vector<DeviceLocalAccount> accounts;

  const base::ListValue* list = NULL;
  cros_settings->GetList(chromeos::kAccountsPrefDeviceLocalAccounts, &list);
  if (!list)
    return accounts;

  std::set<std::string> account_ids;
  for (size_t i = 0; i < list->GetSize(); ++i) {
    const base::DictionaryValue* entry = NULL;
    if (!list->GetDictionary(i, &entry)) {
      LOG(ERROR) << "Corrupt entry in device-local account list at index " << i
                 << ".";
      continue;
    }

    std::string account_id;
    if (!entry->GetStringWithoutPathExpansion(
            chromeos::kAccountsPrefDeviceLocalAccountsKeyId, &account_id) ||
        account_id.empty()) {
      LOG(ERROR) << "Missing account ID in device-local account list at index "
                 << i << ".";
      continue;
    }

    int type;
    if (!entry->GetIntegerWithoutPathExpansion(
            chromeos::kAccountsPrefDeviceLocalAccountsKeyType, &type) ||
        type < 0 || type >= DeviceLocalAccount::TYPE_COUNT) {
      LOG(ERROR) << "Missing or invalid account type in device-local account "
                 << "list at index " << i << ".";
      continue;
    }

    std::string kiosk_app_id;
    if (type == DeviceLocalAccount::TYPE_KIOSK_APP) {
      if (!entry->GetStringWithoutPathExpansion(
              chromeos::kAccountsPrefDeviceLocalAccountsKeyKioskAppId,
              &kiosk_app_id)) {
        LOG(ERROR) << "Missing app ID in device-local account entry at index "
                   << i << ".";
        continue;
      }
    }

    if (!account_ids.insert(account_id).second) {
      LOG(ERROR) << "Duplicate entry in device-local account list at index "
                 << i << ": " << account_id << ".";
      continue;
    }

    accounts.push_back(DeviceLocalAccount(
        static_cast<DeviceLocalAccount::Type>(type), account_id, kiosk_app_id));
  }
  return accounts;
}

}  // namespace policy
