// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/subresource_filter_component_installer.h"

#include <map>
#include <memory>
#include <string>
#include <utility>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/field_trial.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/test/test_simple_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/version.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/component_updater/mock_component_updater_service.h"
#include "components/prefs/testing_pref_service.h"
#include "components/subresource_filter/content/browser/content_ruleset_service_delegate.h"
#include "components/subresource_filter/core/browser/ruleset_service.h"
#include "components/subresource_filter/core/browser/subresource_filter_features.h"
#include "components/subresource_filter/core/browser/subresource_filter_features_test_support.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace {

static const char kTestRulesetVersion[] = "1.2.3.4";

class TestRulesetService : public subresource_filter::RulesetService {
 public:
  TestRulesetService(PrefService* local_state,
                     scoped_refptr<base::SequencedTaskRunner> task_runner,
                     const base::FilePath& base_dir)
      : subresource_filter::RulesetService(
            local_state,
            task_runner,
            base::MakeUnique<
                subresource_filter::ContentRulesetServiceDelegate>(),
            base_dir) {}

  ~TestRulesetService() override {}

  using UnindexedRulesetInfo = subresource_filter::UnindexedRulesetInfo;
  void IndexAndStoreAndPublishRulesetIfNeeded(
      const UnindexedRulesetInfo& unindexed_ruleset_info) override {
    unindexed_ruleset_info_ = unindexed_ruleset_info;
  }

  const base::FilePath& ruleset_path() const {
    return unindexed_ruleset_info_.ruleset_path;
  }

  const base::FilePath& license_path() const {
    return unindexed_ruleset_info_.license_path;
  }

  const std::string& content_version() const {
    return unindexed_ruleset_info_.content_version;
  }

 private:
  UnindexedRulesetInfo unindexed_ruleset_info_;

  DISALLOW_COPY_AND_ASSIGN(TestRulesetService);
};

class SubresourceFilterMockComponentUpdateService
    : public component_updater::MockComponentUpdateService {
 public:
  SubresourceFilterMockComponentUpdateService() {}
  ~SubresourceFilterMockComponentUpdateService() override {}

  scoped_refptr<base::SequencedTaskRunner> GetSequencedTaskRunner() override {
    return base::ThreadTaskRunnerHandle::Get();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(SubresourceFilterMockComponentUpdateService);
};

}  //  namespace

namespace component_updater {

class SubresourceFilterComponentInstallerTest : public PlatformTest {
 public:
  SubresourceFilterComponentInstallerTest() {}

  void SetUp() override {
    PlatformTest::SetUp();

    ASSERT_TRUE(component_install_dir_.CreateUniqueTempDir());
    ASSERT_TRUE(ruleset_service_dir_.CreateUniqueTempDir());
    subresource_filter::IndexedRulesetVersion::RegisterPrefs(
        pref_service_.registry());

    std::unique_ptr<subresource_filter::RulesetService> service(
        new TestRulesetService(&pref_service_, task_runner_,
                               ruleset_service_dir_.GetPath()));

    TestingBrowserProcess::GetGlobal()->SetRulesetService(std::move(service));
    traits_.reset(new SubresourceFilterComponentInstallerTraits());
  }

  TestRulesetService* service() {
    return static_cast<TestRulesetService*>(
        TestingBrowserProcess::GetGlobal()
            ->subresource_filter_ruleset_service());
  }

  void WriteStringToFile(const std::string data, const base::FilePath& path) {
    ASSERT_EQ(static_cast<int32_t>(data.length()),
              base::WriteFile(path, data.data(), data.length()));
  }

  base::FilePath component_install_dir() {
    return component_install_dir_.GetPath();
  }

  // If |license_contents| is null, no license file will be created.
  void CreateTestSubresourceFilterRuleset(const std::string& ruleset_contents,
                                          const std::string* license_contents) {
    base::FilePath ruleset_data_path = component_install_dir().Append(
        SubresourceFilterComponentInstallerTraits::kRulesetDataFileName);
    ASSERT_NO_FATAL_FAILURE(
        WriteStringToFile(ruleset_contents, ruleset_data_path));

    base::FilePath license_path = component_install_dir().Append(
        SubresourceFilterComponentInstallerTraits::kLicenseFileName);
    if (license_contents) {
      ASSERT_NO_FATAL_FAILURE(
          WriteStringToFile(*license_contents, license_path));
    }
  }

