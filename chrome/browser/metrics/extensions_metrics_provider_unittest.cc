// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/extensions_metrics_provider.h"

#include <stdint.h>

#include <memory>
#include <string>

#include "components/metrics/client_info.h"
#include "components/metrics/metrics_service.h"
#include "components/metrics/metrics_state_manager.h"
#include "components/metrics/proto/system_profile.pb.h"
#include "components/metrics/test_enabled_state_provider.h"
#include "components/prefs/testing_pref_service.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/extension_set.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

void StoreNoClientInfoBackup(const metrics::ClientInfo& /* client_info */) {
}

std::unique_ptr<metrics::ClientInfo> ReturnNoBackup() {
  return std::unique_ptr<metrics::ClientInfo>();
}

class TestExtensionsMetricsProvider : public ExtensionsMetricsProvider {
 public:
  explicit TestExtensionsMetricsProvider(
      metrics::MetricsStateManager* metrics_state_manager)
      : ExtensionsMetricsProvider(metrics_state_manager) {}

  // Makes the protected HashExtension method available to testing code.
  using ExtensionsMetricsProvider::HashExtension;

 protected:
  // Override the GetInstalledExtensions method to return a set of extensions
  // for tests.
  std::unique_ptr<extensions::ExtensionSet> GetInstalledExtensions(
      Profile* profile) override {
    std::unique_ptr<extensions::ExtensionSet> extensions(
        new extensions::ExtensionSet());
    scoped_refptr<const extensions::Extension> extension;
    extension = extensions::ExtensionBuilder()
                    .SetManifest(extensions::DictionaryBuilder()
                                     .Set("name", "Test extension")
                                     .Set("version", "1.0.0")
                                     .Set("manifest_version", 2)
                                     .Build())
                    .SetID("ahfgeienlihckogmohjhadlkjgocpleb")
                    .Build();
    extensions->Insert(extension);
    extension = extensions::ExtensionBuilder()
                    .SetManifest(extensions::DictionaryBuilder()
                                     .Set("name", "Test extension 2")
                                     .Set("version", "1.0.0")
                                     .Set("manifest_version", 2)
                                     .Build())
                    .SetID("pknkgggnfecklokoggaggchhaebkajji")
                    .Build();
    extensions->Insert(extension);
    extension = extensions::ExtensionBuilder()
                    .SetManifest(extensions::DictionaryBuilder()
                                     .Set("name", "Colliding Extension")
                                     .Set("version", "1.0.0")
                                     .Set("manifest_version", 2)
                                     .Build())
                    .SetID("mdhofdjgenpkhlmddfaegdjddcecipmo")
                    .Build();
    extensions->Insert(extension);
    return extensions;
  }

  // Override GetClientID() to return a specific value on which test
  // expectations are based.
  uint64_t GetClientID() override { return 0x3f1bfee9; }
};

}  // namespace

// Checks that the hash function used to hide precise extension IDs produces
// the expected values.
TEST(ExtensionsMetricsProvider, HashExtension) {
  EXPECT_EQ(978,
            TestExtensionsMetricsProvider::HashExtension(
                "ahfgeienlihckogmohjhadlkjgocpleb", 0));
  EXPECT_EQ(10,
            TestExtensionsMetricsProvider::HashExtension(
                "ahfgeienlihckogmohjhadlkjgocpleb", 3817));
  EXPECT_EQ(1007,
            TestExtensionsMetricsProvider::HashExtension(
                "pknkgggnfecklokoggaggchhaebkajji", 3817));
  EXPECT_EQ(10,
            TestExtensionsMetricsProvider::HashExtension(
                "mdhofdjgenpkhlmddfaegdjddcecipmo", 3817));
}

// Checks that the fake set of extensions provided by
// TestExtensionsMetricsProvider is encoded properly.
TEST(ExtensionsMetricsProvider, SystemProtoEncoding) {
  metrics::SystemProfileProto system_profile;
  TestingPrefServiceSimple local_state;
  metrics::TestEnabledStateProvider enabled_state_provider(true, true);
  metrics::MetricsService::RegisterPrefs(local_state.registry());
  std::unique_ptr<metrics::MetricsStateManager> metrics_state_manager(
      metrics::MetricsStateManager::Create(
          &local_state, &enabled_state_provider,
          base::Bind(&StoreNoClientInfoBackup), base::Bind(&ReturnNoBackup)));
  TestExtensionsMetricsProvider extension_metrics(metrics_state_manager.get());
  extension_metrics.ProvideSystemProfileMetrics(&system_profile);
  ASSERT_EQ(2, system_profile.occupied_extension_bucket_size());
  EXPECT_EQ(10, system_profile.occupied_extension_bucket(0));
  EXPECT_EQ(1007, system_profile.occupied_extension_bucket(1));
}
