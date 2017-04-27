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
  std::vector<mojom::WebAppManifestSectionPtr> retrieved_manifest =
      web_app_manifest_section_table->GetWebAppManifest("https://bobpay.com");
  ASSERT_TRUE(retrieved_manifest.empty());
}

TEST_F(WebAppManifestSectionTableTest, AddAndGetManifest) {
  std::vector<uint8_t> fingerprint_one = GenerateFingerprint(1);
  std::vector<uint8_t> fingerprint_two = GenerateFingerprint(32);

  // create a bobpay web app manifest.
  std::vector<mojom::WebAppManifestSectionPtr> manifest;
  mojom::WebAppManifestSectionPtr section = mojom::WebAppManifestSection::New();
  section->id = "com.bobpay";
  section->min_version = static_cast<int64_t>(1);
  section->fingerprints.push_back(fingerprint_one);
  section->fingerprints.push_back(fingerprint_two);
  manifest.emplace_back(std::move(section));

  // Adds the manifest to the table.
  WebAppManifestSectionTable* web_app_manifest_section_table =
      WebAppManifestSectionTable::FromWebDatabase(db_.get());
  ASSERT_TRUE(web_app_manifest_section_table->AddWebAppManifest(manifest));

  // Gets and verifys the manifest.
  std::vector<mojom::WebAppManifestSectionPtr> retrieved_manifest =
      web_app_manifest_section_table->GetWebAppManifest("com.bobpay");
  ASSERT_EQ(retrieved_manifest.size(), 1U);
  ASSERT_EQ(retrieved_manifest[0]->id, "com.bobpay");
  ASSERT_EQ(retrieved_manifest[0]->min_version, 1);
  ASSERT_EQ(retrieved_manifest[0]->fingerprints.size(), 2U);

  // Verify the two fingerprints.
  ASSERT_TRUE(retrieved_manifest[0]->fingerprints[0] == fingerprint_one);
  ASSERT_TRUE(retrieved_manifest[0]->fingerprints[1] == fingerprint_two);
}

TEST_F(WebAppManifestSectionTableTest, AddAndGetMultipleManifests) {
  std::vector<uint8_t> fingerprint_one = GenerateFingerprint(1);
  std::vector<uint8_t> fingerprint_two = GenerateFingerprint(32);
  std::vector<uint8_t> fingerprint_three = GenerateFingerprint(2);
  std::vector<uint8_t> fingerprint_four = GenerateFingerprint(30);

  WebAppManifestSectionTable* web_app_manifest_section_table =
      WebAppManifestSectionTable::FromWebDatabase(db_.get());

  // Adds bobpay manifest to the table.
  std::vector<mojom::WebAppManifestSectionPtr> manifest_1;
  mojom::WebAppManifestSectionPtr manifest_1_section =
      mojom::WebAppManifestSection::New();
  manifest_1_section->id = "com.bobpay";
  manifest_1_section->min_version = static_cast<int64_t>(1);
  // Adds two finger prints.
  manifest_1_section->fingerprints.push_back(fingerprint_one);
  manifest_1_section->fingerprints.push_back(fingerprint_two);
  manifest_1.emplace_back(std::move(manifest_1_section));
  ASSERT_TRUE(web_app_manifest_section_table->AddWebAppManifest(manifest_1));

  // Adds alicepay manifest to the table.
  std::vector<mojom::WebAppManifestSectionPtr> manifest_2;
  mojom::WebAppManifestSectionPtr manifest_2_section =
      mojom::WebAppManifestSection::New();
  manifest_2_section->id = "com.alicepay";
  manifest_2_section->min_version = static_cast<int64_t>(2);
  // Adds two finger prints.
  manifest_2_section->fingerprints.push_back(fingerprint_three);
  manifest_2_section->fingerprints.push_back(fingerprint_four);
  manifest_2.emplace_back(std::move(manifest_2_section));
  ASSERT_TRUE(web_app_manifest_section_table->AddWebAppManifest(manifest_2));

  // Verifys bobpay manifest.
  std::vector<mojom::WebAppManifestSectionPtr> bobpay_manifest =
      web_app_manifest_section_table->GetWebAppManifest("com.bobpay");
  ASSERT_EQ(bobpay_manifest.size(), 1U);
  ASSERT_EQ(bobpay_manifest[0]->id, "com.bobpay");
  ASSERT_EQ(bobpay_manifest[0]->min_version, 1);
  ASSERT_EQ(bobpay_manifest[0]->fingerprints.size(), 2U);
  ASSERT_TRUE(bobpay_manifest[0]->fingerprints[0] == fingerprint_one);
  ASSERT_TRUE(bobpay_manifest[0]->fingerprints[1] == fingerprint_two);

  // Verifys alicepay manifest.
  std::vector<mojom::WebAppManifestSectionPtr> alicepay_manifest =
      web_app_manifest_section_table->GetWebAppManifest("com.alicepay");
  ASSERT_EQ(alicepay_manifest.size(), 1U);
  ASSERT_EQ(alicepay_manifest[0]->id, "com.alicepay");
  ASSERT_EQ(alicepay_manifest[0]->min_version, 2);
  ASSERT_EQ(alicepay_manifest[0]->fingerprints.size(), 2U);
  ASSERT_TRUE(alicepay_manifest[0]->fingerprints[0] == fingerprint_three);
  ASSERT_TRUE(alicepay_manifest[0]->fingerprints[1] == fingerprint_four);
}

}  // namespace

}  // namespace payments
