// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/prefix_set.h"

#include <algorithm>

#include "base/file_util.h"
#include "base/logging.h"
#include "base/md5.h"
#include "base/rand_util.h"
#include "base/scoped_ptr.h"
#include "base/scoped_temp_dir.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace {

class PrefixSetTest : public PlatformTest {
 protected:
  // Constants for the v1 format.
  static const size_t kMagicOffset = 0 * sizeof(uint32);
  static const size_t kVersionOffset = 1 * sizeof(uint32);
  static const size_t kIndexSizeOffset = 2 * sizeof(uint32);
  static const size_t kDeltasSizeOffset = 3 * sizeof(uint32);
  static const size_t kPayloadOffset = 4 * sizeof(uint32);

  // Generate a set of random prefixes to share between tests.  For
  // most tests this generation was a large fraction of the test time.
  static void SetUpTestCase() {
    for (size_t i = 0; i < 50000; ++i) {
      const SBPrefix prefix = static_cast<SBPrefix>(base::RandUint64());
      shared_prefixes_.push_back(prefix);
    }

    // Sort for use with PrefixSet constructor.
    std::sort(shared_prefixes_.begin(), shared_prefixes_.end());
  }

  // Check that all elements of |prefixes| are in |prefix_set|, and
  // that nearby elements are not (for lack of a more sensible set of
  // items to check for absence).
  static void CheckPrefixes(safe_browsing::PrefixSet* prefix_set,
                            const std::vector<SBPrefix> &prefixes) {
    // The set can generate the prefixes it believes it has, so that's
    // a good starting point.
    std::set<SBPrefix> check(prefixes.begin(), prefixes.end());
    std::vector<SBPrefix> prefixes_copy;
    prefix_set->GetPrefixes(&prefixes_copy);
    EXPECT_EQ(prefixes_copy.size(), check.size());
    EXPECT_TRUE(std::equal(check.begin(), check.end(), prefixes_copy.begin()));

    for (size_t i = 0; i < prefixes.size(); ++i) {
      EXPECT_TRUE(prefix_set->Exists(prefixes[i]));

      const SBPrefix left_sibling = prefixes[i] - 1;
      if (check.count(left_sibling) == 0)
        EXPECT_FALSE(prefix_set->Exists(left_sibling));

      const SBPrefix right_sibling = prefixes[i] + 1;
      if (check.count(right_sibling) == 0)
        EXPECT_FALSE(prefix_set->Exists(right_sibling));
    }
  }

  // Generate a |PrefixSet| file from |shared_prefixes_|, store it in
  // a temporary file, and return the filename in |filenamep|.
  // Returns |true| on success.
  bool GetPrefixSetFile(FilePath* filenamep) {
    if (!temp_dir_.IsValid() && !temp_dir_.CreateUniqueTempDir())
      return false;

    FilePath filename = temp_dir_.path().AppendASCII("PrefixSetTest");

    safe_browsing::PrefixSet prefix_set(shared_prefixes_);
    if (!prefix_set.WriteFile(filename))
      return false;

    *filenamep = filename;
    return true;
  }

  // Helper function to read the int32 value at |offset|, increment it
  // by |inc|, and write it back in place.  |fp| should be opened in
  // r+ mode.
  static void IncrementIntAt(FILE* fp, long offset, int inc) {
    int32 value = 0;

    ASSERT_NE(-1, fseek(fp, offset, SEEK_SET));
    ASSERT_EQ(1U, fread(&value, sizeof(value), 1, fp));

    value += inc;

    ASSERT_NE(-1, fseek(fp, offset, SEEK_SET));
    ASSERT_EQ(1U, fwrite(&value, sizeof(value), 1, fp));
  }

  // Helper function to re-generated |fp|'s checksum to be correct for
  // the file's contents.  |fp| should be opened in r+ mode.
  static void CleanChecksum(FILE* fp) {
    MD5Context context;
    MD5Init(&context);

    ASSERT_NE(-1, fseek(fp, 0, SEEK_END));
    long file_size = ftell(fp);

    size_t payload_size = static_cast<size_t>(file_size) - sizeof(MD5Digest);
    size_t digested_size = 0;
    ASSERT_NE(-1, fseek(fp, 0, SEEK_SET));
    while (digested_size < payload_size) {
      char buf[1024];
      size_t nitems = std::min(payload_size - digested_size, sizeof(buf));
      ASSERT_EQ(nitems, fread(buf, 1, nitems, fp));
      MD5Update(&context, &buf, nitems);
      digested_size += nitems;
    }
    ASSERT_EQ(digested_size, payload_size);
    ASSERT_EQ(static_cast<long>(digested_size), ftell(fp));

    MD5Digest new_digest;
    MD5Final(&new_digest, &context);
    ASSERT_NE(-1, fseek(fp, digested_size, SEEK_SET));
    ASSERT_EQ(1U, fwrite(&new_digest, sizeof(new_digest), 1, fp));
    ASSERT_EQ(file_size, ftell(fp));
  }

