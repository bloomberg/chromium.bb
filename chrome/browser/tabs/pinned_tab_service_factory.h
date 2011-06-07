// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TABS_PINNED_TAB_SERVICE_FACTORY_H_
#define CHROME_BROWSER_TABS_PINNED_TAB_SERVICE_FACTORY_H_

#include "base/compiler_specific.h"
#include "base/lazy_instance.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class PinnedTabService;
class Profile;

// Singleton that owns all PinnedTabServices and associates them with Profiles.
// Listens for the Profile's destruction notification and cleans up the
// associated PinnedTabService.
class PinnedTabServiceFactory : public ProfileKeyedServiceFactory {
 public:
  // Creates and initializes a PinnedTabService to track pinning changes for
  // |profile|.
  static void InitForProfile(Profile* profile);

 private:
  friend struct base::DefaultLazyInstanceTraits<PinnedTabServiceFactory>;

  PinnedTabServiceFactory();
  virtual ~PinnedTabServiceFactory();

  // ProfileKeyedServiceFactory:
  virtual ProfileKeyedService* BuildServiceInstanceFor(
      Profile* profile) const OVERRIDE;
};

#endif  // CHROME_BROWSER_TABS_PINNED_TAB_SERVICE_FACTORY_H_
