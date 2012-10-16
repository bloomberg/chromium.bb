// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/thumbnails/thumbnail_service_factory.h"

#include "base/logging.h"
#include "chrome/browser/thumbnails/thumbnail_service.h"
#include "chrome/browser/thumbnails/thumbnail_service_impl.h"
#include "chrome/browser/profiles/profile_dependency_manager.h"

using thumbnails::ThumbnailService;
using thumbnails::ThumbnailServiceImpl;

ThumbnailServiceFactory::ThumbnailServiceFactory()
    : RefcountedProfileKeyedServiceFactory(
          "ThumbnailService",
          ProfileDependencyManager::GetInstance()) {
}

ThumbnailServiceFactory::~ThumbnailServiceFactory() {
}

// static
scoped_refptr<ThumbnailService> ThumbnailServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<ThumbnailService*>(
      GetInstance()->GetServiceForProfile(profile, true).get());
}

// static
ThumbnailServiceFactory* ThumbnailServiceFactory::GetInstance() {
  return Singleton<ThumbnailServiceFactory>::get();
}

scoped_refptr<RefcountedProfileKeyedService>
    ThumbnailServiceFactory::BuildServiceInstanceFor(Profile* profile) const {
  return scoped_refptr<RefcountedProfileKeyedService>(
      new ThumbnailServiceImpl(profile));
}
