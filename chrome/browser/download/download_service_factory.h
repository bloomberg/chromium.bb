// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_DOWNLOAD_SERVICE_FACTORY_H_
#define CHROME_BROWSER_DOWNLOAD_DOWNLOAD_SERVICE_FACTORY_H_

#include "base/compiler_specific.h"
#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class DownloadService;

// Singleton that owns all DownloadServices and associates them with
// Profiles. Listens for the Profile's destruction notification and cleans up
// the associated DownloadService.
class DownloadServiceFactory : public BrowserContextKeyedServiceFactory {
 public:
  // Returns the DownloadService for |context|, creating if not yet created.
  static DownloadService* GetForBrowserContext(
      content::BrowserContext* context);

  static DownloadServiceFactory* GetInstance();

 protected:
  // BrowserContextKeyedServiceFactory:
  virtual KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const OVERRIDE;
  virtual content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const OVERRIDE;

 private:
  friend struct DefaultSingletonTraits<DownloadServiceFactory>;

  DownloadServiceFactory();
  virtual ~DownloadServiceFactory();
};

#endif  // CHROME_BROWSER_DOWNLOAD_DOWNLOAD_SERVICE_FACTORY_H_
