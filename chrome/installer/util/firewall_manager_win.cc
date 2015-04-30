// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/firewall_manager_win.h"

#include "base/files/file_path.h"
#include "base/strings/string16.h"
#include "chrome/installer/util/advanced_firewall_manager_win.h"
#include "chrome/installer/util/browser_distribution.h"
#include "chrome/installer/util/install_util.h"
#include "chrome/installer/util/installer_util_strings.h"
#include "chrome/installer/util/l10n_string_util.h"
#include "chrome/installer/util/legacy_firewall_manager_win.h"

namespace installer {

namespace {

const uint16 kDefaultMdnsPort = 5353;

class FirewallManagerAdvancedImpl : public FirewallManager {
 public:
  FirewallManagerAdvancedImpl() {}
  ~FirewallManagerAdvancedImpl() override {}

  bool Init(const base::string16& app_name, const base::FilePath& app_path) {
    return manager_.Init(app_name, app_path);
  }

  // FirewallManager methods.
  bool CanUseLocalPorts() override {
    return !manager_.IsFirewallEnabled() || manager_.HasAnyRule();
  };

  bool AddFirewallRules() override {
    return manager_.AddUDPRule(GetMdnsRuleName(), GetMdnsRuleDescription(),
                               kDefaultMdnsPort);
  }

  void RemoveFirewallRules() override {
    manager_.DeleteAllRules();
  }

 private:
  static base::string16 GetMdnsRuleName() {
#if defined(GOOGLE_CHROME_BUILD)
    if (InstallUtil::IsChromeSxSProcess())
      return GetLocalizedString(IDS_INBOUND_MDNS_RULE_NAME_CANARY_BASE);
#endif
    return GetLocalizedString(IDS_INBOUND_MDNS_RULE_NAME_BASE);
  }

  static base::string16 GetMdnsRuleDescription() {
#if defined(GOOGLE_CHROME_BUILD)
    if (InstallUtil::IsChromeSxSProcess())
      return GetLocalizedString(IDS_INBOUND_MDNS_RULE_DESCRIPTION_CANARY_BASE);
#endif
      return GetLocalizedString(IDS_INBOUND_MDNS_RULE_DESCRIPTION_BASE);
  }

  AdvancedFirewallManager manager_;
  DISALLOW_COPY_AND_ASSIGN(FirewallManagerAdvancedImpl);
};

class FirewallManagerLegacyImpl : public FirewallManager {
 public:
  FirewallManagerLegacyImpl() {}
  ~FirewallManagerLegacyImpl() override {}

  bool Init(const base::string16& app_name, const base::FilePath& app_path) {
    return manager_.Init(app_name, app_path);
  }

  // FirewallManager methods.
  bool CanUseLocalPorts() override {
    return !manager_.IsFirewallEnabled() ||
        manager_.GetAllowIncomingConnection(NULL);
  };

  bool AddFirewallRules() override {
    // Change nothing if rule is set.
    return manager_.GetAllowIncomingConnection(NULL) ||
        manager_.SetAllowIncomingConnection(true);
  }

  void RemoveFirewallRules() override {
    manager_.DeleteRule();
  }

 private:
  LegacyFirewallManager manager_;
  DISALLOW_COPY_AND_ASSIGN(FirewallManagerLegacyImpl);
};

}  // namespace

FirewallManager::~FirewallManager() {}

// static
scoped_ptr<FirewallManager> FirewallManager::Create(
    BrowserDistribution* dist,
    const base::FilePath& chrome_path) {
  // First try to connect to "Windows Firewall with Advanced Security" (Vista+).
  scoped_ptr<FirewallManagerAdvancedImpl> manager(
      new FirewallManagerAdvancedImpl());
  if (manager->Init(dist->GetDisplayName(), chrome_path))
    return manager.Pass();

  // Next try to connect to "Windows Firewall for Windows XP with SP2".
  scoped_ptr<FirewallManagerLegacyImpl> legacy_manager(
      new FirewallManagerLegacyImpl());
  if (legacy_manager->Init(dist->GetDisplayName(), chrome_path))
    return legacy_manager.Pass();

  return scoped_ptr<FirewallManager>();
}

FirewallManager::FirewallManager() {
}

}  // namespace installer
