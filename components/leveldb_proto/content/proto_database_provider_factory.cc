// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/leveldb_proto/content/proto_database_provider_factory.h"

#include "base/memory/singleton.h"
#include "base/no_destructor.h"
#include "components/keyed_service/core/simple_dependency_manager.h"
#include "components/keyed_service/core/simple_factory_key.h"
#include "components/leveldb_proto/public/proto_database_provider.h"

namespace leveldb_proto {

// static
ProtoDatabaseProviderFactory* ProtoDatabaseProviderFactory::GetInstance() {
  static base::NoDestructor<ProtoDatabaseProviderFactory> instance;
  return instance.get();
}

// static
ProtoDatabaseProvider* ProtoDatabaseProviderFactory::GetForKey(
    SimpleFactoryKey* key) {
  return static_cast<ProtoDatabaseProvider*>(
      GetInstance()->GetServiceForKey(key, true));
}

// static
void ProtoDatabaseProviderFactory::RemoveKeyForTesting(SimpleFactoryKey* key) {
  GetInstance()->SimpleContextDestroyed(key);
}

ProtoDatabaseProviderFactory::ProtoDatabaseProviderFactory()
    : SimpleKeyedServiceFactory("leveldb_proto::ProtoDatabaseProvider",
                                SimpleDependencyManager::GetInstance()) {}

ProtoDatabaseProviderFactory::~ProtoDatabaseProviderFactory() = default;

std::unique_ptr<KeyedService>
ProtoDatabaseProviderFactory::BuildServiceInstanceFor(
    SimpleFactoryKey* key) const {
  return std::make_unique<ProtoDatabaseProvider>(key->GetPath());
}

}  // namespace leveldb_proto
