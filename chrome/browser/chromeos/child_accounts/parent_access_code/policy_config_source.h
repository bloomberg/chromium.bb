// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CHILD_ACCOUNTS_PARENT_ACCESS_CODE_POLICY_CONFIG_SOURCE_H_
#define CHROME_BROWSER_CHROMEOS_CHILD_ACCOUNTS_PARENT_ACCESS_CODE_POLICY_CONFIG_SOURCE_H_

#include "base/macros.h"
#include "chrome/browser/chromeos/child_accounts/parent_access_code/config_source.h"
#include "components/prefs/pref_change_registrar.h"

class PrefService;

namespace chromeos {
namespace parent_access {

// Provides parent access code configuration from user policy.
class PolicyConfigSource : public ConfigSource {
 public:
  explicit PolicyConfigSource(PrefService* pref_service);
  ~PolicyConfigSource() override;

  ConfigSet GetConfigSet() override;

 private:
  // Updates configuration when policy changes.
  void OnPolicyChanged();

  PrefService* pref_service_ = nullptr;

  PrefChangeRegistrar pref_change_registrar_;

  DISALLOW_COPY_AND_ASSIGN(PolicyConfigSource);
};

}  // namespace parent_access
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CHILD_ACCOUNTS_PARENT_ACCESS_CODE_POLICY_CONFIG_SOURCE_H_
