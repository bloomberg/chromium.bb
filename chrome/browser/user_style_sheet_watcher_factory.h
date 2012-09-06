// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_USER_STYLE_SHEET_WATCHER_FACTORY_H_
#define CHROME_BROWSER_USER_STYLE_SHEET_WATCHER_FACTORY_H_

#include "base/memory/singleton.h"
#include "chrome/browser/profiles/refcounted_profile_keyed_service_factory.h"

class Profile;
class UserStyleSheetWatcher;

// Singleton that owns all UserStyleSheetWatcher and associates them with
// Profiles.
class UserStyleSheetWatcherFactory
    : public RefcountedProfileKeyedServiceFactory {
 public:
  static scoped_refptr<UserStyleSheetWatcher> GetForProfile(Profile* profile);

  static UserStyleSheetWatcherFactory* GetInstance();

 private:
  friend struct DefaultSingletonTraits<UserStyleSheetWatcherFactory>;

  UserStyleSheetWatcherFactory();
  virtual ~UserStyleSheetWatcherFactory();

  // ProfileKeyedServiceFactory:
  virtual scoped_refptr<RefcountedProfileKeyedService> BuildServiceInstanceFor(
      Profile* profile) const OVERRIDE;
  virtual bool ServiceRedirectedInIncognito() const OVERRIDE;
  virtual bool ServiceIsNULLWhileTesting() const OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(UserStyleSheetWatcherFactory);
};

#endif  // CHROME_BROWSER_USER_STYLE_SHEET_WATCHER_FACTORY_H_
