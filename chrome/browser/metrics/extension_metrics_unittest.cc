// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/extension_metrics.h"

#include <string>

#include "chrome/common/metrics/proto/system_profile.pb.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/extension_set.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class TestHashedExtensionMetrics : public HashedExtensionMetrics {
 public:
  explicit TestHashedExtensionMetrics(uint64 client_id)
      : HashedExtensionMetrics(client_id) {}

  // Makes the protected HashExtension method available to testing code.
  using HashedExtensionMetrics::HashExtension;

 protected:
  // Override the GetInstalledExtensions method to return a set of extensions
  // for tests.
  virtual scoped_ptr<extensions::ExtensionSet> GetInstalledExtensions()
      OVERRIDE {
    scoped_ptr<extensions::ExtensionSet> extensions(
        new extensions::ExtensionSet());
    scoped_refptr<const extensions::Extension> extension;
    extension = extensions::ExtensionBuilder()
                    .SetManifest(extensions::DictionaryBuilder()
                                     .Set("name", "Test extension")
                                     .Set("version", "1.0.0")
                                     .Set("manifest_version", 2))
                    .SetID("ahfgeienlihckogmohjhadlkjgocpleb")
                    .Build();
    extensions->Insert(extension);
    extension = extensions::ExtensionBuilder()
                    .SetManifest(extensions::DictionaryBuilder()
                                     .Set("name", "Test extension 2")
                                     .Set("version", "1.0.0")
                                     .Set("manifest_version", 2))
                    .SetID("pknkgggnfecklokoggaggchhaebkajji")
                    .Build();
    extensions->Insert(extension);
    extension = extensions::ExtensionBuilder()
                    .SetManifest(extensions::DictionaryBuilder()
                                     .Set("name", "Colliding Extension")
                                     .Set("version", "1.0.0")
                                     .Set("manifest_version", 2))
                    .SetID("mdhofdjgenpkhlmddfaegdjddcecipmo")
                    .Build();
    extensions->Insert(extension);
    return extensions.Pass();
  }
};

}  // namespace

// Checks that the hash function used to hide precise extension IDs produces
// the expected values.
TEST(HashedExtensionMetrics, HashExtension) {
  EXPECT_EQ(978, TestHashedExtensionMetrics::HashExtension(
      "ahfgeienlihckogmohjhadlkjgocpleb", 0));
  EXPECT_EQ(10, TestHashedExtensionMetrics::HashExtension(
      "ahfgeienlihckogmohjhadlkjgocpleb", 3817));
  EXPECT_EQ(1007, TestHashedExtensionMetrics::HashExtension(
      "pknkgggnfecklokoggaggchhaebkajji", 3817));
  EXPECT_EQ(10, TestHashedExtensionMetrics::HashExtension(
      "mdhofdjgenpkhlmddfaegdjddcecipmo", 3817));
}

// Checks that the fake set of extensions provided by
// TestHashedExtensionMetrics is encoded properly.
TEST(HashedExtensionMetrics, SystemProtoEncoding) {
  metrics::SystemProfileProto system_profile;
  TestHashedExtensionMetrics extension_metrics(0x3f1bfee9);
  extension_metrics.WriteExtensionList(&system_profile);
  ASSERT_EQ(2, system_profile.occupied_extension_bucket_size());
  EXPECT_EQ(10, system_profile.occupied_extension_bucket(0));
  EXPECT_EQ(1007, system_profile.occupied_extension_bucket(1));
}
