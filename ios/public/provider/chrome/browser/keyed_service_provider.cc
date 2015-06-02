// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/public/provider/chrome/browser/keyed_service_provider.h"

#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"

namespace ios {

KeyedServiceProvider::KeyedServiceProvider() {
}

KeyedServiceProvider::~KeyedServiceProvider() {
}

KeyedServiceBaseFactory*
KeyedServiceProvider::GetProfileOAuth2TokenServiceFactory() {
  return nullptr;
}

ProfileOAuth2TokenService*
KeyedServiceProvider::GetProfileOAuth2TokenServiceForBrowserState(
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

KeyedServiceBaseFactory*
KeyedServiceProvider::GetEnhancedBookmarkModelFactory() {
  return nullptr;
}

enhanced_bookmarks::EnhancedBookmarkModel*
KeyedServiceProvider::GetEnhancedBookmarkModelForBrowserState(
    ChromeBrowserState* browser_state) {
  return nullptr;
}

KeyedServiceBaseFactory* KeyedServiceProvider::GetSyncServiceFactory() {
  return nullptr;
}

sync_driver::SyncService* KeyedServiceProvider::GetSyncServiceForBrowserState(
    ios::ChromeBrowserState* browser_state) {
  return nullptr;
}

}  // namespace ios
