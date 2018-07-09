// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_SIGNIN_UPDATER_FACTORY_H_
#define CHROME_BROWSER_SIGNIN_SIGNIN_UPDATER_FACTORY_H_

#include "base/macros.h"
#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class Profile;
class SigninUpdater;

// Singleton that owns all SigninUpdater and creates/deletes them as
// new Profiles are created/shutdown.
class SigninUpdaterFactory : public BrowserContextKeyedServiceFactory {
 public:
  // Returns an instance of the SigninUpdaterFactory singleton.
  static SigninUpdaterFactory* GetInstance();

  // Returns the instance of SigninUpdater for the passed |profile|.
  static SigninUpdater* GetForProfile(Profile* profile);

 protected:
  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;

 private:
  friend struct base::DefaultSingletonTraits<SigninUpdaterFactory>;

  SigninUpdaterFactory();
  ~SigninUpdaterFactory() override;

  DISALLOW_COPY_AND_ASSIGN(SigninUpdaterFactory);
};

#endif  // CHROME_BROWSER_SIGNIN_SIGNIN_UPDATER_FACTORY_H_
