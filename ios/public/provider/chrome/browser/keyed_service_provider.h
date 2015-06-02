// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_PUBLIC_PROVIDER_CHROME_BROWSER_KEYED_SERVICE_PROVIDER_H_
#define IOS_PUBLIC_PROVIDER_CHROME_BROWSER_KEYED_SERVICE_PROVIDER_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"

enum class ServiceAccessType;

class KeyedServiceBaseFactory;
class ProfileOAuth2TokenService;
class SigninManager;

namespace autofill {
class AutofillWebDataService;
class PersonalDataManager;
}

namespace enhanced_bookmarks {
class EnhancedBookmarkModel;
}

namespace sync_driver {
class SyncService;
}

namespace ios {

class ChromeBrowserState;

// A class that provides access to KeyedService that do not have a pure iOS
// implementation yet.
class KeyedServiceProvider {
 public:
  KeyedServiceProvider();
  virtual ~KeyedServiceProvider();

  // Returns the ProfileOAuth2TokenService factory for dependencies.
  virtual KeyedServiceBaseFactory* GetProfileOAuth2TokenServiceFactory();

  // Returns an instance of ProfileOAuth2TokenService tied to |browser_state|.
  virtual ProfileOAuth2TokenService*
  GetProfileOAuth2TokenServiceForBrowserState(
      ChromeBrowserState* browser_state);

  // Returns the SigninManager factory for dependencies.
  virtual KeyedServiceBaseFactory* GetSigninManagerFactory();

  // Returns an instance of SigninManager tied to |browser_state|.
  virtual SigninManager* GetSigninManagerForBrowserState(
      ChromeBrowserState* browser_state);

  // Returns the autofill::AutofillWebDataService factory for dependencies.
  virtual KeyedServiceBaseFactory* GetAutofillWebDataFactory();

  // Returns an instance of autofill::AutofillWebDataService tied to
  // |browser_state|.
  virtual scoped_refptr<autofill::AutofillWebDataService>
  GetAutofillWebDataForBrowserState(ChromeBrowserState* browser_state,
                                    ServiceAccessType access_type);

  // Returns the autofill::PersonalDataManager factory for dependencies.
  virtual KeyedServiceBaseFactory* GetPersonalDataManagerFactory();

  // Returns an instance of autofill::PersonalDataManager tied to
  // |browser_state|.
  virtual autofill::PersonalDataManager* GetPersonalDataManagerForBrowserState(
      ChromeBrowserState* browser_state);

  // Returns the enhanced_bookmarks::EnhancedBookmarkModel factory for
  // dependencies.
  virtual KeyedServiceBaseFactory* GetEnhancedBookmarkModelFactory();

  // Returns an instance of enhanced_bookmarks::EnhancedBookmarkModel tied to
  // |browser_state|.
  virtual enhanced_bookmarks::EnhancedBookmarkModel*
  GetEnhancedBookmarkModelForBrowserState(ChromeBrowserState* browser_state);

  // Returns the sync_driver::SyncService factory for dependencies.
  virtual KeyedServiceBaseFactory* GetSyncServiceFactory();

  // Returns an instance of sync_driver::SyncService tied to |browser_state|.
  virtual sync_driver::SyncService* GetSyncServiceForBrowserState(
      ChromeBrowserState* browser_state);

 private:
  DISALLOW_COPY_AND_ASSIGN(KeyedServiceProvider);
};

}  // namespace ios

#endif  // IOS_PUBLIC_PROVIDER_CHROME_BROWSER_KEYED_SERVICE_PROVIDER_H_
