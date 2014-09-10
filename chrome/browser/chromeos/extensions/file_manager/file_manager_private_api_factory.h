// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_MANAGER_FILE_MANAGER_PRIVATE_API_FACTORY_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_MANAGER_FILE_MANAGER_PRIVATE_API_FACTORY_H_

#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class Profile;

namespace file_manager {

class FileManagerPrivateAPI;

class FileManagerPrivateAPIFactory : public BrowserContextKeyedServiceFactory {
 public:
  // Returns the FileManagerPrivateAPI for |profile|, creating it if
  // it is not yet created.
  static FileManagerPrivateAPI* GetForProfile(Profile* profile);

  // Returns the FileManagerPrivateAPIFactory instance.
  static FileManagerPrivateAPIFactory* GetInstance();

 protected:
  // BrowserContextKeyedBaseFactory overrides:
  virtual content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const OVERRIDE;
  virtual bool ServiceIsCreatedWithBrowserContext() const OVERRIDE;
  virtual bool ServiceIsNULLWhileTesting() const OVERRIDE;

 private:
  friend struct DefaultSingletonTraits<FileManagerPrivateAPIFactory>;

  FileManagerPrivateAPIFactory();
  virtual ~FileManagerPrivateAPIFactory();

  // BrowserContextKeyedServiceFactory:
  virtual KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const OVERRIDE;
};

}  // namespace file_manager

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_FILE_MANAGER_FILE_MANAGER_PRIVATE_API_FACTORY_H_
