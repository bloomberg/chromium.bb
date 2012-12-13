// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/history/history_api_factory.h"

#include "chrome/browser/extensions/api/history/history_api.h"
#include "chrome/browser/extensions/extension_system_factory.h"
#include "chrome/browser/profiles/profile_dependency_manager.h"

namespace extensions {

// static
HistoryAPI* HistoryAPIFactory::GetForProfile(
    Profile* profile) {
  return static_cast<HistoryAPI*>(
      GetInstance()->GetServiceForProfile(profile, true));
}

// static
HistoryAPIFactory* HistoryAPIFactory::GetInstance() {
  return Singleton<HistoryAPIFactory>::get();
}

HistoryAPIFactory::HistoryAPIFactory()
    : ProfileKeyedServiceFactory("HistoryAPI",
                                 ProfileDependencyManager::GetInstance()) {
  DependsOn(ExtensionSystemFactory::GetInstance());
}

HistoryAPIFactory::~HistoryAPIFactory() {
}

ProfileKeyedService* HistoryAPIFactory::BuildServiceInstanceFor(
    Profile* profile) const {
  return new HistoryAPI(profile);
}

bool HistoryAPIFactory::ServiceIsCreatedWithProfile() const {
  return true;
}

bool HistoryAPIFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

}  // namespace extensions
