// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/value_store/value_store_unittest.h"

#include "chrome/browser/value_store/testing_value_store.h"

namespace extensions {

namespace {

ValueStore* Param(const base::FilePath& file_path) {
  return new TestingValueStore();
}

}  // namespace

INSTANTIATE_TEST_CASE_P(
    TestingValueStore,
    ValueStoreTest,
    testing::Values(&Param));

}  // namespace extensions
