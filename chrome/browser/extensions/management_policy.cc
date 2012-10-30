// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/management_policy.h"

namespace extensions {

namespace {

void GetExtensionNameAndId(const Extension* extension,
                           std::string* name,
                           std::string* id) {
  // The extension may be NULL in testing.
  *id = extension ? extension->id() : "[test]";
  *name = extension ? extension->name() : "test";
}

}  // namespace

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
      std::string id;
      std::string name;
      GetExtensionNameAndId(extension, &name, &id);
      DVLOG(1) << "Installation of extension " << name
               << " (" << id << ")"
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
      std::string id;
      std::string name;
      GetExtensionNameAndId(extension, &name, &id);
      DVLOG(1) << "Modification of extension " << name
               << " (" << id << ")"
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
      std::string id;
      std::string name;
      GetExtensionNameAndId(extension, &name, &id);
      DVLOG(1) << "Extension " << name
               << " (" << id << ")"
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

}  // namespace extensions
