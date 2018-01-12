// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APPS_SAVED_FILES_SERVICE_FACTORY_H_
#define APPS_SAVED_FILES_SERVICE_FACTORY_H_

#include "base/macros.h"
#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace content {
class BrowserContext;
}

namespace apps {

class SavedFilesService;

// BrowserContextKeyedServiceFactory for SavedFilesService.
class SavedFilesServiceFactory : public BrowserContextKeyedServiceFactory {
 public:
  static SavedFilesService* GetForBrowserContext(
      content::BrowserContext* context);

  static SavedFilesService* GetForBrowserContextIfExists(
      content::BrowserContext* context);

  static SavedFilesServiceFactory* GetInstance();

 private:
  SavedFilesServiceFactory();
  ~SavedFilesServiceFactory() override;
  friend struct base::DefaultSingletonTraits<SavedFilesServiceFactory>;

  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;

  DISALLOW_COPY_AND_ASSIGN(SavedFilesServiceFactory);
};

}  // namespace apps

#endif  // APPS_SAVED_FILES_SERVICE_FACTORY_H_
