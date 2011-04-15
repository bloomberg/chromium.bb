// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/background_contents_service_factory.h"

#include "base/command_line.h"
#include "chrome/browser/background_contents_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_dependency_manager.h"

// static
BackgroundContentsService* BackgroundContentsServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<BackgroundContentsService*>(
      GetInstance()->GetServiceForProfile(profile));
}

// static
BackgroundContentsServiceFactory* BackgroundContentsServiceFactory::
    GetInstance() {
  return Singleton<BackgroundContentsServiceFactory>::get();
}

BackgroundContentsServiceFactory::BackgroundContentsServiceFactory()
    : ProfileKeyedServiceFactory(ProfileDependencyManager::GetInstance()) {
}

BackgroundContentsServiceFactory::~BackgroundContentsServiceFactory() {
}

ProfileKeyedService* BackgroundContentsServiceFactory::BuildServiceInstanceFor(
    Profile* profile) const {
  return new BackgroundContentsService(profile,
                                       CommandLine::ForCurrentProcess());
}

bool BackgroundContentsServiceFactory::ServiceHasOwnInstanceInIncognito() {
  return true;
}