  void LoadSubresourceFilterRuleset(int ruleset_format) {
    std::unique_ptr<base::DictionaryValue> manifest(new base::DictionaryValue);
    manifest->SetInteger(
        SubresourceFilterComponentInstallerTraits::kManifestRulesetFormatKey,
        ruleset_format);
    ASSERT_TRUE(
        traits_->VerifyInstallation(*manifest, component_install_dir()));
    const base::Version expected_version(kTestRulesetVersion);
    traits_->ComponentReady(expected_version, component_install_dir(),
                            std::move(manifest));
    base::RunLoop().RunUntilIdle();
  }

  update_client::InstallerAttributes GetInstallerAttributes() {
    return traits_->GetInstallerAttributes();
  }

  void ExpectInstallerTag(const char* expected_tag,
                          const char* ruleset_flavor) {
    base::FieldTrialList field_trial_list(nullptr /* entropy_provider */);
    subresource_filter::testing::ScopedSubresourceFilterFeatureToggle
        scoped_feature_toggle(base::FeatureList::OVERRIDE_ENABLE_FEATURE,
                              {{subresource_filter::kRulesetFlavorParameterName,
                                ruleset_flavor}});
    EXPECT_EQ(expected_tag,
              SubresourceFilterComponentInstallerTraits::GetInstallerTag());
  }

 private:
  base::ScopedTempDir component_install_dir_;
  base::ScopedTempDir ruleset_service_dir_;

  content::TestBrowserThreadBundle thread_bundle_;
  std::unique_ptr<SubresourceFilterComponentInstallerTraits> traits_;
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  TestingPrefServiceSimple pref_service_;

