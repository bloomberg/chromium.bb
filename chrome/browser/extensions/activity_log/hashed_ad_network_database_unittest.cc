// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/extensions/activity_log/hashed_ad_network_database.h"
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

}  // namespace

class HashedAdNetworkDatabaseUnitTest : public testing::Test {
 protected:
  virtual void SetUp() OVERRIDE;

  AdNetworkDatabase* database() { return database_.get(); }

 private:
  void GenerateHashes();

  // The fake hashes for the ad networks.
  const char* ad_networks_[kNumAdNetworkHosts];

  // The backing data for the ad networks. Since everything expects a const
  // char, we need this little hack in order to generate the data.
  std::vector<std::string> ad_networks_data_;

  // The database used in testing.
  scoped_ptr<HashedAdNetworkDatabase> database_;
};

void HashedAdNetworkDatabaseUnitTest::SetUp() {
  GenerateHashes();
  database_.reset(new HashedAdNetworkDatabase());
  database_->set_entries_for_testing(ad_networks_, kNumAdNetworkHosts);
}

void HashedAdNetworkDatabaseUnitTest::GenerateHashes() {
  for (size_t i = 0; i < kNumAdNetworkHosts; ++i) {
    char hash[8u];
    crypto::SHA256HashString(kAdNetworkHosts[i], hash, 8u);
    ad_networks_data_.push_back(base::HexEncode(hash, 8u));
  }

  // HashedAdNetworkDatabase assumes the list is sorted.
  std::sort(ad_networks_data_.begin(), ad_networks_data_.end());
  for (size_t i = 0u; i < ad_networks_data_.size(); ++i)
    ad_networks_[i] = ad_networks_data_[i].c_str();
}

// Test that the logic for the Ad Network Database works. That is, the hashing
// scheme works, correctly reports when URLs are present in the database,
// treats hosts and sumdomains correctly, etc.
TEST_F(HashedAdNetworkDatabaseUnitTest, HashedAdNetworkDatabaseTest) {
  // First, just check the basic urls in the list of ad networks.
  EXPECT_TRUE(database()->IsAdNetwork(GURL("http://alpha.adnetwork")));
  EXPECT_TRUE(database()->IsAdNetwork(GURL("http://bravo.adnetwork")));
  EXPECT_TRUE(database()->IsAdNetwork(GURL("http://charlie.delta.adnetwork")));

  // Next, try adding some paths. These should still register.
  EXPECT_TRUE(database()->IsAdNetwork(GURL("http://alpha.adnetwork/foo")));
  EXPECT_TRUE(database()->IsAdNetwork(GURL("http://bravo.adnetwork/foo/bar")));
  EXPECT_TRUE(
      database()->IsAdNetwork(GURL("http://charlie.delta.adnetwork/foo.html")));

  // Then, try subdomains. These should not register, as they are treated as
  // different hosts.
  EXPECT_FALSE(database()->IsAdNetwork(GURL("http://foo.alpha.adnetwork")));
  EXPECT_FALSE(database()->IsAdNetwork(GURL("http://foo.bar.bravo.adnetwork")));
  EXPECT_FALSE(
      database()->IsAdNetwork(GURL("http://foo.charlie.delta.adnetwork")));

  // Check to make sure that removing a subdomain (from charlie.delta.adnetwork)
  // is considered different, and doesn't register.
  EXPECT_FALSE(database()->IsAdNetwork(GURL("http://delta.adnetwork")));

  // And, of course, try some random sites and make sure we don't miscategorize.
  EXPECT_FALSE(database()->IsAdNetwork(GURL("http://www.google.com")));
  EXPECT_FALSE(database()->IsAdNetwork(GURL("http://drive.google.com")));
  EXPECT_FALSE(database()->IsAdNetwork(GURL("https://www.google.com")));
  EXPECT_FALSE(
      database()->IsAdNetwork(GURL("file:///usr/someone/files/file.html")));
  EXPECT_FALSE(database()->IsAdNetwork(
      GURL("chrome-extension://aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa")));
}

// Test that the HashAdNetworkDatabse test works with the real file. We have
// inserted a fake URL in the used dataset for testing purposes.
TEST(HashedAdNetworkDatabaseUnitTest2, RealFile) {
  HashedAdNetworkDatabase database;
  AdNetworkDatabase* db = static_cast<AdNetworkDatabase*>(&database);
  EXPECT_TRUE(db->IsAdNetwork(
      GURL("http://definitely.surely.always.an.adnetwork")));
  EXPECT_FALSE(db->IsAdNetwork(GURL("http://definitely.not.one")));
}

}  // namespace extensions
