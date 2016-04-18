// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <memory>
#include <vector>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "blimp/common/blob_cache/blob_cache.h"
#include "blimp/common/blob_cache/in_memory_blob_cache.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blimp {
namespace {

const char kFoo[] = "foo";
const char kBar[] = "bar";
const char kDeadbeef[] = "\xde\xad\xbe\xef";
const char kForbiddenCode[] = "\x4b\x1d\xc0\xd3";

BlobData CreateBlobData(const std::string& data) {
  return new base::RefCountedData<const std::string>(data);
}

class InMemoryBlobCacheTest : public testing::Test {
 public:
  InMemoryBlobCacheTest() {}
  ~InMemoryBlobCacheTest() override {}

 protected:
  InMemoryBlobCache cache_;
};

TEST_F(InMemoryBlobCacheTest, SimplePutContainsAndGetOperations) {
  EXPECT_FALSE(cache_.Contains(kFoo));
  EXPECT_EQ(nullptr, cache_.Get(kFoo));

  BlobData blob_data = CreateBlobData(kDeadbeef);
  cache_.Put(kFoo, blob_data);

  EXPECT_TRUE(cache_.Contains(kFoo));
  EXPECT_FALSE(cache_.Contains(kBar));

  BlobData out = cache_.Get(kFoo);

  EXPECT_EQ(blob_data, out);
}

TEST_F(InMemoryBlobCacheTest, TestDuplicatePut) {
  BlobData first = CreateBlobData(kDeadbeef);
  BlobData duplicate = CreateBlobData(kForbiddenCode);
  cache_.Put(kFoo, first);

  BlobData out1 = cache_.Get(kFoo);
  EXPECT_EQ(first, out1);

  // The second put should be ignored and retrieving kFoo should still retrieve
  // the first item.
  cache_.Put(kFoo, duplicate);
  BlobData out2 = cache_.Get(kFoo);
  EXPECT_EQ(first, out2);
}

}  // namespace
}  // namespace blimp
