// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_service_factory.h"

#include "chrome/browser/download/download_service.h"
#include "chrome/browser/profiles/profile_dependency_manager.h"

// static
DownloadService* DownloadServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<DownloadService*>(
      GetInstance()->GetServiceForProfile(profile, true));
}

// static
DownloadServiceFactory* DownloadServiceFactory::GetInstance() {
  return Singleton<DownloadServiceFactory>::get();
}

DownloadServiceFactory::DownloadServiceFactory()
    : ProfileKeyedServiceFactory(
        ProfileDependencyManager::GetInstance()) {
  // TODO(rdsmith): For Shutdown() order we need to:
  //    DependsOn(HistoryServiceDataFactory::GetInstance());
}

DownloadServiceFactory::~DownloadServiceFactory() {
}

ProfileKeyedService* DownloadServiceFactory::BuildServiceInstanceFor(
    Profile* profile) const {
  DownloadService* service = new DownloadService(profile);

  // No need for initialization; initialization can be done on first
  // use of service.

  return service;
}

bool DownloadServiceFactory::ServiceHasOwnInstanceInIncognito() {
  return true;
}