  // Open |filename| and increment the int32 at |offset| by |inc|.
  // Then re-generate the checksum to account for the new contents.
  void ModifyAndCleanChecksum(const FilePath& filename, long offset, int inc) {
    int64 size_64;
    ASSERT_TRUE(file_util::GetFileSize(filename, &size_64));

    file_util::ScopedFILE file(file_util::OpenFile(filename, "r+b"));
    IncrementIntAt(file.get(), offset, inc);
    CleanChecksum(file.get());
    file.reset();

    int64 new_size_64;
    ASSERT_TRUE(file_util::GetFileSize(filename, &new_size_64));
    ASSERT_EQ(new_size_64, size_64);
  }

  // Tests should not modify this shared resource.
  static std::vector<SBPrefix> shared_prefixes_;

  ScopedTempDir temp_dir_;
};

std::vector<SBPrefix> PrefixSetTest::shared_prefixes_;

// Test that a small sparse random input works.
TEST_F(PrefixSetTest, Baseline) {
  safe_browsing::PrefixSet prefix_set(shared_prefixes_);
  CheckPrefixes(&prefix_set, shared_prefixes_);
}

// Test that the empty set doesn't appear to have anything in it.
TEST_F(PrefixSetTest, Empty) {
  const std::vector<SBPrefix> empty;
  safe_browsing::PrefixSet prefix_set(empty);
  for (size_t i = 0; i < shared_prefixes_.size(); ++i) {
    EXPECT_FALSE(prefix_set.Exists(shared_prefixes_[i]));
  }
}

// Use artificial inputs to test various edge cases in Exists().
// Items before the lowest item aren't present.  Items after the
// largest item aren't present.  Create a sequence of items with
// deltas above and below 2^16, and make sure they're all present.
// Create a very long sequence with deltas below 2^16 to test crossing
// |kMaxRun|.
TEST_F(PrefixSetTest, EdgeCases) {
  std::vector<SBPrefix> prefixes;

  const SBPrefix kVeryPositive = 1000 * 1000 * 1000;
  const SBPrefix kVeryNegative = -kVeryPositive;

  // Put in a very negative prefix.
  SBPrefix prefix = kVeryNegative;
  prefixes.push_back(prefix);

  // Add a sequence with very large deltas.
  unsigned delta = 100 * 1000 * 1000;
  for (int i = 0; i < 10; ++i) {
    prefix += delta;
    prefixes.push_back(prefix);
  }

  // Add a sequence with deltas that start out smaller than the
  // maximum delta, and end up larger.  Also include some duplicates.
  delta = 256 * 256 - 100;
  for (int i = 0; i < 200; ++i) {
    prefix += delta;
    prefixes.push_back(prefix);
    prefixes.push_back(prefix);
    delta++;
  }

  // Add a long sequence with deltas smaller than the maximum delta,
  // so a new index item will be injected.
  delta = 256 * 256 - 1;
  prefix = kVeryPositive - delta * 1000;
  prefixes.push_back(prefix);
  for (int i = 0; i < 1000; ++i) {
    prefix += delta;
    prefixes.push_back(prefix);
    delta--;
  }

  std::sort(prefixes.begin(), prefixes.end());
  safe_browsing::PrefixSet prefix_set(prefixes);

  // Check that |GetPrefixes()| returns the same set of prefixes as
  // was passed in.
  std::vector<SBPrefix> prefixes_copy;
  prefix_set.GetPrefixes(&prefixes_copy);
  prefixes.erase(std::unique(prefixes.begin(), prefixes.end()), prefixes.end());
  EXPECT_EQ(prefixes_copy.size(), prefixes.size());
  EXPECT_TRUE(std::equal(prefixes.begin(), prefixes.end(),
                         prefixes_copy.begin()));

  // Items before and after the set are not present, and don't crash.
  EXPECT_FALSE(prefix_set.Exists(kVeryNegative - 100));
  EXPECT_FALSE(prefix_set.Exists(kVeryPositive + 100));

  // Check that the set correctly flags all of the inputs, and also
  // check items just above and below the inputs to make sure they
  // aren't present.
  for (size_t i = 0; i < prefixes.size(); ++i) {
    EXPECT_TRUE(prefix_set.Exists(prefixes[i]));

    EXPECT_FALSE(prefix_set.Exists(prefixes[i] - 1));
    EXPECT_FALSE(prefix_set.Exists(prefixes[i] + 1));
  }
}

// Similar to Baseline test, but write the set out to a file and read
// it back in before testing.
TEST_F(PrefixSetTest, ReadWrite) {
  FilePath filename;
  ASSERT_TRUE(GetPrefixSetFile(&filename));

  scoped_ptr<safe_browsing::PrefixSet>
      prefix_set(safe_browsing::PrefixSet::LoadFile(filename));
  ASSERT_TRUE(prefix_set.get());

  CheckPrefixes(prefix_set.get(), shared_prefixes_);
}

