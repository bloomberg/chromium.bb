// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/flash/flash_cache_test_base.h"

#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/time.h"
#include "net/disk_cache/flash/format.h"
#include "net/disk_cache/flash/log_store.h"
#include "net/disk_cache/flash/storage.h"

FlashCacheTest::FlashCacheTest() {
  int seed = static_cast<int>(base::Time::Now().ToInternalValue());
  srand(seed);
}

FlashCacheTest::~FlashCacheTest() {
}

void FlashCacheTest::SetUp() {
  const base::FilePath::StringType kCachePath = FILE_PATH_LITERAL("cache");
  ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  path_ = temp_dir_.path().Append(kCachePath);
}

void FlashCacheTest::TearDown() {
}
