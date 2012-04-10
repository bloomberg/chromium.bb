// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/gdata/gdata_files.h"

#include <string>
#include <utility>
#include <vector>
#include "testing/gtest/include/gtest/gtest.h"

namespace gdata {

TEST(GDataRootDirectoryTest, RemoveTemporaryFilesFromCacheMap) {
  scoped_ptr<GDataRootDirectory> root(new GDataRootDirectory(NULL));
  GDataRootDirectory::CacheMap cache_map;
  cache_map.insert(std::make_pair(
      "<resource_id_1>",
      new GDataRootDirectory::CacheEntry(
          "<md5>",
          GDataRootDirectory::CACHE_TYPE_TMP,
          GDataFile::CACHE_STATE_NONE)));
  cache_map.insert(std::make_pair(
      "<resource_id_2>",
      new GDataRootDirectory::CacheEntry(
          "<md5>",
          GDataRootDirectory::CACHE_TYPE_PINNED,
          GDataFile::CACHE_STATE_NONE)));
  cache_map.insert(std::make_pair(
      "<resource_id_3>",
      new GDataRootDirectory::CacheEntry(
          "<md5>",
          GDataRootDirectory::CACHE_TYPE_OUTGOING,
          GDataFile::CACHE_STATE_NONE)));
  cache_map.insert(std::make_pair(
      "<resource_id_4>",
      new GDataRootDirectory::CacheEntry(
          "<md5>",
          GDataRootDirectory::CACHE_TYPE_TMP,
          GDataFile::CACHE_STATE_NONE)));
  root->SetCacheMap(cache_map);
  root->RemoveTemporaryFilesFromCacheMap();
  // resource 1 and 4 should be gone, as these are CACHE_TYPE_TMP.
  ASSERT_TRUE(root->GetCacheEntry("<resource_id_1>", "") == NULL);
  ASSERT_TRUE(root->GetCacheEntry("<resource_id_2>", "") != NULL);
  ASSERT_TRUE(root->GetCacheEntry("<resource_id_3>", "") != NULL);
  ASSERT_TRUE(root->GetCacheEntry("<resource_id_4>", "") == NULL);

}

}  // namespace gdata
