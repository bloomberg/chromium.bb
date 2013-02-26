// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/resource_cache.h"

#include "base/basictypes.h"
#include "base/files/scoped_temp_dir.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

namespace {

const char kKey[] = "key";
const char kAnotherKey[] = "anotherkey";
const char kSubA[] = "a";
const char kSubB[] = "bb";
const char kSubC[] = "ccc";
const char kSubD[] = "dddd";
const char kSubE[] = "eeeee";

const char kData0[] = "{ \"key\": \"value\" }";
const char kData1[] = "{}";

}  // namespace

TEST(ResourceCacheTest, StoreAndLoad) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  ResourceCache cache(temp_dir.path().AppendASCII("db"));

  // No data initially.
  std::string data;
  EXPECT_FALSE(cache.Load(kKey, kSubA, &data));

  // Store some data and load it.
  EXPECT_TRUE(cache.Store(kKey, kSubA, kData0));
  EXPECT_TRUE(cache.Load(kKey, kSubA, &data));
  EXPECT_EQ(kData0, data);

  // Store more data in another subkey.
  EXPECT_TRUE(cache.Store(kKey, kSubB, kData1));

  // Write subkeys to another key.
  EXPECT_TRUE(cache.Store(kAnotherKey, kSubA, kData0));
  EXPECT_TRUE(cache.Store(kAnotherKey, kSubB, kData1));

  // Enumerate all the keys.
  std::map<std::string, std::string> contents;
  cache.LoadAllSubkeys(kKey, &contents);
  EXPECT_EQ(2u, contents.size());
  EXPECT_EQ(kData0, contents[kSubA]);
  EXPECT_EQ(kData1, contents[kSubB]);

  // Store more keys.
  EXPECT_TRUE(cache.Store(kKey, kSubC, kData1));
  EXPECT_TRUE(cache.Store(kKey, kSubD, kData1));
  EXPECT_TRUE(cache.Store(kKey, kSubE, kData1));

  // Now purge some of them.
  std::set<std::string> keep;
  keep.insert(kSubB);
  keep.insert(kSubD);
  cache.PurgeOtherSubkeys(kKey, keep);

  // Enumerate all the remaining keys.
  cache.LoadAllSubkeys(kKey, &contents);
  EXPECT_EQ(2u, contents.size());
  EXPECT_EQ(kData1, contents[kSubB]);
  EXPECT_EQ(kData1, contents[kSubD]);

  // Delete keys directly.
  cache.Delete(kKey, kSubB);
  cache.Delete(kKey, kSubD);
  cache.LoadAllSubkeys(kKey, &contents);
  EXPECT_EQ(0u, contents.size());

  // The other key was not affected.
  cache.LoadAllSubkeys(kAnotherKey, &contents);
  EXPECT_EQ(2u, contents.size());
  EXPECT_EQ(kData0, contents[kSubA]);
  EXPECT_EQ(kData1, contents[kSubB]);
}

}  // namespace policy
