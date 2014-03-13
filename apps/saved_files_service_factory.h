// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APPS_SAVED_FILES_SERVICE_FACTORY_H_
#define APPS_SAVED_FILES_SERVICE_FACTORY_H_

#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class Profile;

namespace apps {

class SavedFilesService;

// BrowserContextKeyedServiceFactory for SavedFilesService.
class SavedFilesServiceFactory : public BrowserContextKeyedServiceFactory {
 public:
  static SavedFilesService* GetForProfile(Profile* profile);

  static SavedFilesServiceFactory* GetInstance();

 private:
  SavedFilesServiceFactory();
  virtual ~SavedFilesServiceFactory();
  friend struct DefaultSingletonTraits<SavedFilesServiceFactory>;

  virtual KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const OVERRIDE;
};

}  // namespace apps

#endif  // APPS_SAVED_FILES_SERVICE_FACTORY_H_
