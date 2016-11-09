// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/reading_list/reading_list_model_factory.h"

#include <utility>

#include "base/memory/ptr_util.h"
#include "base/memory/singleton.h"
#include "components/keyed_service/ios/browser_state_dependency_manager.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "ios/chrome/browser/browser_state/browser_state_otr_helper.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/reading_list/reading_list_model_impl.h"
#include "ios/chrome/browser/reading_list/reading_list_model_storage_defaults.h"
#include "ios/chrome/browser/reading_list/reading_list_pref_names.h"

// static
ReadingListModel* ReadingListModelFactory::GetForBrowserState(
    ios::ChromeBrowserState* browser_state) {
  return static_cast<ReadingListModelImpl*>(
      GetInstance()->GetServiceForBrowserState(browser_state, true));
}

// static
ReadingListModel* ReadingListModelFactory::GetForBrowserStateIfExists(
    ios::ChromeBrowserState* browser_state) {
  return static_cast<ReadingListModelImpl*>(
      GetInstance()->GetServiceForBrowserState(browser_state, false));
}

// static
ReadingListModelFactory* ReadingListModelFactory::GetInstance() {
  return base::Singleton<ReadingListModelFactory>::get();
}

ReadingListModelFactory::ReadingListModelFactory()
    : BrowserStateKeyedServiceFactory(
          "ReadingListModel",
          BrowserStateDependencyManager::GetInstance()) {}

ReadingListModelFactory::~ReadingListModelFactory() {}

void ReadingListModelFactory::RegisterBrowserStatePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(
      reading_list::prefs::kReadingListHasUnseenEntries, false,
      PrefRegistry::NO_REGISTRATION_FLAGS);
}

std::unique_ptr<KeyedService> ReadingListModelFactory::BuildServiceInstanceFor(
    web::BrowserState* context) const {
  auto storage = base::MakeUnique<ReadingListModelStorageDefaults>();
  ios::ChromeBrowserState* chrome_browser_state =
      ios::ChromeBrowserState::FromBrowserState(context);
  std::unique_ptr<ReadingListModelImpl> reading_list_model(
      new ReadingListModelImpl(std::move(storage),
                               chrome_browser_state->GetPrefs()));
  return reading_list_model;
}

web::BrowserState* ReadingListModelFactory::GetBrowserStateToUse(
    web::BrowserState* context) const {
  return GetBrowserStateRedirectedInIncognito(context);
}
