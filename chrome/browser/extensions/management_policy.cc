// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/management_policy.h"

namespace extensions {
ManagementPolicy::ManagementPolicy() {
}

ManagementPolicy::~ManagementPolicy() {
}

bool ManagementPolicy::Provider::UserMayLoad(const Extension* extension,
                                             string16* error) const {
  return true;
}

bool ManagementPolicy::Provider::UserMayModifySettings(
    const Extension* extension, string16* error) const {
  return true;
}

bool ManagementPolicy::Provider::MustRemainEnabled(const Extension* extension,
                                                   string16* error) const {
  return false;
}

void ManagementPolicy::RegisterProvider(Provider* provider) {
  providers_.insert(provider);
}

void ManagementPolicy::UnregisterProvider(Provider* provider) {
  providers_.erase(provider);
}

bool ManagementPolicy::UserMayLoad(const Extension* extension,
                                   string16* error) const {
  for (ProviderList::const_iterator it = providers_.begin();
      it != providers_.end(); ++it) {
    if (!(*it)->UserMayLoad(extension, error)) {
      // The extension may be NULL in testing.
      std::string id = extension ? extension->id() : "[test]";
      std::string name = extension ? extension->name() : "test";
      DLOG(WARNING) << "Installation of extension " << name
                    << "( " << id << ")"
                    << " prohibited by " << (*it)->GetDebugPolicyProviderName();
      return false;
    }
  }
  return true;
}

bool ManagementPolicy::UserMayModifySettings(const Extension* extension,
                                             string16* error) const {
  for (ProviderList::const_iterator it = providers_.begin();
      it != providers_.end(); ++it) {
    if (!(*it)->UserMayModifySettings(extension, error)) {
      // The extension may be NULL in testing.
      std::string id = extension ? extension->id() : "[test]";
      std::string name = extension ? extension->name() : "test";
      DLOG(WARNING) << "Modification of extension " << name
                    << "( " << id << ")"
                    << " prohibited by " << (*it)->GetDebugPolicyProviderName();
      return false;
    }
  }
  return true;
}

bool ManagementPolicy::MustRemainEnabled(const Extension* extension,
                                         string16* error) const {
  for (ProviderList::const_iterator it = providers_.begin();
      it != providers_.end(); ++it) {
    if ((*it)->MustRemainEnabled(extension, error)) {
      // The extension may be NULL in testing.
      std::string id = extension ? extension->id() : "[test]";
      std::string name = extension ? extension->name() : "test";
      DLOG(WARNING) << "Extension " << name
                    << "( " << id << ")"
                    << " required to remain enabled by "
                    << (*it)->GetDebugPolicyProviderName();
      return true;
    }
  }
  return false;
}

void ManagementPolicy::UnregisterAllProviders() {
  providers_.clear();
}

int ManagementPolicy::GetNumProviders() const {
  return providers_.size();
}
}  // namespace
