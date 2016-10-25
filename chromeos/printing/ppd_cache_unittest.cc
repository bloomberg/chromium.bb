// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/hash.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chromeos/printing/ppd_cache.h"
#include "net/url_request/test_url_request_interceptor.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace printing {
namespace {

const char kTestManufacturer[] = "FooPrinters, Inc.";
const char kTestModel[] = "Laser BarMatic 1000";
const char kTestUserUrl[] = "/some/path/to/some/ppd/file";
const char kTestPpdContents[] = "This is the ppd for the first printer";
// Output of 'gzip -9' on a file with contents 'ppd contents'
const char kTestGZippedPpdContents[] = {
    0x1f, 0x8b, 0x08, 0x08, 0xe4, 0x2e, 0x00, 0x58, 0x02, 0x03, 0x70,
    0x70, 0x64, 0x2e, 0x74, 0x78, 0x74, 0x00, 0x2b, 0x28, 0x48, 0x51,
    0x48, 0xce, 0xcf, 0x2b, 0x49, 0xcd, 0x2b, 0x29, 0xe6, 0x02, 0x00,
    0x2b, 0x51, 0x91, 0x24, 0x0d, 0x00, 0x00, 0x00};

// Generate and return a PPDReference that has the fields set from the kTest
// constants above.
Printer::PpdReference TestReference() {
  Printer::PpdReference ret;
  ret.effective_manufacturer = kTestManufacturer;
  ret.effective_model = kTestModel;
  ret.user_supplied_ppd_url = kTestUserUrl;
  return ret;
}

// This fixture just points the cache at a temporary directory for the life of
// the test.
class PpdCacheTest : public ::testing::Test {
 public:
  void SetUp() override {
    ASSERT_TRUE(ppd_cache_temp_dir_.CreateUniqueTempDir());
  }

  // Make and return a cache for the test that uses a temporary directory
  // which is cleaned up at the end of the test.
  std::unique_ptr<PpdCache> CreateTestCache() {
    return PpdCache::Create(ppd_cache_temp_dir_.GetPath());
  }

 protected:
  // Overrider for DIR_CHROMEOS_PPD_CACHE that points it at a temporary
  // directory for the life of the test.
  base::ScopedTempDir ppd_cache_temp_dir_;
};

// Check that path has a value, and the contents of the referenced file are
// |expected_contents|.
void CheckFileContentsAre(base::Optional<base::FilePath> result,
                          const std::string& expected_contents) {
  // Make sure we have a value.
  ASSERT_TRUE(result);
  // Make sure we get the ppd back that we stored.
  std::string contents;
  ASSERT_TRUE(base::ReadFileToString(result.value(), &contents));
  EXPECT_EQ(expected_contents, contents);
}

// Test that we miss on an empty cache.
TEST_F(PpdCacheTest, SimpleMiss) {
  auto cache = CreateTestCache();
  EXPECT_FALSE(cache->Find(TestReference()));
}

// Test that when we store stuff, we get it back.
TEST_F(PpdCacheTest, MissThenHit) {
  auto cache = CreateTestCache();
  auto ref = TestReference();
  EXPECT_FALSE(cache->Find(ref));

  // Store should give us a reference to the file.
  CheckFileContentsAre(cache->Store(ref, kTestPpdContents), kTestPpdContents);

  // We should also get it back in response to a Find.
  CheckFileContentsAre(cache->Find(ref), kTestPpdContents);
}

// Test that mutating any field in the reference causes us to miss in the cache
// when we change the highest priority resolve criteria.
TEST_F(PpdCacheTest, FieldChangeMeansCacheMiss) {
  auto cache = CreateTestCache();
  auto ref = TestReference();
  CheckFileContentsAre(cache->Store(ref, kTestPpdContents), kTestPpdContents);

  // We have a user url, so should still cache hit on manufacturer change.
  ref.effective_manufacturer = "Something else";
  EXPECT_TRUE(cache->Find(ref));
  ref = TestReference();

  // We have a user url, so should still cache hit on model change.
  ref.effective_model = "Something else";
  EXPECT_TRUE(cache->Find(ref));
  ref = TestReference();

  // But if we change th user url, that's a cache miss.
  ref.user_supplied_ppd_url = "Something else";
  EXPECT_FALSE(cache->Find(ref));
  ref = TestReference();
  // Should still find the initial Store when ref *is* identical.
  CheckFileContentsAre(cache->Find(ref), kTestPpdContents);

  // Now store the reference with no test url.
  ref.user_supplied_ppd_url.clear();
  CheckFileContentsAre(cache->Store(ref, kTestPpdContents), kTestPpdContents);

  // Now changing the model or manufacturer should cause a cache miss.
  ref.effective_manufacturer = "Something else";
  EXPECT_FALSE(cache->Find(ref));
  ref = TestReference();
  ref.user_supplied_ppd_url.clear();
  ref.effective_model = "Something else";
  EXPECT_FALSE(cache->Find(ref));
}

// Test that gzipped contents get stored as .ppd.gz, and non-gzipped
// contents get stored as .ppd
TEST_F(PpdCacheTest, StoredExtensions) {
  auto cache = CreateTestCache();
  auto ref = TestReference();

  // Not compressed, so should store as .ppd
  auto result = cache->Store(ref, kTestPpdContents);
  EXPECT_EQ(".ppd", cache->Store(ref, kTestPpdContents).value().Extension());

  // Compressed, so should identify this and store as .ppd.gz
  std::string gzipped_contents(kTestGZippedPpdContents,
                               sizeof(kTestGZippedPpdContents));
  EXPECT_EQ(".ppd.gz", cache->Store(ref, gzipped_contents).value().Extension());
}

}  // namespace
}  // namespace printing
}  // namespace chromeos
