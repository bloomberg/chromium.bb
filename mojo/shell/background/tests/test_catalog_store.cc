// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/shell/background/tests/test_catalog_store.h"

using catalog::Store;

namespace mojo {
namespace shell {

TestCatalogStore::TestCatalogStore(scoped_ptr<base::ListValue> store)
    : store_(std::move(store)) {}

TestCatalogStore::~TestCatalogStore() {}

const base::ListValue* TestCatalogStore::GetStore() {
  get_store_called_ = true;
  return store_.get();
}

void TestCatalogStore::UpdateStore(
    scoped_ptr<base::ListValue> store) {}

scoped_ptr<base::DictionaryValue> BuildPermissiveSerializedAppInfo(
    const std::string& name,
    const std::string& display_name) {
  scoped_ptr<base::DictionaryValue> app(new base::DictionaryValue);
  app->SetString(Store::kNameKey, name);
  app->SetString(Store::kDisplayNameKey, display_name);

  scoped_ptr<base::DictionaryValue> capabilities(new base::DictionaryValue);
  scoped_ptr<base::ListValue> interfaces(new base::ListValue);
  interfaces->AppendString("*");
  capabilities->Set("*", std::move(interfaces));

  app->Set(Store::kCapabilitiesKey, std::move(capabilities));

  return app;
}

}  // namespace shell
}  // namespace mojo
