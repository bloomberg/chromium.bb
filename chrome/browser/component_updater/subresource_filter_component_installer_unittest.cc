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
#include "base/metrics/field_trial.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/test/test_simple_task_runner.h"
#include "base/version.h"
#include "chrome/browser/after_startup_task_utils.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/component_updater/mock_component_updater_service.h"
#include "components/prefs/testing_pref_service.h"
#include "components/subresource_filter/core/browser/ruleset_service.h"
#include "components/subresource_filter/core/browser/subresource_filter_features.h"
#include "components/subresource_filter/core/browser/subresource_filter_features_test_support.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace {

class TestRulesetService : public subresource_filter::RulesetService {
 public:
  TestRulesetService(PrefService* local_state,
                     scoped_refptr<base::SequencedTaskRunner> task_runner,
                     const base::FilePath& base_dir)
      : subresource_filter::RulesetService(local_state, task_runner, base_dir) {
  }

  ~TestRulesetService() override {}

  void IndexAndStoreAndPublishRulesetVersionIfNeeded(
      const base::FilePath& unindexed_ruleset_path,
      const std::string& content_version) override {
    ruleset_path_ = unindexed_ruleset_path;
    content_version_ = content_version;
  }

  const base::FilePath& ruleset_path() const { return ruleset_path_; }
  const std::string& content_version() const { return content_version_; }

 private:
  base::FilePath ruleset_path_;
  std::string content_version_;

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
                               ruleset_service_dir_.path()));

    TestingBrowserProcess::GetGlobal()->SetRulesetService(std::move(service));
    traits_.reset(new SubresourceFilterComponentInstallerTraits());
    AfterStartupTaskUtils::SetBrowserStartupIsCompleteForTesting();
  }

  void TearDown() override { AfterStartupTaskUtils::UnsafeResetForTesting(); }

  TestRulesetService* service() {
    return static_cast<TestRulesetService*>(
        TestingBrowserProcess::GetGlobal()
            ->subresource_filter_ruleset_service());
  }

  void WriteSubresourceFilterToFile(
      const std::string subresource_filter_content,
      const base::FilePath& filename) {
    ASSERT_EQ(static_cast<int32_t>(subresource_filter_content.length()),
              base::WriteFile(filename, subresource_filter_content.c_str(),
                              subresource_filter_content.length()));
  }

  base::FilePath component_install_dir() {
    return component_install_dir_.path();
  }

  void LoadSubresourceFilterRules(const std::string& ruleset_contents) {
    std::unique_ptr<base::DictionaryValue> manifest(new base::DictionaryValue);
    const base::FilePath subresource_filters_dir(component_install_dir());

    const base::FilePath first_subresource_filter_file =
        subresource_filters_dir.Append(
            FILE_PATH_LITERAL("subresource_filter_rules.blob"));
    WriteSubresourceFilterToFile(ruleset_contents,
                                 first_subresource_filter_file);

    ASSERT_TRUE(
        traits_->VerifyInstallation(*manifest, component_install_dir_.path()));

    const base::Version expected_version("1.2.3.4");
    traits_->ComponentReady(expected_version, component_install_dir(),
                            std::move(manifest));
    base::RunLoop().RunUntilIdle();
    EXPECT_EQ(expected_version.GetString(), service()->content_version());
  }

 private:
  content::TestBrowserThreadBundle thread_bundle_;
  base::ScopedTempDir component_install_dir_;
  base::ScopedTempDir ruleset_service_dir_;
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
                            subresource_filter::kActivationStateEnabled);
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
                            subresource_filter::kActivationStateDisabled);
  std::unique_ptr<SubresourceFilterMockComponentUpdateService>
      component_updater(new SubresourceFilterMockComponentUpdateService());
  EXPECT_CALL(*component_updater, RegisterComponent(testing::_)).Times(1);
  RegisterSubresourceFilterComponent(component_updater.get());
  base::RunLoop().RunUntilIdle();
}

TEST_F(SubresourceFilterComponentInstallerTest, LoadEmptyFile) {
  ASSERT_TRUE(service());
  ASSERT_NO_FATAL_FAILURE(LoadSubresourceFilterRules(std::string()));
  std::string actual_ruleset_contents;
  ASSERT_TRUE(base::ReadFileToString(service()->ruleset_path(),
                                     &actual_ruleset_contents));
  EXPECT_TRUE(actual_ruleset_contents.empty()) << actual_ruleset_contents;
}

TEST_F(SubresourceFilterComponentInstallerTest, LoadFileWithData) {
  ASSERT_TRUE(service());
  const std::string expected_ruleset_contents("foobar");
  ASSERT_NO_FATAL_FAILURE(
      LoadSubresourceFilterRules(expected_ruleset_contents));
  std::string actual_ruleset_contents;
  ASSERT_TRUE(base::ReadFileToString(service()->ruleset_path(),
                                     &actual_ruleset_contents));
  EXPECT_EQ(expected_ruleset_contents, actual_ruleset_contents);
}

}  // namespace component_updater
