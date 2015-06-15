// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/public/provider/chrome/browser/keyed_service_provider.h"

#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"

namespace ios {
namespace {
KeyedServiceProvider* g_keyed_service_provider = nullptr;
}  // namespace

void SetKeyedServiceProvider(KeyedServiceProvider* provider) {
  // Since the dependency between KeyedService is only resolved at instantiation
  // time, forbid un-installation or overridden the global KeyedServiceProvider.
  DCHECK(provider && !g_keyed_service_provider);
  g_keyed_service_provider = provider;
}

KeyedServiceProvider* GetKeyedServiceProvider() {
  return g_keyed_service_provider;
}

KeyedServiceProvider::KeyedServiceProvider() {
}

KeyedServiceProvider::~KeyedServiceProvider() {
}

void KeyedServiceProvider::AssertKeyedFactoriesBuilt() {
}

KeyedServiceBaseFactory* KeyedServiceProvider::GetBookmarkModelFactory() {
  return nullptr;
}

bookmarks::BookmarkModel* KeyedServiceProvider::GetBookmarkModelForBrowserState(
    ios::ChromeBrowserState* browser_state) {
  return nullptr;
}

KeyedServiceBaseFactory*
KeyedServiceProvider::GetProfileOAuth2TokenServiceIOSFactory() {
  return nullptr;
}

ProfileOAuth2TokenServiceIOS*
KeyedServiceProvider::GetProfileOAuth2TokenServiceIOSForBrowserState(
    ChromeBrowserState* browser_state) {
  return nullptr;
}

KeyedServiceBaseFactory* KeyedServiceProvider::GetSigninManagerFactory() {
  return nullptr;
}

SigninManager* KeyedServiceProvider::GetSigninManagerForBrowserState(
    ChromeBrowserState* browser_state) {
  return nullptr;
}

KeyedServiceBaseFactory* KeyedServiceProvider::GetAutofillWebDataFactory() {
  return nullptr;
}

scoped_refptr<autofill::AutofillWebDataService>
KeyedServiceProvider::GetAutofillWebDataForBrowserState(
    ChromeBrowserState* browser_state,
    ServiceAccessType access_type) {
  return nullptr;
}

KeyedServiceBaseFactory* KeyedServiceProvider::GetPersonalDataManagerFactory() {
  return nullptr;
}

autofill::PersonalDataManager*
KeyedServiceProvider::GetPersonalDataManagerForBrowserState(
    ChromeBrowserState* browser_state) {
  return nullptr;
}

KeyedServiceBaseFactory* KeyedServiceProvider::GetSyncServiceFactory() {
  return nullptr;
}

sync_driver::SyncService* KeyedServiceProvider::GetSyncServiceForBrowserState(
    ChromeBrowserState* browser_state) {
  return nullptr;
}

KeyedServiceBaseFactory* KeyedServiceProvider::GetHistoryServiceFactory() {
  return nullptr;
}

history::HistoryService* KeyedServiceProvider::GetHistoryServiceForBrowserState(
    ChromeBrowserState* browser_state,
    ServiceAccessType access_type) {
  return nullptr;
}

}  // namespace ios
