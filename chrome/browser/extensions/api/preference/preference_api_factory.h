// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_PREFERENCE_PREFERENCE_API_FACTORY_H_
#define CHROME_BROWSER_EXTENSIONS_API_PREFERENCE_PREFERENCE_API_FACTORY_H_

#include "base/memory/singleton.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace extensions {
class PreferenceAPI;

class PreferenceAPIFactory : public ProfileKeyedServiceFactory {
 public:
  static PreferenceAPI* GetForProfile(Profile* profile);

  static PreferenceAPIFactory* GetInstance();

 private:
  friend struct DefaultSingletonTraits<PreferenceAPIFactory>;

  PreferenceAPIFactory();
  virtual ~PreferenceAPIFactory();

  // ProfileKeyedBaseFactory implementation.
  virtual ProfileKeyedService* BuildServiceInstanceFor(
      Profile* profile) const OVERRIDE;
  virtual bool ServiceIsCreatedWithProfile() const OVERRIDE;
  virtual bool ServiceIsNULLWhileTesting() const OVERRIDE;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_PREFERENCE_PREFERENCE_API_FACTORY_H_
