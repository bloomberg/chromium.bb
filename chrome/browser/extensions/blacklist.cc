// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/blacklist.h"

#include "chrome/browser/extensions/extension_prefs.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/common/pref_names.h"

namespace extensions {

Blacklist::Observer::Observer(Blacklist* blacklist) : blacklist_(blacklist) {
  blacklist_->AddObserver(this);
}

Blacklist::Observer::~Observer() {
  blacklist_->RemoveObserver(this);
}

Blacklist::Blacklist(ExtensionPrefs* prefs) : prefs_(prefs) {
}

Blacklist::~Blacklist() {
}

bool Blacklist::IsBlacklisted(const Extension* extension) const {
  return prefs_->IsExtensionBlacklisted(extension->id());
}

bool Blacklist::IsBlacklisted(const std::string& extension_id) const {
  return prefs_->IsExtensionBlacklisted(extension_id);
}

void Blacklist::SetFromUpdater(const std::vector<std::string>& ids,
                               const std::string& version) {
  std::set<std::string> ids_as_set;
  for (std::vector<std::string>::const_iterator it = ids.begin();
       it != ids.end(); ++it) {
    if (Extension::IdIsValid(*it))
      ids_as_set.insert(*it);
    else
      LOG(WARNING) << "Got invalid extension ID \"" << *it << "\"";
  }

  prefs_->UpdateBlacklist(ids_as_set);
  prefs_->pref_service()->SetString(prefs::kExtensionBlacklistUpdateVersion,
                                    version);

  FOR_EACH_OBSERVER(Observer, observers_, OnBlacklistUpdated());
}

void Blacklist::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void Blacklist::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

}  // namespace extensions
