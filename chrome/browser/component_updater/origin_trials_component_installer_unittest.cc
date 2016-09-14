// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/origin_trials_component_installer.h"

#include <string>
#include <utility>

#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/ptr_util.h"
#include "base/values.h"
#include "base/version.h"
#include "chrome/browser/browser_process.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace {

// Mirror the constants used in the component installer. Do not share the
// constants, as want to catch inadvertent changes in the tests. The keys will
// will be generated server-side, so any changes need to be intentional and
// coordinated.
static const char kManifestOriginTrialsKey[] = "origin-trials";
static const char kManifestPublicKeyPath[] = "origin-trials.public-key";
static const char kManifestDisabledFeaturesPath[] =
    "origin-trials.disabled-features";

static const char kTestUpdateVersion[] = "1.0";
static const char kExistingPublicKey[] = "existing public key";
static const char kNewPublicKey[] = "new public key";
static const char kExistingDisabledFeature[] = "already disabled";
static const std::vector<std::string> kExistingDisabledFeatures = {
    kExistingDisabledFeature};
static const char kNewDisabledFeature1[] = "newly disabled 1";
static const char kNewDisabledFeature2[] = "newly disabled 2";
static const std::vector<std::string> kNewDisabledFeatures = {
    kNewDisabledFeature1, kNewDisabledFeature2};

}  // namespace

namespace component_updater {

class OriginTrialsComponentInstallerTest : public PlatformTest {
 public:
  OriginTrialsComponentInstallerTest()
      : testing_local_state_(TestingBrowserProcess::GetGlobal()) {}

  void SetUp() override {
    PlatformTest::SetUp();

    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    traits_.reset(new OriginTrialsComponentInstallerTraits());
  }

  void LoadUpdates(std::unique_ptr<base::DictionaryValue> manifest) {
    if (!manifest) {
      manifest = base::MakeUnique<base::DictionaryValue>();
      manifest->SetString(kManifestOriginTrialsKey, "");
    }
    ASSERT_TRUE(traits_->VerifyInstallation(*manifest, temp_dir_.GetPath()));
    const base::Version expected_version(kTestUpdateVersion);
    traits_->ComponentReady(expected_version, temp_dir_.GetPath(),
                            std::move(manifest));
  }

  void AddDisabledFeaturesToPrefs(const std::vector<std::string>& features) {
    base::ListValue disabled_feature_list;
    disabled_feature_list.AppendStrings(features);
    ListPrefUpdate update(local_state(), prefs::kOriginTrialDisabledFeatures);
    update->Swap(&disabled_feature_list);
  }

  void CheckDisabledFeaturesPrefs(const std::vector<std::string>& features) {
    ASSERT_FALSE(features.empty());

    ASSERT_TRUE(
        local_state()->HasPrefPath(prefs::kOriginTrialDisabledFeatures));

    const base::ListValue* disabled_feature_list =
        local_state()->GetList(prefs::kOriginTrialDisabledFeatures);
    ASSERT_TRUE(disabled_feature_list);

    ASSERT_EQ(features.size(), disabled_feature_list->GetSize());

    std::string disabled_feature;
    for (size_t i = 0; i < features.size(); ++i) {
      const bool found = disabled_feature_list->GetString(i, &disabled_feature);
      EXPECT_TRUE(found) << "Entry not found or not a string at index " << i;
      if (!found) {
        continue;
      }
      EXPECT_EQ(features[i], disabled_feature)
          << "Feature lists differ at index " << i;
    }
  }

  PrefService* local_state() { return g_browser_process->local_state(); }

 protected:
  base::ScopedTempDir temp_dir_;
  ScopedTestingLocalState testing_local_state_;
  std::unique_ptr<ComponentInstallerTraits> traits_;

 private:
  DISALLOW_COPY_AND_ASSIGN(OriginTrialsComponentInstallerTest);
};

TEST_F(OriginTrialsComponentInstallerTest,
       PublicKeyResetToDefaultWhenOverrideMissing) {
  local_state()->Set(prefs::kOriginTrialPublicKey,
                     base::StringValue(kExistingPublicKey));
  ASSERT_EQ(kExistingPublicKey,
            local_state()->GetString(prefs::kOriginTrialPublicKey));

  // Load with empty section in manifest
  LoadUpdates(nullptr);

  EXPECT_FALSE(local_state()->HasPrefPath(prefs::kOriginTrialPublicKey));
}

TEST_F(OriginTrialsComponentInstallerTest, PublicKeySetWhenOverrideExists) {
  ASSERT_FALSE(local_state()->HasPrefPath(prefs::kOriginTrialPublicKey));

  auto manifest = base::MakeUnique<base::DictionaryValue>();
  manifest->SetString(kManifestPublicKeyPath, kNewPublicKey);
  LoadUpdates(std::move(manifest));

  EXPECT_EQ(kNewPublicKey,
            local_state()->GetString(prefs::kOriginTrialPublicKey));
}

TEST_F(OriginTrialsComponentInstallerTest,
       DisabledFeaturesResetToDefaultWhenListMissing) {
  AddDisabledFeaturesToPrefs(kExistingDisabledFeatures);
  ASSERT_TRUE(local_state()->HasPrefPath(prefs::kOriginTrialDisabledFeatures));

  // Load with empty section in manifest
  LoadUpdates(nullptr);

  EXPECT_FALSE(local_state()->HasPrefPath(prefs::kOriginTrialDisabledFeatures));
}

TEST_F(OriginTrialsComponentInstallerTest,
       DisabledFeaturesResetToDefaultWhenListEmpty) {
  AddDisabledFeaturesToPrefs(kExistingDisabledFeatures);
  ASSERT_TRUE(local_state()->HasPrefPath(prefs::kOriginTrialDisabledFeatures));

  auto manifest = base::MakeUnique<base::DictionaryValue>();
  auto disabled_feature_list = base::MakeUnique<base::ListValue>();
  manifest->Set(kManifestDisabledFeaturesPath,
                std::move(disabled_feature_list));

  LoadUpdates(std::move(manifest));

  EXPECT_FALSE(local_state()->HasPrefPath(prefs::kOriginTrialDisabledFeatures));
}

TEST_F(OriginTrialsComponentInstallerTest, DisabledFeaturesSetWhenListExists) {
  ASSERT_FALSE(local_state()->HasPrefPath(prefs::kOriginTrialDisabledFeatures));

  auto manifest = base::MakeUnique<base::DictionaryValue>();
  auto disabled_feature_list = base::MakeUnique<base::ListValue>();
  disabled_feature_list->AppendString(kNewDisabledFeature1);
  manifest->Set(kManifestDisabledFeaturesPath,
                std::move(disabled_feature_list));

  LoadUpdates(std::move(manifest));

  std::vector<std::string> features = {kNewDisabledFeature1};
  CheckDisabledFeaturesPrefs(features);
}

TEST_F(OriginTrialsComponentInstallerTest,
       DisabledFeaturesReplacedWhenListExists) {
  AddDisabledFeaturesToPrefs(kExistingDisabledFeatures);
  ASSERT_TRUE(local_state()->HasPrefPath(prefs::kOriginTrialDisabledFeatures));

  auto manifest = base::MakeUnique<base::DictionaryValue>();
  auto disabled_feature_list = base::MakeUnique<base::ListValue>();
  disabled_feature_list->AppendStrings(kNewDisabledFeatures);
  manifest->Set(kManifestDisabledFeaturesPath,
                std::move(disabled_feature_list));

  LoadUpdates(std::move(manifest));

  CheckDisabledFeaturesPrefs(kNewDisabledFeatures);
}

}  // namespace component_updater
