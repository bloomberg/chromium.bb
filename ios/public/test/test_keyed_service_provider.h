// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_PUBLIC_TEST_TEST_KEYED_SERVICE_PROVIDER_H_
#define IOS_PUBLIC_TEST_TEST_KEYED_SERVICE_PROVIDER_H_

#include "base/macros.h"
#include "ios/public/provider/chrome/browser/keyed_service_provider.h"

namespace ios {

class TestKeyedServiceProvider : public KeyedServiceProvider {
 public:
  TestKeyedServiceProvider();
  ~TestKeyedServiceProvider() override;

  // KeyedServiceProvider implementation:
  void AssertKeyedFactoriesBuilt() override;
  KeyedServiceBaseFactory* GetBookmarkModelFactory() override;
  bookmarks::BookmarkModel* GetBookmarkModelForBrowserState(
      ChromeBrowserState* browser_state) override;
  KeyedServiceBaseFactory* GetProfileOAuth2TokenServiceIOSFactory() override;
  ProfileOAuth2TokenServiceIOS* GetProfileOAuth2TokenServiceIOSForBrowserState(
      ChromeBrowserState* browser_state) override;
  KeyedServiceBaseFactory* GetSigninManagerFactory() override;
  SigninManager* GetSigninManagerForBrowserState(
      ChromeBrowserState* browser_state) override;
  KeyedServiceBaseFactory* GetAutofillWebDataFactory() override;
  scoped_refptr<autofill::AutofillWebDataService>
  GetAutofillWebDataForBrowserState(ChromeBrowserState* browser_state,
                                    ServiceAccessType access_type) override;
  KeyedServiceBaseFactory* GetPersonalDataManagerFactory() override;
  autofill::PersonalDataManager* GetPersonalDataManagerForBrowserState(
      ChromeBrowserState* browser_state) override;
  KeyedServiceBaseFactory* GetSyncServiceFactory() override;
  sync_driver::SyncService* GetSyncServiceForBrowserState(
      ChromeBrowserState* browser_state) override;
  KeyedServiceBaseFactory* GetHistoryServiceFactory() override;
  history::HistoryService* GetHistoryServiceForBrowserState(
      ChromeBrowserState* browser_state,
      ServiceAccessType access_type) override;
  history::HistoryService* GetHistoryServiceForBrowserStateIfExists(
      ChromeBrowserState* browser_state,
      ServiceAccessType access_type) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(TestKeyedServiceProvider);
};

}  // namespace ios

#endif  // IOS_PUBLIC_TEST_TEST_KEYED_SERVICE_PROVIDER_H_
