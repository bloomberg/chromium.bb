// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/image_loader_factory.h"

#include "chrome/browser/extensions/image_loader.h"
#include "chrome/browser/profiles/profile_dependency_manager.h"

namespace extensions {

// static
ImageLoader* ImageLoaderFactory::GetForProfile(Profile* profile) {
  return static_cast<ImageLoader*>(
      GetInstance()->GetServiceForProfile(profile, true));
}

// static
void ImageLoaderFactory::ResetForProfile(Profile* profile) {
  ImageLoaderFactory* factory = GetInstance();
  factory->ProfileShutdown(profile);
  factory->ProfileDestroyed(profile);
}

ImageLoaderFactory* ImageLoaderFactory::GetInstance() {
  return Singleton<ImageLoaderFactory>::get();
}

ImageLoaderFactory::ImageLoaderFactory()
    : ProfileKeyedServiceFactory("ImageLoader",
                                 ProfileDependencyManager::GetInstance()) {
}

ImageLoaderFactory::~ImageLoaderFactory() {
}

ProfileKeyedService* ImageLoaderFactory::BuildServiceInstanceFor(
    Profile* profile) const {
  return new ImageLoader;
}

bool ImageLoaderFactory::ServiceIsCreatedWithProfile() const {
  return false;
}

bool ImageLoaderFactory::ServiceRedirectedInIncognito() const {
  return true;
}

}  // namespace extensions
