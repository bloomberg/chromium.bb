// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DISK_CACHE_DISK_CACHE_FLASH_TEST_BASE_H_
#define NET_DISK_CACHE_DISK_CACHE_FLASH_TEST_BASE_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/scoped_ptr.h"
#include "net/disk_cache/flash/format.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const int32 kNumTestSegments = 10;
const int32 kStorageSize = kNumTestSegments * disk_cache::kFlashSegmentSize;

}  // namespace

namespace disk_cache {

class LogStore;

}  // namespace disk_cache

class FlashCacheTest : public testing::Test {
 protected:
  FlashCacheTest();
  virtual ~FlashCacheTest();

  virtual void SetUp() OVERRIDE;
  virtual void TearDown() OVERRIDE;

  base::ScopedTempDir temp_dir_;
  base::FilePath path_;

 private:
  DISALLOW_COPY_AND_ASSIGN(FlashCacheTest);
};

#endif  // NET_DISK_CACHE_DISK_CACHE_FLASH_TEST_BASE_H_
