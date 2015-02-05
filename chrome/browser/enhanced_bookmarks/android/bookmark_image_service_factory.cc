// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enhanced_bookmarks/android/bookmark_image_service_android.h"
#include "chrome/browser/enhanced_bookmarks/android/bookmark_image_service_factory.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "content/public/browser/browser_context.h"

namespace enhanced_bookmarks {

// static
BookmarkImageServiceFactory* BookmarkImageServiceFactory::GetInstance() {
  return Singleton<BookmarkImageServiceFactory>::get();
}

// static
BookmarkImageService* BookmarkImageServiceFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<BookmarkImageService*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

BookmarkImageServiceFactory::BookmarkImageServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "BookmarkExtendedStorageService",
          BrowserContextDependencyManager::GetInstance()) {}

BookmarkImageServiceFactory::~BookmarkImageServiceFactory() {}

content::BrowserContext* BookmarkImageServiceFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return context;
}

BookmarkImageService* BookmarkImageServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* browser_context) const {
  return new BookmarkImageServiceAndroid(browser_context);
}

}  // namespace enhanced_bookmarks
