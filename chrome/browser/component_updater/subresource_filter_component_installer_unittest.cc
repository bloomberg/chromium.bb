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
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/test/test_simple_task_runner.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/prefs/testing_pref_service.h"
#include "components/subresource_filter/core/browser/ruleset_service.h"
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

  void NotifyRulesetVersionAvailable(const std::string& rules,
                                     const base::Version& version) override {
    rules_ = rules;
    version_ = version;
  }

  const std::string& rules() { return rules_; }
  const base::Version& version() { return version_; }

 private:
  std::string rules_;
  base::Version version_;
  DISALLOW_COPY_AND_ASSIGN(TestRulesetService);
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
    subresource_filter::RulesetVersion::RegisterPrefs(pref_service_.registry());

    std::unique_ptr<subresource_filter::RulesetService> service(
        new TestRulesetService(&pref_service_, task_runner_,
                               ruleset_service_dir_.path()));

    TestingBrowserProcess::GetGlobal()->SetRulesetService(std::move(service));
    traits_.reset(new SubresourceFilterComponentInstallerTraits());
  }

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

  void LoadSubresourceFilterRules(const std::string& rules) {
    const base::DictionaryValue manifest;
    const base::FilePath subresource_filters_dir(component_install_dir());

    const base::FilePath first_subresource_filter_file =
        subresource_filters_dir.Append(
            FILE_PATH_LITERAL("subresource_filter_rules.blob"));
    WriteSubresourceFilterToFile(rules, first_subresource_filter_file);

    ASSERT_TRUE(
        traits_->VerifyInstallation(manifest, component_install_dir_.path()));

    const base::Version v("1.0");
    // TODO(melandory): test ComponentReady.
    traits_->LoadSubresourceFilterRulesFromDisk(component_install_dir(), v);
    // Drain the RunLoop created by the TestBrowserThreadBundle
    base::RunLoop().RunUntilIdle();
    EXPECT_EQ(v, service()->version());
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

TEST_F(SubresourceFilterComponentInstallerTest, LoadEmptyFile) {
  ASSERT_NO_FATAL_FAILURE(LoadSubresourceFilterRules(std::string()));
}

TEST_F(SubresourceFilterComponentInstallerTest, LoadFileWithData) {
  ASSERT_TRUE(service());
  const std::string rules("example.com");
  ASSERT_NO_FATAL_FAILURE(LoadSubresourceFilterRules(rules));
  EXPECT_EQ(rules, service()->rules());
}

}  // namespace component_updater
