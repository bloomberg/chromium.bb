// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/extensions/activity_log/ad_network_database.h"
#include "chrome/browser/extensions/activity_log/hashed_ad_network_database.h"
#include "crypto/secure_hash.h"
#include "crypto/sha2.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace extensions {

namespace {

// A list of fake ad networks.
const char* kAdNetworkHosts[] = {
  "alpha.adnetwork",
  "bravo.adnetwork",
  "charlie.delta.adnetwork"
};

// The number of ad networks for these tests.
const size_t kNumAdNetworkHosts = arraysize(kAdNetworkHosts);

// Each hash of an ad network is stored in an int64.
const size_t kAdNetworkHostHashSize = sizeof(int64);

// The total size for storing all ad network host hashes.
const size_t kAdNetworkHostHashesTotalSize =
    kAdNetworkHostHashSize * kNumAdNetworkHosts;

// The size of the checksum we use in the data resource.
const size_t kChecksumSize = 32u;

// The total size of the data resource, including the checksum and all host
// hashes.
const size_t kDataResourceSize = kChecksumSize + kAdNetworkHostHashesTotalSize;

}  // namespace

class HashedAdNetworkDatabaseUnitTest : public testing::Test {
 protected:
  virtual void SetUp() OVERRIDE;
  virtual void TearDown() OVERRIDE;

 private:
  // Generate a piece of memory with a hash structure identical to the real one,
  // but with only mock data.
  void GenerateMockMemory();

  // The raw bits of the mocked-up data resource.
  char raw_data_[kDataResourceSize];

  // The RefCountedStaticMemory wrapper around |raw_data_|.
  scoped_refptr<base::RefCountedStaticMemory> memory_;
};

void HashedAdNetworkDatabaseUnitTest::SetUp() {
  GenerateMockMemory();
  AdNetworkDatabase::SetForTesting(
      scoped_ptr<AdNetworkDatabase>(new HashedAdNetworkDatabase(memory_)));
}

void HashedAdNetworkDatabaseUnitTest::TearDown() {
  // Reset the database.
  AdNetworkDatabase::SetForTesting(scoped_ptr<AdNetworkDatabase>());
}

void HashedAdNetworkDatabaseUnitTest::GenerateMockMemory() {
  int64 host_hashes[kNumAdNetworkHosts];

  for (size_t i = 0; i < kNumAdNetworkHosts; ++i) {
    int64 hash = 0;
    crypto::SHA256HashString(kAdNetworkHosts[i], &hash, sizeof(hash));
    host_hashes[i] = hash;
  }

  // Create the Checksum.
  scoped_ptr<crypto::SecureHash> hash(
      crypto::SecureHash::Create(crypto::SecureHash::SHA256));
  hash->Update(host_hashes, kNumAdNetworkHosts * kAdNetworkHostHashSize);

  char checksum[kChecksumSize];
  hash->Finish(checksum, kChecksumSize);

  // Copy the checksum to our data.
  memcpy(raw_data_, &checksum, kChecksumSize);

  // Copy the hashes.
  memcpy(raw_data_ + kChecksumSize, host_hashes, kAdNetworkHostHashesTotalSize);

  memory_ = new base::RefCountedStaticMemory(raw_data_, kDataResourceSize);
};

// Test that the logic for the Ad Network Database works. That is, the hashing
// scheme works, correctly reports when URLs are present in the database,
// treats hosts and sumdomains correctly, etc.
TEST_F(HashedAdNetworkDatabaseUnitTest, HashedAdNetworkDatabaseTest) {
  const AdNetworkDatabase* database = AdNetworkDatabase::Get();
  ASSERT_TRUE(database);

  // First, just check the basic urls in the list of ad networks.
  EXPECT_TRUE(database->IsAdNetwork(GURL("http://alpha.adnetwork")));
  EXPECT_TRUE(database->IsAdNetwork(GURL("http://bravo.adnetwork")));
  EXPECT_TRUE(database->IsAdNetwork(GURL("http://charlie.delta.adnetwork")));

  // Next, try adding some paths. These should still register.
  EXPECT_TRUE(database->IsAdNetwork(GURL("http://alpha.adnetwork/foo")));
  EXPECT_TRUE(database->IsAdNetwork(GURL("http://bravo.adnetwork/foo/bar")));
  EXPECT_TRUE(
      database->IsAdNetwork(GURL("http://charlie.delta.adnetwork/foo.html")));

  // Then, try subdomains. These should not register, as they are treated as
  // different hosts.
  EXPECT_FALSE(database->IsAdNetwork(GURL("http://foo.alpha.adnetwork")));
  EXPECT_FALSE(database->IsAdNetwork(GURL("http://foo.bar.bravo.adnetwork")));
  EXPECT_FALSE(
      database->IsAdNetwork(GURL("http://foo.charlie.delta.adnetwork")));

  // Check to make sure that removing a subdomain (from charlie.delta.adnetwork)
  // is considered different, and doesn't register.
  EXPECT_FALSE(database->IsAdNetwork(GURL("http://delta.adnetwork")));

  // And, of course, try some random sites and make sure we don't miscategorize.
  EXPECT_FALSE(database->IsAdNetwork(GURL("http://www.google.com")));
  EXPECT_FALSE(database->IsAdNetwork(GURL("http://drive.google.com")));
  EXPECT_FALSE(database->IsAdNetwork(GURL("https://www.google.com")));
  EXPECT_FALSE(
      database->IsAdNetwork(GURL("file:///usr/someone/files/file.html")));
  EXPECT_FALSE(database->IsAdNetwork(
      GURL("chrome-extension://aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa")));
}

// Test that the HashAdNetworkDatabse test works with the real file. For
// security and privacy purposes, we cannot verify that real URLs are
// recognized. However, we can at least verify that the file is recognized and
// parsed.
TEST(HashedAdNetworkDatabaseWithRealFileUnitTest,
     HashedAdNetworkDatabaseRealFileTest) {
  // This constructs the database, and, since we didn't mock up any memory, it
  // uses the real file.
  const AdNetworkDatabase* database = AdNetworkDatabase::Get();
  ASSERT_TRUE(database);
  const HashedAdNetworkDatabase* hashed_database =
      static_cast<const HashedAdNetworkDatabase*>(database);
  EXPECT_TRUE(hashed_database->is_valid());

  // We can also safely assume that a made-up url is not in the database.
  EXPECT_FALSE(database->IsAdNetwork(
      GURL("http://www.this-is-definitely-not-a-real-site.orarealtld")));
}

}  // namespace extensions
