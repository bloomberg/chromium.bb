// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_STORAGE_DURABLE_STORAGE_PERMISSION_CONTEXT_FACTORY_H_
#define CHROME_BROWSER_STORAGE_DURABLE_STORAGE_PERMISSION_CONTEXT_FACTORY_H_

#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class DurableStoragePermissionContext;
class Profile;

class DurableStoragePermissionContextFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  static DurableStoragePermissionContext* GetForProfile(Profile* profile);
  static DurableStoragePermissionContextFactory* GetInstance();

 private:
  friend struct DefaultSingletonTraits<DurableStoragePermissionContextFactory>;

  DurableStoragePermissionContextFactory();
  ~DurableStoragePermissionContextFactory() override = default;

  // BrowserContextKeyedBaseFactory methods:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;

  DISALLOW_COPY_AND_ASSIGN(DurableStoragePermissionContextFactory);
};

#endif  // CHROME_BROWSER_STORAGE_DURABLE_STORAGE_PERMISSION_CONTEXT_FACTORY_H_
