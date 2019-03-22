// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/content/factory/all_download_item_notifier_factory.h"

#include "components/download/content/public/all_download_item_notifier.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "content/public/browser/browser_context.h"

namespace download {

// static
AllDownloadItemNotifierFactory* AllDownloadItemNotifierFactory::GetInstance() {
  return base::Singleton<AllDownloadItemNotifierFactory>::get();
}

download::AllDownloadItemNotifier*
AllDownloadItemNotifierFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  // Ensure that the download manager has been created.
  content::BrowserContext::GetDownloadManager(context);
  return static_cast<download::AllDownloadItemNotifier*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

AllDownloadItemNotifierFactory::AllDownloadItemNotifierFactory()
    : BrowserContextKeyedServiceFactory(
          "download::AllDownloadItemNotifier",
          BrowserContextDependencyManager::GetInstance()) {}

AllDownloadItemNotifierFactory::~AllDownloadItemNotifierFactory() = default;

KeyedService* AllDownloadItemNotifierFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new AllDownloadItemNotifier(
      content::BrowserContext::GetDownloadManager(context));
}

content::BrowserContext* AllDownloadItemNotifierFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return context;
}

}  // namespace download
