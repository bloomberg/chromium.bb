// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_MANAGER_FILE_BROWSER_PRIVATE_API_FACTORY_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_MANAGER_FILE_BROWSER_PRIVATE_API_FACTORY_H_

#include "base/memory/singleton.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class FileBrowserPrivateAPI;
class Profile;

class FileBrowserPrivateAPIFactory : public ProfileKeyedServiceFactory {
 public:
  // Returns the FileBrowserPrivateAPI for |profile|, creating it if
  // it is not yet created.
  static FileBrowserPrivateAPI* GetForProfile(Profile* profile);

  // Returns the FileBrowserPrivateAPIFactory instance.
  static FileBrowserPrivateAPIFactory* GetInstance();

 protected:
  // ProfileKeyedBaseFactory overrides:
  virtual content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const OVERRIDE;
  virtual bool ServiceIsCreatedWithProfile() const OVERRIDE;
  virtual bool ServiceIsNULLWhileTesting() const OVERRIDE;

 private:
  friend struct DefaultSingletonTraits<FileBrowserPrivateAPIFactory>;

  FileBrowserPrivateAPIFactory();
  virtual ~FileBrowserPrivateAPIFactory();

  // ProfileKeyedServiceFactory:
  virtual ProfileKeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const OVERRIDE;
};

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_MANAGER_FILE_BROWSER_PRIVATE_API_FACTORY_H_
