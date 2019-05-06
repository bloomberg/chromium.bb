// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LEVELDB_PROTO_CONTENT_PROTO_DATABASE_PROVIDER_FACTORY_H_
#define COMPONENTS_LEVELDB_PROTO_CONTENT_PROTO_DATABASE_PROVIDER_FACTORY_H_

#include <memory>

#include "base/macros.h"
#include "components/keyed_service/core/simple_keyed_service_factory.h"

class KeyedService;
class SimpleFactoryKey;

namespace base {
template <typename T>
class NoDestructor;
}  // namespace base

namespace leveldb_proto {
class ProtoDatabaseProvider;

// A factory for ProtoDatabaseProvider, a class that provides proto databases
// stored in the appropriate directory for the current profile.
class ProtoDatabaseProviderFactory : public SimpleKeyedServiceFactory {
 public:
  // Returns singleton instance of ProtoDatabaseProviderFactory.
  static ProtoDatabaseProviderFactory* GetInstance();

  // Returns ProtoDatabaseProvider associated with |key|, so we can
  // instantiate ProtoDatabases that use the appropriate profile directory.
  static ProtoDatabaseProvider* GetForKey(SimpleFactoryKey* key);

  // Some tests may get databases with manually created keys, this method
  // removes a key from the mapping to avoid reusing a database from a past
  // test.
  static void RemoveKeyForTesting(SimpleFactoryKey* key);

 private:
  friend class base::NoDestructor<ProtoDatabaseProviderFactory>;

  ProtoDatabaseProviderFactory();
  ~ProtoDatabaseProviderFactory() override;

  // SimpleKeyedServiceFactory overrides:
  std::unique_ptr<KeyedService> BuildServiceInstanceFor(
      SimpleFactoryKey* key) const override;

  DISALLOW_COPY_AND_ASSIGN(ProtoDatabaseProviderFactory);
};

}  // namespace leveldb_proto

#endif  // COMPONENTS_LEVELDB_PROTO_CONTENT_PROTO_DATABASE_PROVIDER_FACTORY_H_
