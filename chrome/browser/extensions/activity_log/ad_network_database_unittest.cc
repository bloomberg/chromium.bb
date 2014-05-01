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
#include "crypto/secure_hash.h"
#include "crypto/sha2.h"
#include "grit/browser_resources.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/resource/resource_bundle.h"
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

// The MockResourceDelegate handles the call to get the data for the ad networks
// file, which is constructed to contain hashes for the hosts in
// |kAdNetworkHosts|.
class MockResourceDelegate : public ui::ResourceBundle::Delegate {
 public:
  MockResourceDelegate();
  virtual ~MockResourceDelegate();

 private:
  // Populate |data_resource_| with fake data for ad network url hashes.
  void MakeMockResourceData();

  // ResourceBundle::Delegate implementation. We actually only care about
  // LoadDataResourceBytes(), but they're all pure virtual, so no choice but
  // to have them all.
  virtual base::FilePath GetPathForResourcePack(
        const base::FilePath& pack_path, ui::ScaleFactor scale_factor) OVERRIDE;
  virtual base::FilePath GetPathForLocalePack(
        const base::FilePath& pack_path,
        const std::string& locale) OVERRIDE;
  virtual gfx::Image GetImageNamed(int resource_id) OVERRIDE;
  virtual gfx::Image GetNativeImageNamed(
      int resource_id, ui::ResourceBundle::ImageRTL rtl) OVERRIDE;
  virtual base::RefCountedStaticMemory* LoadDataResourceBytes(
        int resource_id, ui::ScaleFactor scale_factor) OVERRIDE;
  virtual bool GetRawDataResource(int resource_id,
                                  ui::ScaleFactor scale_factor,
                                  base::StringPiece* value) OVERRIDE;
  virtual bool GetLocalizedString(int message_id, base::string16* value)
      OVERRIDE;
  virtual scoped_ptr<gfx::Font> GetFont(ResourceBundle::FontStyle style)
      OVERRIDE;

  // The raw bits of the mocked-up data resource.
  char raw_data_[kDataResourceSize];

  // The RefCountedStaticMemory wrapper around |raw_data_|.
  scoped_refptr<base::RefCountedStaticMemory> data_resource_;
};

MockResourceDelegate::MockResourceDelegate() {
  MakeMockResourceData();
}

MockResourceDelegate::~MockResourceDelegate() {}

void MockResourceDelegate::MakeMockResourceData() {
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

  data_resource_ =
      new base::RefCountedStaticMemory(raw_data_, kDataResourceSize);
};

base::FilePath MockResourceDelegate::GetPathForResourcePack(
        const base::FilePath& pack_path, ui::ScaleFactor scale_factor) {
  return base::FilePath();
}

base::FilePath MockResourceDelegate::GetPathForLocalePack(
        const base::FilePath& pack_path,
        const std::string& locale) {
  return base::FilePath();
}

gfx::Image MockResourceDelegate::GetImageNamed(int resource_id) {
  return gfx::Image();
}

gfx::Image MockResourceDelegate::GetNativeImageNamed(
    int resource_id, ui::ResourceBundle::ImageRTL rtl) {
  return gfx::Image();
}

base::RefCountedStaticMemory* MockResourceDelegate::LoadDataResourceBytes(
        int resource_id, ui::ScaleFactor scale_factor) {
  if (resource_id != IDR_AD_NETWORK_HASHES)
    return NULL;
  return data_resource_;
}

bool MockResourceDelegate::GetRawDataResource(int resource_id,
                                              ui::ScaleFactor scale_factor,
                                              base::StringPiece* value) {
  return false;
}

bool MockResourceDelegate::GetLocalizedString(int message_id,
                                              base::string16* value) {
  return false;
}

scoped_ptr<gfx::Font> MockResourceDelegate::GetFont(
    ResourceBundle::FontStyle style) {
  return scoped_ptr<gfx::Font>();
}

}  // namespace

class AdNetworkDatabaseUnitTest : public testing::Test {
 protected:
  virtual void SetUp() OVERRIDE;

 private:
  scoped_ptr<MockResourceDelegate> mock_resource_delegate_;
};

void AdNetworkDatabaseUnitTest::SetUp() {
  // Clear the current resource bundle, and replace it with one with our own
  // Delegate.
  mock_resource_delegate_.reset(new MockResourceDelegate);
  ui::ResourceBundle::CleanupSharedInstance();
  ui::ResourceBundle::InitSharedInstanceWithLocale(
      "en-US", mock_resource_delegate_.get());
}

TEST_F(AdNetworkDatabaseUnitTest, AdNetworkDatabaseTest) {
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

}  // namespace extensions
