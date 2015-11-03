// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_PUBLIC_PROVIDER_CHROME_BROWSER_KEYED_SERVICE_PROVIDER_H_
#define IOS_PUBLIC_PROVIDER_CHROME_BROWSER_KEYED_SERVICE_PROVIDER_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"

enum class ServiceAccessType;

class KeyedServiceBaseFactory;

#if defined(ENABLE_CONFIGURATION_POLICY)
namespace bookmarks {
class ManagedBookmarkService;
}
#endif

namespace data_reduction_proxy {
class DataReductionProxySettings;
}

namespace invalidation {
class ProfileInvalidationProvider;
}

namespace sync_driver {
class SyncService;
}

namespace ios {

class ChromeBrowserState;
class KeyedServiceProvider;

// Registers and returns the global KeyedService provider.
void SetKeyedServiceProvider(KeyedServiceProvider* provider);
KeyedServiceProvider* GetKeyedServiceProvider();

// A class that provides access to KeyedService that do not have a pure iOS
// implementation yet.
class KeyedServiceProvider {
 public:
  KeyedServiceProvider();
  virtual ~KeyedServiceProvider();

  // Ensures that all KeyedService factories are instantiated. Must be called
  // before any BrowserState instance is created so that dependencies are
  // correct.
  void AssertKeyedFactoriesBuilt();

#if defined(ENABLE_CONFIGURATION_POLICY)
  // Returns the bookmarks::ManagedBookmarkService factory for dependencies.
  virtual KeyedServiceBaseFactory* GetManagedBookmarkServiceFactory() = 0;

  // Returns an instance of bookmarks::ManagedBookmarkService tied to
  // |browser_state|.
  virtual bookmarks::ManagedBookmarkService*
  GetManagedBookmarkServiceForBrowserState(
      ChromeBrowserState* browser_state) = 0;
#endif

  // Returns the sync_driver::SyncService factory for dependencies.
  virtual KeyedServiceBaseFactory* GetSyncServiceFactory() = 0;

  // Returns an instance of sync_driver::SyncService tied to |browser_state|.
  virtual sync_driver::SyncService* GetSyncServiceForBrowserState(
      ChromeBrowserState* browser_state) = 0;

  // Returns an instance of sync_driver::SyncService tied to |browser_state| if
  // there is one created already.
  virtual sync_driver::SyncService* GetSyncServiceForBrowserStateIfExists(
      ChromeBrowserState* browser_state) = 0;

  // Returns the invalidation::ProfileInvalidationProvider factory for
  // dependencies.
  virtual KeyedServiceBaseFactory* GetProfileInvalidationProviderFactory() = 0;

  // Returns an instance of invalidation::ProfileInvalidationProvider tied to
  // |browser_state|.
  virtual invalidation::ProfileInvalidationProvider*
  GetProfileInvalidationProviderForBrowserState(
      ChromeBrowserState* browser_state) = 0;

  // Returns the data_reduction_proxy::DataReductionProxySettings factory for
  // dependencies.
  virtual KeyedServiceBaseFactory* GetDataReductionProxySettingsFactory() = 0;

  // Returns an instance of data_reduction_proxy::DataReductionProxySettings
  // tied to |browser_state|.
  virtual data_reduction_proxy::DataReductionProxySettings*
  GetDataReductionProxySettingsForBrowserState(
      ios::ChromeBrowserState* browser_state) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(KeyedServiceProvider);
};

}  // namespace ios

#endif  // IOS_PUBLIC_PROVIDER_CHROME_BROWSER_KEYED_SERVICE_PROVIDER_H_
