// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/blacklist.h"

#include <algorithm>

#include "base/bind.h"
#include "base/message_loop.h"
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

void Blacklist::GetBlacklistedIDs(const std::set<std::string>& ids,
                                  const GetBlacklistedIDsCallback& callback) {
  // TODO(kalman): Get the blacklisted IDs from the safebrowsing list.
  // This will require going to the IO thread and back.
  std::set<std::string> blacklisted_ids;
  for (std::set<std::string>::const_iterator it = ids.begin();
       it != ids.end(); ++it) {
    if (prefs_->IsExtensionBlacklisted(*it))
      blacklisted_ids.insert(*it);
  }
  MessageLoop::current()->PostTask(FROM_HERE,
                                   base::Bind(callback, blacklisted_ids));
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

  std::set<std::string> from_prefs = prefs_->GetBlacklistedExtensions();

  std::set<std::string> no_longer_blacklisted;
  std::set_difference(from_prefs.begin(), from_prefs.end(),
                      ids_as_set.begin(), ids_as_set.end(),
                      std::inserter(no_longer_blacklisted,
                                    no_longer_blacklisted.begin()));
  std::set<std::string> not_yet_blacklisted;
  std::set_difference(ids_as_set.begin(), ids_as_set.end(),
                      from_prefs.begin(), from_prefs.end(),
                      std::inserter(not_yet_blacklisted,
                                    not_yet_blacklisted.begin()));

  for (std::set<std::string>::iterator it = no_longer_blacklisted.begin();
       it != no_longer_blacklisted.end(); ++it) {
    prefs_->SetExtensionBlacklisted(*it, false);
  }
  for (std::set<std::string>::iterator it = not_yet_blacklisted.begin();
       it != not_yet_blacklisted.end(); ++it) {
    prefs_->SetExtensionBlacklisted(*it, true);
  }

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
