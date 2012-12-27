// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_OMNIBOX_OMNIBOX_API_FACTORY_H_
#define CHROME_BROWSER_EXTENSIONS_API_OMNIBOX_OMNIBOX_API_FACTORY_H_

#include "base/memory/singleton.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace extensions {

class OmniboxAPI;

class OmniboxAPIFactory : public ProfileKeyedServiceFactory {
 public:
  static OmniboxAPI* GetForProfile(Profile* profile);
  static OmniboxAPIFactory* GetInstance();

 private:
  friend struct DefaultSingletonTraits<OmniboxAPIFactory>;

  OmniboxAPIFactory();
  virtual ~OmniboxAPIFactory();

  // ProfileKeyedServiceFactory implementation.
  virtual ProfileKeyedService* BuildServiceInstanceFor(
      Profile* profile) const OVERRIDE;
  virtual bool ServiceIsCreatedWithProfile() const OVERRIDE;
  virtual bool ServiceRedirectedInIncognito() const OVERRIDE;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_OMNIBOX_OMNIBOX_API_FACTORY_H_