// Check that |CleanChecksum()| makes an acceptable checksum.
TEST_F(PrefixSetTest, CorruptionHelpers) {
  FilePath filename;
  ASSERT_TRUE(GetPrefixSetFile(&filename));

  // This will modify data in |index_|, which will fail the digest check.
  file_util::ScopedFILE file(file_util::OpenFile(filename, "r+b"));
  IncrementIntAt(file.get(), kPayloadOffset, 1);
  file.reset();
  scoped_ptr<safe_browsing::PrefixSet>
      prefix_set(safe_browsing::PrefixSet::LoadFile(filename));
  ASSERT_FALSE(prefix_set.get());

  // Fix up the checksum and it will read successfully (though the
  // data will be wrong).
  file.reset(file_util::OpenFile(filename, "r+b"));
  CleanChecksum(file.get());
  file.reset();
  prefix_set.reset(safe_browsing::PrefixSet::LoadFile(filename));
  ASSERT_TRUE(prefix_set.get());
}

// Bad |index_| size is caught by the sanity check.
TEST_F(PrefixSetTest, CorruptionMagic) {
  FilePath filename;
  ASSERT_TRUE(GetPrefixSetFile(&filename));

  ASSERT_NO_FATAL_FAILURE(
      ModifyAndCleanChecksum(filename, kMagicOffset, 1));
  scoped_ptr<safe_browsing::PrefixSet>
      prefix_set(safe_browsing::PrefixSet::LoadFile(filename));
  ASSERT_FALSE(prefix_set.get());
}

// Bad |index_| size is caught by the sanity check.
TEST_F(PrefixSetTest, CorruptionVersion) {
  FilePath filename;
  ASSERT_TRUE(GetPrefixSetFile(&filename));

  ASSERT_NO_FATAL_FAILURE(
      ModifyAndCleanChecksum(filename, kVersionOffset, 1));
  scoped_ptr<safe_browsing::PrefixSet>
      prefix_set(safe_browsing::PrefixSet::LoadFile(filename));
  ASSERT_FALSE(prefix_set.get());
}

// Bad |index_| size is caught by the sanity check.
TEST_F(PrefixSetTest, CorruptionIndexSize) {
  FilePath filename;
  ASSERT_TRUE(GetPrefixSetFile(&filename));

  ASSERT_NO_FATAL_FAILURE(
      ModifyAndCleanChecksum(filename, kIndexSizeOffset, 1));
  scoped_ptr<safe_browsing::PrefixSet>
      prefix_set(safe_browsing::PrefixSet::LoadFile(filename));
  ASSERT_FALSE(prefix_set.get());
}

// Bad |deltas_| size is caught by the sanity check.
TEST_F(PrefixSetTest, CorruptionDeltasSize) {
  FilePath filename;
  ASSERT_TRUE(GetPrefixSetFile(&filename));

  ASSERT_NO_FATAL_FAILURE(
      ModifyAndCleanChecksum(filename, kDeltasSizeOffset, 1));
  scoped_ptr<safe_browsing::PrefixSet>
      prefix_set(safe_browsing::PrefixSet::LoadFile(filename));
  ASSERT_FALSE(prefix_set.get());
}

// Test that the digest catches corruption in the middle of the file
// (in the payload between the header and the digest).
TEST_F(PrefixSetTest, CorruptionPayload) {
  FilePath filename;
  ASSERT_TRUE(GetPrefixSetFile(&filename));

  file_util::ScopedFILE file(file_util::OpenFile(filename, "r+b"));
  ASSERT_NO_FATAL_FAILURE(IncrementIntAt(file.get(), 666, 1));
  file.reset();
  scoped_ptr<safe_browsing::PrefixSet>
      prefix_set(safe_browsing::PrefixSet::LoadFile(filename));
  ASSERT_FALSE(prefix_set.get());
}

// Test corruption in the digest itself.
TEST_F(PrefixSetTest, CorruptionDigest) {
  FilePath filename;
  ASSERT_TRUE(GetPrefixSetFile(&filename));

  int64 size_64;
  ASSERT_TRUE(file_util::GetFileSize(filename, &size_64));
  file_util::ScopedFILE file(file_util::OpenFile(filename, "r+b"));
  long digest_offset = static_cast<long>(size_64 - sizeof(MD5Digest));
  ASSERT_NO_FATAL_FAILURE(IncrementIntAt(file.get(), digest_offset, 1));
  file.reset();
  scoped_ptr<safe_browsing::PrefixSet>
      prefix_set(safe_browsing::PrefixSet::LoadFile(filename));
  ASSERT_FALSE(prefix_set.get());
}

// Test excess data after the digest (fails the size test).
TEST_F(PrefixSetTest, CorruptionExcess) {
  FilePath filename;
  ASSERT_TRUE(GetPrefixSetFile(&filename));

  // Add some junk to the trunk.
  file_util::ScopedFILE file(file_util::OpenFile(filename, "ab"));
  const char buf[] = "im in ur base, killing ur d00dz.";
  ASSERT_EQ(strlen(buf), fwrite(buf, 1, strlen(buf), file.get()));
  file.reset();
  scoped_ptr<safe_browsing::PrefixSet>
      prefix_set(safe_browsing::PrefixSet::LoadFile(filename));
  ASSERT_FALSE(prefix_set.get());
}

}  // namespace
