// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LEVELDB_PROTO_PROTO_DATABASE_PROVIDER_FACTORY_H_
#define IOS_CHROME_BROWSER_LEVELDB_PROTO_PROTO_DATABASE_PROVIDER_FACTORY_H_

#include "base/macros.h"
#include "components/keyed_service/ios/browser_state_keyed_service_factory.h"

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}  // namespace base

namespace ios {
class ChromeBrowserState;
}  // namespace ios

namespace leveldb_proto {
class ProtoDatabaseProvider;

// A factory for ProtoDatabaseProvider, a class that provides proto databases
// stored in the appropriate directory given an ios::ChromeBrowserState object.
class ProtoDatabaseProviderFactory : public BrowserStateKeyedServiceFactory {
 public:
  // Returns singleton instance of ProtoDatabaseProviderFactory.
  static ProtoDatabaseProviderFactory* GetInstance();

  // Returns ProtoDatabaseProvider associated with |context|, so we can
  // instantiate ProtoDatabases that use the appropriate profile directory.
  static ProtoDatabaseProvider* GetForBrowserState(
      ios::ChromeBrowserState* browser_state);

 protected:
  // BrowserStateKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      web::BrowserState* context) const override;

 private:
  friend struct base::DefaultSingletonTraits<ProtoDatabaseProviderFactory>;

  ProtoDatabaseProviderFactory();
  ~ProtoDatabaseProviderFactory() override;

  DISALLOW_COPY_AND_ASSIGN(ProtoDatabaseProviderFactory);
};

}  // namespace leveldb_proto

#endif  // IOS_CHROME_BROWSER_LEVELDB_PROTO_PROTO_DATABASE_PROVIDER_FACTORY_H_