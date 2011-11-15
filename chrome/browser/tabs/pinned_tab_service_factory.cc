// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tabs/pinned_tab_service_factory.h"

#include "chrome/browser/tabs/pinned_tab_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_dependency_manager.h"

namespace {
base::LazyInstance<PinnedTabServiceFactory> g_pinned_tab_service_factory =
    LAZY_INSTANCE_INITIALIZER;
}

// static
void PinnedTabServiceFactory::InitForProfile(Profile* profile) {
  g_pinned_tab_service_factory.Get().GetServiceForProfile(profile, true);
}

PinnedTabServiceFactory::PinnedTabServiceFactory()
    : ProfileKeyedServiceFactory(ProfileDependencyManager::GetInstance()) {
}

PinnedTabServiceFactory::~PinnedTabServiceFactory() {
}

ProfileKeyedService* PinnedTabServiceFactory::BuildServiceInstanceFor(
    Profile* profile) const {
  return new PinnedTabService(profile);
}
