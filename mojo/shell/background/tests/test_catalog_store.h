// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SHELL_BACKGROUND_TESTS_TEST_CATALOG_STORE_H_
#define MOJO_SHELL_BACKGROUND_TESTS_TEST_CATALOG_STORE_H_

#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "mojo/services/catalog/store.h"

namespace mojo {
namespace shell {

// ApplicationCatalogStore implementation that takes the ListValue to return
// as store.
class TestCatalogStore : public catalog::Store {
 public:
  explicit TestCatalogStore(scoped_ptr<base::ListValue> store);
  ~TestCatalogStore() override;

  bool get_store_called() const { return get_store_called_; }

  // ApplicationCatalogStore:
  const base::ListValue* GetStore() override;
  void UpdateStore(scoped_ptr<base::ListValue> store) override;

 private:
  bool get_store_called_ = false;
  scoped_ptr<base::ListValue> store_;

  DISALLOW_COPY_AND_ASSIGN(TestCatalogStore);
};

// Returns a dictionary for an app with the specified name, display name and a
// permissive filter.
scoped_ptr<base::DictionaryValue> BuildPermissiveSerializedAppInfo(
    const std::string& name,
    const std::string& display_name);

}  // namespace shell
}  // namespace mojo

#endif  // MOJO_SHELL_BACKGROUND_TESTS_TEST_CATALOG_STORE_H_
