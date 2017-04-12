// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/android/web_app_manifest_section_table.h"

#include "base/files/scoped_temp_dir.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace payments {
namespace {

class WebAppManifestSectionTableTest : public testing::Test {
 public:
  WebAppManifestSectionTableTest() {}
  ~WebAppManifestSectionTableTest() override {}

 protected:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    file_ = temp_dir_.GetPath().AppendASCII("TestWebDatabase");

    table_.reset(new WebAppManifestSectionTable);
    db_.reset(new WebDatabase);
    db_->AddTable(table_.get());
    ASSERT_EQ(sql::INIT_OK, db_->Init(file_));
  }

  void TearDown() override {}

  std::vector<uint8_t> GenerateFingerprint(uint8_t seed) {
    std::vector<uint8_t> fingerprint;
    // Note that the fingerprint is calculated with SHA-256, so the length is
    // 32.
    for (size_t i = 0; i < 32U; i++) {
      fingerprint.push_back((seed + i) % 256U);
    }
    return fingerprint;
  }

  base::FilePath file_;
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<WebAppManifestSectionTable> table_;
  std::unique_ptr<WebDatabase> db_;

 private:
  DISALLOW_COPY_AND_ASSIGN(WebAppManifestSectionTableTest);
};

TEST_F(WebAppManifestSectionTableTest, GetNonExistManifest) {
  WebAppManifestSectionTable* web_app_manifest_section_table =
      WebAppManifestSectionTable::FromWebDatabase(db_.get());
  mojom::WebAppManifestSectionPtr retrieved_manifest =
      web_app_manifest_section_table->GetWebAppManifest("https://bobpay.com");
  ASSERT_TRUE(retrieved_manifest.get() == nullptr);
}

TEST_F(WebAppManifestSectionTableTest, AddAndGetManifest) {
  std::vector<uint8_t> fingerprint_one = GenerateFingerprint(1);
  std::vector<uint8_t> fingerprint_two = GenerateFingerprint(32);

  // create a bobpay web app manifest.
  mojom::WebAppManifestSectionPtr manifest =
      mojom::WebAppManifestSection::New();
  manifest->id = "com.bobpay";
  manifest->min_version = static_cast<int64_t>(1);
  manifest->fingerprints.push_back(fingerprint_one);
  manifest->fingerprints.push_back(fingerprint_two);

  // Adds the manifest to the table.
  WebAppManifestSectionTable* web_app_manifest_section_table =
      WebAppManifestSectionTable::FromWebDatabase(db_.get());
  ASSERT_TRUE(
      web_app_manifest_section_table->AddWebAppManifest(manifest.get()));

  // Gets and verifys the manifest.
  mojom::WebAppManifestSectionPtr retrieved_manifest =
      web_app_manifest_section_table->GetWebAppManifest("com.bobpay");
  ASSERT_EQ(retrieved_manifest->id, "com.bobpay");
  ASSERT_EQ(retrieved_manifest->min_version, 1);
  ASSERT_EQ(retrieved_manifest->fingerprints.size(), 2U);

  // Verify the two fingerprints.
  ASSERT_TRUE(retrieved_manifest->fingerprints[0] == fingerprint_one);
  ASSERT_TRUE(retrieved_manifest->fingerprints[1] == fingerprint_two);
}

TEST_F(WebAppManifestSectionTableTest, AddAndGetMultipleManifests) {
  std::vector<uint8_t> fingerprint_one = GenerateFingerprint(1);
  std::vector<uint8_t> fingerprint_two = GenerateFingerprint(32);
  std::vector<uint8_t> fingerprint_three = GenerateFingerprint(2);
  std::vector<uint8_t> fingerprint_four = GenerateFingerprint(30);

  WebAppManifestSectionTable* web_app_manifest_section_table =
      WebAppManifestSectionTable::FromWebDatabase(db_.get());

  // Adds bobpay manifest to the table.
  mojom::WebAppManifestSectionPtr manifest_1 =
      mojom::WebAppManifestSection::New();
  manifest_1->id = "com.bobpay";
  manifest_1->min_version = static_cast<int64_t>(1);
  // Adds two finger prints.
  manifest_1->fingerprints.push_back(fingerprint_one);
  manifest_1->fingerprints.push_back(fingerprint_two);
  ASSERT_TRUE(
      web_app_manifest_section_table->AddWebAppManifest(manifest_1.get()));

  // Adds alicepay manifest to the table.
  mojom::WebAppManifestSectionPtr manifest_2 =
      mojom::WebAppManifestSection::New();
  manifest_2->id = "com.alicepay";
  manifest_2->min_version = static_cast<int64_t>(2);
  // Adds two finger prints.
  manifest_2->fingerprints.push_back(fingerprint_three);
  manifest_2->fingerprints.push_back(fingerprint_four);
  ASSERT_TRUE(
      web_app_manifest_section_table->AddWebAppManifest(manifest_2.get()));

  // Verifys bobpay manifest.
  mojom::WebAppManifestSectionPtr bobpay_manifest =
      web_app_manifest_section_table->GetWebAppManifest("com.bobpay");
  ASSERT_EQ(bobpay_manifest->id, "com.bobpay");
  ASSERT_EQ(bobpay_manifest->min_version, 1);
  ASSERT_EQ(bobpay_manifest->fingerprints.size(), 2U);
  ASSERT_TRUE(bobpay_manifest->fingerprints[0] == fingerprint_one);
  ASSERT_TRUE(bobpay_manifest->fingerprints[1] == fingerprint_two);

  // Verifys alicepay manifest.
  mojom::WebAppManifestSectionPtr alicepay_manifest =
      web_app_manifest_section_table->GetWebAppManifest("com.alicepay");
  ASSERT_EQ(alicepay_manifest->id, "com.alicepay");
  ASSERT_EQ(alicepay_manifest->min_version, 2);
  ASSERT_EQ(alicepay_manifest->fingerprints.size(), 2U);
  ASSERT_TRUE(alicepay_manifest->fingerprints[0] == fingerprint_three);
  ASSERT_TRUE(alicepay_manifest->fingerprints[1] == fingerprint_four);
}
}
}  // payments