  DISALLOW_COPY_AND_ASSIGN(SubresourceFilterComponentInstallerTest);
};

TEST_F(SubresourceFilterComponentInstallerTest,
       TestComponentRegistrationWhenFeatureDisabled) {
  base::FieldTrialList field_trial_list(nullptr);
  subresource_filter::testing::ScopedSubresourceFilterFeatureToggle
      scoped_feature_toggle(base::FeatureList::OVERRIDE_DISABLE_FEATURE,
                            subresource_filter::kActivationLevelEnabled,
                            subresource_filter::kActivationScopeNoSites);
  std::unique_ptr<SubresourceFilterMockComponentUpdateService>
      component_updater(new SubresourceFilterMockComponentUpdateService());
  EXPECT_CALL(*component_updater, RegisterComponent(testing::_)).Times(0);
  RegisterSubresourceFilterComponent(component_updater.get());
  base::RunLoop().RunUntilIdle();
}

TEST_F(SubresourceFilterComponentInstallerTest,
       TestComponentRegistrationWhenFeatureEnabled) {
  base::FieldTrialList field_trial_list(nullptr);
  subresource_filter::testing::ScopedSubresourceFilterFeatureToggle
      scoped_feature_toggle(base::FeatureList::OVERRIDE_ENABLE_FEATURE,
                            subresource_filter::kActivationLevelDisabled,
                            subresource_filter::kActivationScopeNoSites);
  std::unique_ptr<SubresourceFilterMockComponentUpdateService>
      component_updater(new SubresourceFilterMockComponentUpdateService());
  EXPECT_CALL(*component_updater, RegisterComponent(testing::_)).Times(1);
  RegisterSubresourceFilterComponent(component_updater.get());
  base::RunLoop().RunUntilIdle();
}

TEST_F(SubresourceFilterComponentInstallerTest, LoadEmptyRuleset) {
  ASSERT_TRUE(service());
  ASSERT_NO_FATAL_FAILURE(
      CreateTestSubresourceFilterRuleset(std::string(), nullptr));
  ASSERT_NO_FATAL_FAILURE(LoadSubresourceFilterRuleset(
      SubresourceFilterComponentInstallerTraits::kCurrentRulesetFormat));
  EXPECT_EQ(kTestRulesetVersion, service()->content_version());
  std::string actual_ruleset_contents;
  ASSERT_TRUE(base::ReadFileToString(service()->ruleset_path(),
                                     &actual_ruleset_contents));
  EXPECT_TRUE(actual_ruleset_contents.empty()) << actual_ruleset_contents;
  EXPECT_FALSE(base::PathExists(service()->license_path()));
}

TEST_F(SubresourceFilterComponentInstallerTest, FutureVersionIgnored) {
  ASSERT_TRUE(service());
  const std::string expected_ruleset_contents = "future stuff";
  ASSERT_NO_FATAL_FAILURE(
      CreateTestSubresourceFilterRuleset(expected_ruleset_contents, nullptr));
  ASSERT_NO_FATAL_FAILURE(LoadSubresourceFilterRuleset(
      SubresourceFilterComponentInstallerTraits::kCurrentRulesetFormat + 1));
  EXPECT_EQ(std::string(), service()->content_version());
  EXPECT_TRUE(service()->ruleset_path().empty());
  EXPECT_TRUE(service()->license_path().empty());
}

TEST_F(SubresourceFilterComponentInstallerTest, LoadFileWithData) {
  ASSERT_TRUE(service());
  const std::string expected_ruleset_contents = "foobar";
  const std::string expected_license_contents = "license";
  ASSERT_NO_FATAL_FAILURE(CreateTestSubresourceFilterRuleset(
      expected_ruleset_contents, &expected_license_contents));
  ASSERT_NO_FATAL_FAILURE(LoadSubresourceFilterRuleset(
      SubresourceFilterComponentInstallerTraits::kCurrentRulesetFormat));
  EXPECT_EQ(kTestRulesetVersion, service()->content_version());
  std::string actual_ruleset_contents;
  std::string actual_license_contents;
  ASSERT_TRUE(base::ReadFileToString(service()->ruleset_path(),
                                     &actual_ruleset_contents));
  EXPECT_EQ(expected_ruleset_contents, actual_ruleset_contents);
  ASSERT_TRUE(base::ReadFileToString(service()->license_path(),
                                     &actual_license_contents));
  EXPECT_EQ(expected_license_contents, actual_license_contents);
}

TEST_F(SubresourceFilterComponentInstallerTest, InstallerTag) {
  ExpectInstallerTag("", "");
  ExpectInstallerTag("a", "a");
  ExpectInstallerTag("b", "b");
  ExpectInstallerTag("c", "c");
  ExpectInstallerTag("d", "d");
  ExpectInstallerTag("invalid", "e");
  ExpectInstallerTag("invalid", "foo");
}

TEST_F(SubresourceFilterComponentInstallerTest, InstallerAttributesDefault) {
  base::FieldTrialList field_trial_list(nullptr /* entropy_provider */);
  subresource_filter::testing::ScopedSubresourceFilterFeatureToggle
      scoped_feature_toggle(base::FeatureList::OVERRIDE_ENABLE_FEATURE,
                            std::map<std::string, std::string>());
  EXPECT_EQ(update_client::InstallerAttributes(), GetInstallerAttributes());
}

TEST_F(SubresourceFilterComponentInstallerTest, InstallerAttributesCustomTag) {
  constexpr char kTagKey[] = "tag";
  constexpr char kTagValue[] = "a";

  base::FieldTrialList field_trial_list(nullptr /* entropy_provider */);
  subresource_filter::testing::ScopedSubresourceFilterFeatureToggle
      scoped_feature_toggle(
          base::FeatureList::OVERRIDE_ENABLE_FEATURE,
          {{subresource_filter::kRulesetFlavorParameterName, kTagValue}});
  EXPECT_EQ(update_client::InstallerAttributes({{kTagKey, kTagValue}}),
            GetInstallerAttributes());
}

TEST_F(SubresourceFilterComponentInstallerTest,
       InstallerAttributesFeatureDisabled) {
  constexpr char kTagValue[] = "test_value";

  base::FieldTrialList field_trial_list(nullptr /* entropy_provider */);
  subresource_filter::testing::ScopedSubresourceFilterFeatureToggle
      scoped_feature_toggle(
          base::FeatureList::OVERRIDE_USE_DEFAULT,
          {{subresource_filter::kRulesetFlavorParameterName, kTagValue}});
  EXPECT_EQ(update_client::InstallerAttributes(), GetInstallerAttributes());
}

}  // namespace component_updater
