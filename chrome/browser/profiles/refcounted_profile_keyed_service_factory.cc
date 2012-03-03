// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/refcounted_profile_keyed_service_factory.h"

#include "base/logging.h"
#include "chrome/browser/profiles/profile_keyed_service.h"
#include "chrome/browser/profiles/refcounted_profile_keyed_service.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

RefcountedProfileKeyedServiceFactory::RefcountedProfileKeyedServiceFactory(
    const char* name,
    ProfileDependencyManager* manager)
    : ProfileKeyedBaseFactory(name, manager) {
}

RefcountedProfileKeyedServiceFactory::~RefcountedProfileKeyedServiceFactory() {
  DCHECK(mapping_.empty());
}

void RefcountedProfileKeyedServiceFactory::Associate(Profile* profile,
                                                     ProfileKeyedBase* base) {
  DCHECK(mapping_.find(profile) == mapping_.end());
  mapping_.insert(std::make_pair(
      profile, make_scoped_refptr(
          static_cast<RefcountedProfileKeyedService*>(base))));
}

bool RefcountedProfileKeyedServiceFactory::GetAssociation(
    Profile* profile,
    ProfileKeyedBase** out) const {
  bool found = false;
  RefCountedStorage::const_iterator it = mapping_.find(profile);
  if (it != mapping_.end()) {
    found = true;

    if (out)
      *out = it->second.get();
  }
  return found;
}

void RefcountedProfileKeyedServiceFactory::ProfileShutdown(Profile* profile) {
  RefCountedStorage::iterator it = mapping_.find(profile);
  if (it != mapping_.end() && it->second)
    it->second->ShutdownOnUIThread();
}

void RefcountedProfileKeyedServiceFactory::ProfileDestroyed(Profile* profile) {
  RefCountedStorage::iterator it = mapping_.find(profile);
  if (it != mapping_.end()) {
    // We "merely" drop our reference to the service. Hopefully this will cause
    // the service to be destroyed. If not, oh well.
    mapping_.erase(it);
  }

  ProfileKeyedBaseFactory::ProfileDestroyed(profile);
}
