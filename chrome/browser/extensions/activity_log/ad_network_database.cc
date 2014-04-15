// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/activity_log/ad_network_database.h"

#include "base/lazy_instance.h"

namespace extensions {

namespace {

class AdNetworkDatabaseFactory {
 public:
  AdNetworkDatabaseFactory();
  ~AdNetworkDatabaseFactory();

  void SetDatabase(scoped_ptr<AdNetworkDatabase> database);

  const AdNetworkDatabase* database() const { return database_.get(); }

 private:
  scoped_ptr<AdNetworkDatabase> database_;
};

AdNetworkDatabaseFactory::AdNetworkDatabaseFactory() {}
AdNetworkDatabaseFactory::~AdNetworkDatabaseFactory() {}

void AdNetworkDatabaseFactory::SetDatabase(
    scoped_ptr<AdNetworkDatabase> database) {
  database_.reset(database.release());
}

base::LazyInstance<AdNetworkDatabaseFactory> g_factory =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

AdNetworkDatabase::~AdNetworkDatabase() {}

// static
const AdNetworkDatabase* AdNetworkDatabase::Get() {
  return g_factory.Get().database();
}

// static
void AdNetworkDatabase::SetForTesting(scoped_ptr<AdNetworkDatabase> database) {
  g_factory.Get().SetDatabase(database.Pass());
}

}  // namespace extensions
