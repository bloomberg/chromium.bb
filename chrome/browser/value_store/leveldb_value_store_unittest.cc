// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/value_store/value_store_unittest.h"

#include "base/memory/ref_counted.h"
#include "chrome/browser/value_store/leveldb_value_store.h"

namespace {

ValueStore* Param(const base::FilePath& file_path) {
  return new LeveldbValueStore(file_path);
}

}  // namespace

INSTANTIATE_TEST_CASE_P(
    LeveldbValueStore,
    ValueStoreTest,
    testing::Values(&Param));
