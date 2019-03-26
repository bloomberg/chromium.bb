// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_CONTENT_FACTORY_ALL_DOWNLOAD_ITEM_NOTIFIER_FACTORY_H_
#define COMPONENTS_DOWNLOAD_CONTENT_FACTORY_ALL_DOWNLOAD_ITEM_NOTIFIER_FACTORY_H_

#include "base/macros.h"
#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace download {

class AllDownloadItemNotifier;

// Creates the AllDownloadItemNotifier instance.
class AllDownloadItemNotifierFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  // Returns singleton instance of AllDownloadItemNotifierFactory.
  static AllDownloadItemNotifierFactory* GetInstance();

  // Helper method to create the AllDownloadItemNotifier instance from
  // |context| if it doesn't exist.
  static download::AllDownloadItemNotifier* GetForBrowserContext(
      content::BrowserContext* context);

 private:
  friend struct base::DefaultSingletonTraits<AllDownloadItemNotifierFactory>;

  AllDownloadItemNotifierFactory();
  ~AllDownloadItemNotifierFactory() override;

  // BrowserContextKeyedServiceFactory implementation.
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;

  DISALLOW_COPY_AND_ASSIGN(AllDownloadItemNotifierFactory);
};

}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_CONTENT_FACTORY_ALL_DOWNLOAD_ITEM_NOTIFIER_FACTORY_H_
