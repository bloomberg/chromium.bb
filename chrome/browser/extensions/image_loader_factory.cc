// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/image_loader_factory.h"

#include "chrome/browser/extensions/image_loader.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "components/browser_context_keyed_service/browser_context_dependency_manager.h"

namespace extensions {

// static
ImageLoader* ImageLoaderFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<ImageLoader*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

ImageLoaderFactory* ImageLoaderFactory::GetInstance() {
  return Singleton<ImageLoaderFactory>::get();
}

ImageLoaderFactory::ImageLoaderFactory()
    : BrowserContextKeyedServiceFactory(
        "ImageLoader",
        BrowserContextDependencyManager::GetInstance()) {
}

ImageLoaderFactory::~ImageLoaderFactory() {
}

BrowserContextKeyedService* ImageLoaderFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new ImageLoader;
}

bool ImageLoaderFactory::ServiceIsCreatedWithBrowserContext() const {
  return false;
}

content::BrowserContext* ImageLoaderFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  // TODO(jamescook): Find a more generic way to do this in the extensions
  // system without being tied to Chrome code.
  return chrome::GetBrowserContextRedirectedInIncognito(context);
}

}  // namespace extensions
