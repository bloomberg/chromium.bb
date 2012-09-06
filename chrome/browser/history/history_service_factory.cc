// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/history/history_service_factory.h"

#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/history/history.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile_dependency_manager.h"
#include "chrome/common/pref_names.h"

// static
scoped_refptr<HistoryService> HistoryServiceFactory::GetForProfile(
    Profile* profile, Profile::ServiceAccessType sat) {
  // If saving history is disabled, only allow explicit access.
  if (profile->GetPrefs()->GetBoolean(prefs::kSavingBrowserHistoryDisabled) &&
      sat != Profile::EXPLICIT_ACCESS)
    return NULL;

  return static_cast<HistoryService*>(
      GetInstance()->GetServiceForProfile(profile, true).get());
}

// static
scoped_refptr<HistoryService>
HistoryServiceFactory::GetForProfileIfExists(
    Profile* profile, Profile::ServiceAccessType sat) {
  // If saving history is disabled, only allow explicit access.
  if (profile->GetPrefs()->GetBoolean(prefs::kSavingBrowserHistoryDisabled) &&
      sat != Profile::EXPLICIT_ACCESS)
    return NULL;

  return static_cast<HistoryService*>(
      GetInstance()->GetServiceForProfile(profile, false).get());
}

// static
scoped_refptr<HistoryService>
HistoryServiceFactory::GetForProfileWithoutCreating(Profile* profile) {
  return static_cast<HistoryService*>(
      GetInstance()->GetServiceForProfile(profile, false).get());
}

// static
HistoryServiceFactory* HistoryServiceFactory::GetInstance() {
  return Singleton<HistoryServiceFactory>::get();
}

// static
void HistoryServiceFactory::ShutdownForProfile(Profile* profile) {
  HistoryServiceFactory* factory = GetInstance();
  factory->ProfileDestroyed(profile);
}

HistoryServiceFactory::HistoryServiceFactory()
    : RefcountedProfileKeyedServiceFactory(
          "HistoryService", ProfileDependencyManager::GetInstance()) {
  DependsOn(BookmarkModelFactory::GetInstance());
}

HistoryServiceFactory::~HistoryServiceFactory() {
}

scoped_refptr<RefcountedProfileKeyedService>
HistoryServiceFactory::BuildServiceInstanceFor(Profile* profile) const {
  scoped_refptr<HistoryService> history_service(
      new HistoryService(profile));
  if (!history_service->Init(profile->GetPath(),
                             BookmarkModelFactory::GetForProfile(profile))) {
    return NULL;
  }
  return history_service;
}

bool HistoryServiceFactory::ServiceRedirectedInIncognito() const {
  return true;
}

bool HistoryServiceFactory::ServiceIsNULLWhileTesting() const {
  return true;
}
