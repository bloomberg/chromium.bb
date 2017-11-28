// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/sw_reporter_installer_win.h"

#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/json/json_reader.h"
#include "base/macros.h"
#include "base/metrics/field_trial.h"
#include "base/strings/stringprintf.h"
#include "base/test/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "base/version.h"
#include "chrome/browser/safe_browsing/chrome_cleaner/reporter_runner_win.h"
#include "components/chrome_cleaner/public/constants/constants.h"
#include "components/variations/variations_params_manager.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace component_updater {

namespace {

constexpr char kExperimentGroupName[] = "my_test_engine_group";

constexpr char kErrorHistogramName[] = "SoftwareReporter.ExperimentErrors";
constexpr char kExperimentTag[] = "experiment_tag";
constexpr char kMissingTag[] = "missing_tag";

using safe_browsing::OnReporterSequenceDone;
using safe_browsing::SwReporterInvocation;
using safe_browsing::SwReporterInvocationResult;
using safe_browsing::SwReporterInvocationSequence;
using safe_browsing::SwReporterInvocationType;

}  // namespace

// Test parameters:
//   - SwReporterInvocationType invocation_type_: the type of invocation
//         tested.
class SwReporterInstallerTest
    : public ::testing::TestWithParam<SwReporterInvocationType> {
 public:
  SwReporterInstallerTest()
      : launched_callback_(
            base::Bind(&SwReporterInstallerTest::SwReporterLaunched,
                       base::Unretained(this))),
        default_version_("1.2.3"),
        default_path_(L"C:\\full\\path\\to\\download") {
    invocation_type_ = GetParam();
  }

  ~SwReporterInstallerTest() override {}

 protected:
  void SwReporterLaunched(SwReporterInvocationType invocation_type,
                          SwReporterInvocationSequence&& invocations) {
    ASSERT_TRUE(launched_invocations_.container().empty());
    launched_invocations_ = std::move(invocations);
  }

  base::FilePath MakeTestFilePath(const base::FilePath& path) const {
    return path.Append(L"software_reporter_tool.exe");
  }

  void ExpectEmptyAttributes(const SwReporterInstallerPolicy& policy) const {
    update_client::InstallerAttributes attributes =
        policy.GetInstallerAttributes();
    EXPECT_TRUE(attributes.empty());
  }

  // Expects that the SwReporter was launched exactly once, with a session-id
  // switch.
  void ExpectDefaultInvocation() const {
    EXPECT_EQ(default_version_, launched_invocations_.version());
    ASSERT_EQ(1U, launched_invocations_.container().size());

    const SwReporterInvocation& invocation =
        launched_invocations_.container().front();
    EXPECT_EQ(MakeTestFilePath(default_path_),
              invocation.command_line().GetProgram());
    EXPECT_EQ(1U, invocation.command_line().GetSwitches().size());
    EXPECT_FALSE(invocation.command_line()
                     .GetSwitchValueASCII(chrome_cleaner::kSessionIdSwitch)
                     .empty());
    EXPECT_TRUE(invocation.command_line().GetArgs().empty());
    EXPECT_TRUE(invocation.suffix().empty());
    EXPECT_EQ(SwReporterInvocation::BEHAVIOURS_ENABLED_BY_DEFAULT,
              invocation.supported_behaviours());
  }

  // |ComponentReady| asserts that it is run on the UI thread, so we must
  // create test threads before calling it.
  content::TestBrowserThreadBundle threads_;

  // Bound callback to the |SwReporterLaunched| method.
  SwReporterRunner launched_callback_;

  // Default parameters for |ComponentReady|.
  base::Version default_version_;
  base::FilePath default_path_;

  // Results of running |ComponentReady|.
  SwReporterInvocationSequence launched_invocations_;

  SwReporterInvocationType invocation_type_ =
      SwReporterInvocationType::kPeriodicRun;

 private:
  DISALLOW_COPY_AND_ASSIGN(SwReporterInstallerTest);
};

// This class contains extended setup that is only used for tests of the
// experimental reporter.
class ExperimentalSwReporterInstallerTest : public SwReporterInstallerTest {
 public:
  ExperimentalSwReporterInstallerTest() {}
  ~ExperimentalSwReporterInstallerTest() override {}

 protected:
  void CreateFeatureWithoutTag() {
    std::map<std::string, std::string> params;
    CreateFeatureWithParams(params);
  }

  void CreateFeatureWithTag(const std::string& tag) {
    std::map<std::string, std::string> params;
    params["tag"] = tag;
    CreateFeatureWithParams(params);
  }

  void CreateFeatureWithParams(
      const std::map<std::string, std::string>& params) {
    constexpr char kFeatureAndTrialName[] = "ExperimentalSwReporterEngine";

    std::map<std::string, std::string> params_with_group = params;
    params_with_group["experiment_group_for_reporting"] = kExperimentGroupName;

    // Assign the given variation params to the experiment group until
    // |variations_| goes out of scope when the test exits. This will also
    // create a FieldTrial for this group and associate the params with the
    // feature.
    variations_ = std::make_unique<variations::testing::VariationParamsManager>(
        kFeatureAndTrialName,  // trial_name
        params_with_group,
        std::set<std::string>{kFeatureAndTrialName});  // associated_features
  }

  void ExpectAttributesWithTag(const SwReporterInstallerPolicy& policy,
                               const std::string& tag) {
    update_client::InstallerAttributes attributes =
        policy.GetInstallerAttributes();
    EXPECT_EQ(1U, attributes.size());
    EXPECT_EQ(tag, attributes["tag"]);
  }

  // Expects that the SwReporter was launched exactly once, with the given
  // |expected_suffix|, an experiment group, a session-id, and one
  // |expected_additional_argument| on the command-line.
  // (|expected_additional_argument| mainly exists to test that arguments are
  // included at all, so there is no need to test for combinations of multiple
  // arguments and switches in this function.)
  void ExpectExperimentalInvocation(
      const std::string& expected_suffix,
      const base::string16& expected_additional_argument) {
    EXPECT_EQ(default_version_, launched_invocations_.version());
    ASSERT_EQ(1U, launched_invocations_.container().size());

    const SwReporterInvocation& invocation =
        launched_invocations_.container().front();
    EXPECT_EQ(MakeTestFilePath(default_path_),
              invocation.command_line().GetProgram());
    EXPECT_FALSE(invocation.command_line()
                     .GetSwitchValueASCII(chrome_cleaner::kSessionIdSwitch)
                     .empty());
    EXPECT_EQ(kExperimentGroupName,
              invocation.command_line().GetSwitchValueASCII(
                  chrome_cleaner::kEngineExperimentGroupSwitch));

    if (expected_suffix.empty()) {
      EXPECT_EQ(2U, invocation.command_line().GetSwitches().size());
      EXPECT_TRUE(invocation.suffix().empty());
    } else {
      EXPECT_EQ(3U, invocation.command_line().GetSwitches().size());
      EXPECT_EQ(expected_suffix, invocation.command_line().GetSwitchValueASCII(
                                     chrome_cleaner::kRegistrySuffixSwitch));
      EXPECT_EQ(expected_suffix, invocation.suffix());
    }

    if (expected_additional_argument.empty()) {
      EXPECT_TRUE(invocation.command_line().GetArgs().empty());
    } else {
      EXPECT_EQ(1U, invocation.command_line().GetArgs().size());
      EXPECT_EQ(expected_additional_argument,
                invocation.command_line().GetArgs()[0]);
    }

    EXPECT_EQ(0U, invocation.supported_behaviours());
    histograms_.ExpectTotalCount(kErrorHistogramName, 0);
  }

  // Expects that the SwReporter was launched with the given |expected_suffix|,
  // |expected_engine|, and |expected_behaviours|, as part of a series of
  // multiple invocations.
  void ConsumeAndCheckExperimentalInvocationInSeries(
      const std::string& expected_suffix,
      const std::string& expected_engine,
      SwReporterInvocation::Behaviours expected_behaviours,
      std::string* out_session_id) {
    SCOPED_TRACE("Invocation with suffix " + expected_suffix);
    SwReporterInvocation invocation = launched_invocations_.container().front();
    launched_invocations_.mutable_container().pop();
    EXPECT_EQ(MakeTestFilePath(default_path_),
              invocation.command_line().GetProgram());
    // There should be one switch added from the manifest, plus registry-suffix
    // added automatically.
    EXPECT_EQ(4U, invocation.command_line().GetSwitches().size());
    EXPECT_EQ(expected_engine,
              invocation.command_line().GetSwitchValueASCII("engine"));
    EXPECT_EQ(expected_suffix, invocation.command_line().GetSwitchValueASCII(
                                   chrome_cleaner::kRegistrySuffixSwitch));
    *out_session_id = invocation.command_line().GetSwitchValueASCII(
        chrome_cleaner::kSessionIdSwitch);
    EXPECT_FALSE(out_session_id->empty());
    EXPECT_EQ(kExperimentGroupName,
              invocation.command_line().GetSwitchValueASCII(
                  chrome_cleaner::kEngineExperimentGroupSwitch));
    ASSERT_TRUE(invocation.command_line().GetArgs().empty());
    EXPECT_EQ(expected_suffix, invocation.suffix());
    EXPECT_EQ(expected_behaviours, invocation.supported_behaviours());
  }

  void ExpectLaunchError() {
    // The SwReporter should not be launched, and an error should be logged.
    EXPECT_TRUE(launched_invocations_.container().empty());
    histograms_.ExpectUniqueSample(kErrorHistogramName,
                                   SW_REPORTER_EXPERIMENT_ERROR_BAD_PARAMS, 1);
  }

  std::unique_ptr<variations::testing::VariationParamsManager> variations_;
  base::test::ScopedFeatureList scoped_feature_list_;
  base::HistogramTester histograms_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ExperimentalSwReporterInstallerTest);
};

TEST_P(SwReporterInstallerTest, Default) {
  SwReporterInstallerPolicy policy(launched_callback_, false, invocation_type_,
                                   OnReporterSequenceDone());
  ExpectEmptyAttributes(policy);
  policy.ComponentReady(default_version_, default_path_,
                        std::make_unique<base::DictionaryValue>());
  ExpectDefaultInvocation();
}

INSTANTIATE_TEST_CASE_P(
    All,
    SwReporterInstallerTest,
    ::testing::Values(
        SwReporterInvocationType::kPeriodicRun,
        SwReporterInvocationType::kUserInitiatedWithLogsDisallowed,
        SwReporterInvocationType::kUserInitiatedWithLogsAllowed));

TEST_P(ExperimentalSwReporterInstallerTest, NoExperimentConfig) {
  // Even if the experiment is supported on this hardware, the user shouldn't
  // be enrolled unless enabled through variations.
  SwReporterInstallerPolicy policy(launched_callback_, true, invocation_type_,
                                   OnReporterSequenceDone());
  ExpectEmptyAttributes(policy);
  policy.ComponentReady(default_version_, default_path_,
                        std::make_unique<base::DictionaryValue>());
  ExpectDefaultInvocation();
}

TEST_P(ExperimentalSwReporterInstallerTest, ExperimentUnsupported) {
  // Even if the experiment config is enabled in variations, the user shouldn't
  // be enrolled if the hardware doesn't support it.
  SwReporterInstallerPolicy policy(launched_callback_, false, invocation_type_,
                                   OnReporterSequenceDone());
  CreateFeatureWithTag(kExperimentTag);
  ExpectEmptyAttributes(policy);
  policy.ComponentReady(default_version_, default_path_,
                        std::make_unique<base::DictionaryValue>());
  ExpectDefaultInvocation();
}

TEST_P(ExperimentalSwReporterInstallerTest, ExperimentMissingTag) {
  SwReporterInstallerPolicy policy(launched_callback_, true, invocation_type_,
                                   OnReporterSequenceDone());
  CreateFeatureWithoutTag();
  ExpectAttributesWithTag(policy, kMissingTag);
  histograms_.ExpectUniqueSample(kErrorHistogramName,
                                 SW_REPORTER_EXPERIMENT_ERROR_BAD_TAG, 1);
}

TEST_P(ExperimentalSwReporterInstallerTest, ExperimentInvalidTag) {
  SwReporterInstallerPolicy policy(launched_callback_, true, invocation_type_,
                                   OnReporterSequenceDone());
  CreateFeatureWithTag("tag with invalid whitespace chars");
  ExpectAttributesWithTag(policy, kMissingTag);
  histograms_.ExpectUniqueSample(kErrorHistogramName,
                                 SW_REPORTER_EXPERIMENT_ERROR_BAD_TAG, 1);
}

TEST_P(ExperimentalSwReporterInstallerTest, ExperimentTagTooLong) {
  SwReporterInstallerPolicy policy(launched_callback_, true, invocation_type_,
                                   OnReporterSequenceDone());
  std::string tag_too_long(500, 'x');
  CreateFeatureWithTag(tag_too_long);
  ExpectAttributesWithTag(policy, kMissingTag);
  histograms_.ExpectUniqueSample(kErrorHistogramName,
                                 SW_REPORTER_EXPERIMENT_ERROR_BAD_TAG, 1);
}

TEST_P(ExperimentalSwReporterInstallerTest, ExperimentEmptyTag) {
  SwReporterInstallerPolicy policy(launched_callback_, true, invocation_type_,
                                   OnReporterSequenceDone());
  CreateFeatureWithTag("");
  ExpectAttributesWithTag(policy, kMissingTag);
  histograms_.ExpectUniqueSample(kErrorHistogramName,
                                 SW_REPORTER_EXPERIMENT_ERROR_BAD_TAG, 1);
}

TEST_P(ExperimentalSwReporterInstallerTest, SingleInvocation) {
  SwReporterInstallerPolicy policy(launched_callback_, true, invocation_type_,
                                   OnReporterSequenceDone());
  CreateFeatureWithTag(kExperimentTag);
  ExpectAttributesWithTag(policy, kExperimentTag);

  static constexpr char kTestManifest[] =
      "{\"launch_params\": ["
      "  {"
      "    \"arguments\": [\"--engine=experimental\", \"random argument\"],"
      "    \"suffix\": \"TestSuffix\","
      "    \"prompt\": false"
      "  }"
      "]}";
  policy.ComponentReady(
      default_version_, default_path_,
      base::DictionaryValue::From(base::JSONReader::Read(kTestManifest)));

  // The SwReporter should be launched once with the given arguments.
  EXPECT_EQ(default_version_, launched_invocations_.version());
  ASSERT_EQ(1U, launched_invocations_.container().size());

  const SwReporterInvocation& invocation =
      launched_invocations_.container().front();
  EXPECT_EQ(MakeTestFilePath(default_path_),
            invocation.command_line().GetProgram());
  EXPECT_EQ(4U, invocation.command_line().GetSwitches().size());
  EXPECT_EQ("experimental",
            invocation.command_line().GetSwitchValueASCII("engine"));
  EXPECT_EQ("TestSuffix", invocation.command_line().GetSwitchValueASCII(
                              chrome_cleaner::kRegistrySuffixSwitch));
  EXPECT_FALSE(invocation.command_line()
                   .GetSwitchValueASCII(chrome_cleaner::kSessionIdSwitch)
                   .empty());
  EXPECT_EQ(kExperimentGroupName,
            invocation.command_line().GetSwitchValueASCII(
                chrome_cleaner::kEngineExperimentGroupSwitch));
  ASSERT_EQ(1U, invocation.command_line().GetArgs().size());
  EXPECT_EQ(L"random argument", invocation.command_line().GetArgs()[0]);
  EXPECT_EQ("TestSuffix", invocation.suffix());
  EXPECT_EQ(0U, invocation.supported_behaviours());
  histograms_.ExpectTotalCount(kErrorHistogramName, 0);
}

TEST_P(ExperimentalSwReporterInstallerTest, MultipleInvocations) {
  SwReporterInstallerPolicy policy(launched_callback_, true, invocation_type_,
                                   OnReporterSequenceDone());
  CreateFeatureWithTag(kExperimentTag);
  ExpectAttributesWithTag(policy, kExperimentTag);

  static constexpr char kTestManifest[] =
      "{\"launch_params\": ["
      "  {"
      "    \"arguments\": [\"--engine=experimental\"],"
      "    \"suffix\": \"TestSuffix\","
      "    \"prompt\": false,"
      "    \"allow-reporter-logs\": true"
      "  },"
      "  {"
      "    \"arguments\": [\"--engine=second\"],"
      "    \"suffix\": \"SecondSuffix\","
      "    \"prompt\": true,"
      "    \"allow-reporter-logs\": false"
      "  },"
      "  {"
      "    \"arguments\": [\"--engine=third\"],"
      "    \"suffix\": \"ThirdSuffix\""
      "  },"
      "  {"
      "    \"arguments\": [\"--engine=fourth\"],"
      "    \"suffix\": \"FourthSuffix\","
      "    \"prompt\": true,"
      "    \"allow-reporter-logs\": true"
      "  }"

      "]}";
  policy.ComponentReady(
      default_version_, default_path_,
      base::DictionaryValue::From(base::JSONReader::Read(kTestManifest)));

  // The SwReporter should be launched four times with the given arguments.
  EXPECT_EQ(default_version_, launched_invocations_.version());
  ASSERT_EQ(4U, launched_invocations_.container().size());
  std::string out_session_id;
  ConsumeAndCheckExperimentalInvocationInSeries("TestSuffix", "experimental",
                                                /*supported_behaviours=*/0,
                                                &out_session_id);

  const std::string first_session_id(out_session_id);

  ConsumeAndCheckExperimentalInvocationInSeries(
      "SecondSuffix", "second", SwReporterInvocation::BEHAVIOUR_TRIGGER_PROMPT,
      &out_session_id);
  EXPECT_EQ(first_session_id, out_session_id);

  ConsumeAndCheckExperimentalInvocationInSeries("ThirdSuffix", "third", 0U,
                                                &out_session_id);
  EXPECT_EQ(first_session_id, out_session_id);

  ConsumeAndCheckExperimentalInvocationInSeries(
      "FourthSuffix", "fourth", SwReporterInvocation::BEHAVIOUR_TRIGGER_PROMPT,
      &out_session_id);
  EXPECT_EQ(first_session_id, out_session_id);

  histograms_.ExpectTotalCount(kErrorHistogramName, 0);
}

TEST_P(ExperimentalSwReporterInstallerTest, MissingSuffix) {
  SwReporterInstallerPolicy policy(launched_callback_, true, invocation_type_,
                                   OnReporterSequenceDone());
  CreateFeatureWithTag(kExperimentTag);

  static constexpr char kTestManifest[] =
      "{\"launch_params\": ["
      "  {"
      "    \"arguments\": [\"random argument\"]"
      "  }"
      "]}";
  policy.ComponentReady(
      default_version_, default_path_,
      base::DictionaryValue::From(base::JSONReader::Read(kTestManifest)));

  ExpectLaunchError();
}

TEST_P(ExperimentalSwReporterInstallerTest, EmptySuffix) {
  SwReporterInstallerPolicy policy(launched_callback_, true, invocation_type_,
                                   OnReporterSequenceDone());
  CreateFeatureWithTag(kExperimentTag);

  static constexpr char kTestManifest[] =
      "{\"launch_params\": ["
      "  {"
      "    \"suffix\": \"\","
      "    \"arguments\": [\"random argument\"]"
      "  }"
      "]}";
  policy.ComponentReady(
      default_version_, default_path_,
      base::DictionaryValue::From(base::JSONReader::Read(kTestManifest)));

  ExpectExperimentalInvocation("", L"random argument");
}

TEST_P(ExperimentalSwReporterInstallerTest, MissingSuffixAndArgs) {
  SwReporterInstallerPolicy policy(launched_callback_, true, invocation_type_,
                                   OnReporterSequenceDone());
  CreateFeatureWithTag(kExperimentTag);

  static constexpr char kTestManifest[] =
      "{\"launch_params\": ["
      "  {"
      "  }"
      "]}";
  policy.ComponentReady(
      default_version_, default_path_,
      base::DictionaryValue::From(base::JSONReader::Read(kTestManifest)));

  ExpectLaunchError();
}

TEST_P(ExperimentalSwReporterInstallerTest, EmptySuffixAndArgs) {
  SwReporterInstallerPolicy policy(launched_callback_, true, invocation_type_,
                                   OnReporterSequenceDone());
  CreateFeatureWithTag(kExperimentTag);

  static constexpr char kTestManifest[] =
      "{\"launch_params\": ["
      "  {"
      "    \"suffix\": \"\","
      "    \"arguments\": []"
      "  }"
      "]}";
  policy.ComponentReady(
      default_version_, default_path_,
      base::DictionaryValue::From(base::JSONReader::Read(kTestManifest)));

  ExpectExperimentalInvocation("", L"");
}

TEST_P(ExperimentalSwReporterInstallerTest, EmptySuffixAndArgs2) {
  SwReporterInstallerPolicy policy(launched_callback_, true, invocation_type_,
                                   OnReporterSequenceDone());
  CreateFeatureWithTag(kExperimentTag);

  static constexpr char kTestManifest[] =
      "{\"launch_params\": ["
      "  {"
      "    \"suffix\": \"\","
      "    \"arguments\": [\"\"]"
      "  }"
      "]}";
  policy.ComponentReady(
      default_version_, default_path_,
      base::DictionaryValue::From(base::JSONReader::Read(kTestManifest)));

  ExpectExperimentalInvocation("", L"");
}

TEST_P(ExperimentalSwReporterInstallerTest, MissingArguments) {
  SwReporterInstallerPolicy policy(launched_callback_, true, invocation_type_,
                                   OnReporterSequenceDone());
  CreateFeatureWithTag(kExperimentTag);

  static constexpr char kTestManifest[] =
      "{\"launch_params\": ["
      "  {"
      "    \"suffix\": \"TestSuffix\""
      "  }"
      "]}";
  policy.ComponentReady(
      default_version_, default_path_,
      base::DictionaryValue::From(base::JSONReader::Read(kTestManifest)));

  ExpectLaunchError();
}

TEST_P(ExperimentalSwReporterInstallerTest, EmptyArguments) {
  SwReporterInstallerPolicy policy(launched_callback_, true, invocation_type_,
                                   OnReporterSequenceDone());
  CreateFeatureWithTag(kExperimentTag);

  static constexpr char kTestManifest[] =
      "{\"launch_params\": ["
      "  {"
      "    \"suffix\": \"TestSuffix\","
      "    \"arguments\": []"
      "  }"
      "]}";
  policy.ComponentReady(
      default_version_, default_path_,
      base::DictionaryValue::From(base::JSONReader::Read(kTestManifest)));

  ExpectExperimentalInvocation("TestSuffix", L"");
}

TEST_P(ExperimentalSwReporterInstallerTest, EmptyArguments2) {
  SwReporterInstallerPolicy policy(launched_callback_, true, invocation_type_,
                                   OnReporterSequenceDone());
  CreateFeatureWithTag(kExperimentTag);

  static constexpr char kTestManifest[] =
      "{\"launch_params\": ["
      "  {"
      "    \"suffix\": \"TestSuffix\","
      "    \"arguments\": [\"\"]"
      "  }"
      "]}";
  policy.ComponentReady(
      default_version_, default_path_,
      base::DictionaryValue::From(base::JSONReader::Read(kTestManifest)));

  ExpectExperimentalInvocation("TestSuffix", L"");
}

TEST_P(ExperimentalSwReporterInstallerTest, EmptyManifest) {
  SwReporterInstallerPolicy policy(launched_callback_, true, invocation_type_,
                                   OnReporterSequenceDone());
  CreateFeatureWithTag(kExperimentTag);

  static constexpr char kTestManifest[] = "{}";
  policy.ComponentReady(
      default_version_, default_path_,
      base::DictionaryValue::From(base::JSONReader::Read(kTestManifest)));

  // The SwReporter should not be launched, and an error should be logged.
  EXPECT_TRUE(launched_invocations_.container().empty());
  histograms_.ExpectUniqueSample(
      kErrorHistogramName, SW_REPORTER_EXPERIMENT_ERROR_MISSING_PARAMS, 1);
}

TEST_P(ExperimentalSwReporterInstallerTest, EmptyLaunchParams) {
  SwReporterInstallerPolicy policy(launched_callback_, true, invocation_type_,
                                   OnReporterSequenceDone());
  CreateFeatureWithTag(kExperimentTag);

  static constexpr char kTestManifest[] = "{\"launch_params\": []}";
  policy.ComponentReady(
      default_version_, default_path_,
      base::DictionaryValue::From(base::JSONReader::Read(kTestManifest)));

  // The SwReporter should not be launched, and an error should be logged.
  EXPECT_TRUE(launched_invocations_.container().empty());
  histograms_.ExpectUniqueSample(kErrorHistogramName,
                                 SW_REPORTER_EXPERIMENT_ERROR_BAD_PARAMS, 1);
}

TEST_P(ExperimentalSwReporterInstallerTest, EmptyLaunchParams2) {
  SwReporterInstallerPolicy policy(launched_callback_, true, invocation_type_,
                                   OnReporterSequenceDone());
  CreateFeatureWithTag(kExperimentTag);

  static constexpr char kTestManifest[] = "{\"launch_params\": {}}";
  policy.ComponentReady(
      default_version_, default_path_,
      base::DictionaryValue::From(base::JSONReader::Read(kTestManifest)));

  // The SwReporter should not be launched, and an error should be logged.
  EXPECT_TRUE(launched_invocations_.container().empty());
  histograms_.ExpectUniqueSample(kErrorHistogramName,
                                 SW_REPORTER_EXPERIMENT_ERROR_BAD_PARAMS, 1);
}

TEST_P(ExperimentalSwReporterInstallerTest, BadSuffix) {
  SwReporterInstallerPolicy policy(launched_callback_, true, invocation_type_,
                                   OnReporterSequenceDone());
  CreateFeatureWithTag(kExperimentTag);

  static constexpr char kTestManifest[] =
      "{\"launch_params\": ["
      "  {"
      "    \"arguments\": [\"--engine=experimental\"],"
      "    \"suffix\": \"invalid whitespace characters\""
      "  }"
      "]}";
  policy.ComponentReady(
      default_version_, default_path_,
      base::DictionaryValue::From(base::JSONReader::Read(kTestManifest)));

  // The SwReporter should not be launched, and an error should be logged.
  EXPECT_TRUE(launched_invocations_.container().empty());
  histograms_.ExpectUniqueSample(kErrorHistogramName,
                                 SW_REPORTER_EXPERIMENT_ERROR_BAD_PARAMS, 1);
}

TEST_P(ExperimentalSwReporterInstallerTest, SuffixTooLong) {
  SwReporterInstallerPolicy policy(launched_callback_, true, invocation_type_,
                                   OnReporterSequenceDone());
  CreateFeatureWithTag(kExperimentTag);

  static constexpr char kTestManifest[] =
      "{\"launch_params\": ["
      "  {"
      "    \"arguments\": [\"--engine=experimental\"],"
      "    \"suffix\": \"%s\""
      "  }"
      "]}";
  std::string suffix_too_long(500, 'x');
  std::string manifest =
      base::StringPrintf(kTestManifest, suffix_too_long.c_str());
  policy.ComponentReady(
      default_version_, default_path_,
      base::DictionaryValue::From(base::JSONReader::Read(manifest)));

  // The SwReporter should not be launched, and an error should be logged.
  EXPECT_TRUE(launched_invocations_.container().empty());
  histograms_.ExpectUniqueSample(kErrorHistogramName,
                                 SW_REPORTER_EXPERIMENT_ERROR_BAD_PARAMS, 1);
}

TEST_P(ExperimentalSwReporterInstallerTest,
       BadTypesInManifest_ArgumentsIsNotAList) {
  SwReporterInstallerPolicy policy(launched_callback_, true, invocation_type_,
                                   OnReporterSequenceDone());
  CreateFeatureWithTag(kExperimentTag);

  // This has a string instead of a list for "arguments".
  static constexpr char kTestManifest[] =
      "{\"launch_params\": ["
      "  {"
      "    \"arguments\": \"--engine=experimental\","
      "    \"suffix\": \"TestSuffix\""
      "  }"
      "]}";
  policy.ComponentReady(
      default_version_, default_path_,
      base::DictionaryValue::From(base::JSONReader::Read(kTestManifest)));

  // The SwReporter should not be launched, and an error should be logged.
  EXPECT_TRUE(launched_invocations_.container().empty());
  histograms_.ExpectUniqueSample(kErrorHistogramName,
                                 SW_REPORTER_EXPERIMENT_ERROR_BAD_PARAMS, 1);
}

TEST_P(ExperimentalSwReporterInstallerTest,
       BadTypesInManifest_InvocationParamsIsNotAList) {
  SwReporterInstallerPolicy policy(launched_callback_, true, invocation_type_,
                                   OnReporterSequenceDone());
  CreateFeatureWithTag(kExperimentTag);

  // This has the invocation parameters as direct children of "launch_params",
  // instead of using a list.
  static constexpr char kTestManifest[] =
      "{\"launch_params\": "
      "  {"
      "    \"arguments\": [\"--engine=experimental\"],"
      "    \"suffix\": \"TestSuffix\""
      "  }"
      "}";
  policy.ComponentReady(
      default_version_, default_path_,
      base::DictionaryValue::From(base::JSONReader::Read(kTestManifest)));

  // The SwReporter should not be launched, and an error should be logged.
  EXPECT_TRUE(launched_invocations_.container().empty());
  histograms_.ExpectUniqueSample(kErrorHistogramName,
                                 SW_REPORTER_EXPERIMENT_ERROR_BAD_PARAMS, 1);
}

TEST_P(ExperimentalSwReporterInstallerTest, BadTypesInManifest_SuffixIsAList) {
  SwReporterInstallerPolicy policy(launched_callback_, true, invocation_type_,
                                   OnReporterSequenceDone());
  CreateFeatureWithTag(kExperimentTag);

  // This has a list for suffix as well as for arguments.
  static constexpr char kTestManifest[] =
      "{\"launch_params\": ["
      "  {"
      "    \"arguments\": [\"--engine=experimental\"],"
      "    \"suffix\": [\"TestSuffix\"]"
      "  }"
      "]}";
  policy.ComponentReady(
      default_version_, default_path_,
      base::DictionaryValue::From(base::JSONReader::Read(kTestManifest)));

  // The SwReporter should not be launched, and an error should be logged.
  EXPECT_TRUE(launched_invocations_.container().empty());
  histograms_.ExpectUniqueSample(kErrorHistogramName,
                                 SW_REPORTER_EXPERIMENT_ERROR_BAD_PARAMS, 1);
}

TEST_P(ExperimentalSwReporterInstallerTest,
       BadTypesInManifest_PromptIsNotABoolean) {
  SwReporterInstallerPolicy policy(launched_callback_, true, invocation_type_,
                                   OnReporterSequenceDone());
  CreateFeatureWithTag(kExperimentTag);

  // This has an int instead of a bool for prompt.
  static constexpr char kTestManifest[] =
      "{\"launch_params\": ["
      "  {"
      "    \"arguments\": [\"--engine=experimental\"],"
      "    \"suffix\": \"TestSuffix\","
      "    \"prompt\": 1"
      "  }"
      "]}";
  policy.ComponentReady(
      default_version_, default_path_,
      base::DictionaryValue::From(base::JSONReader::Read(kTestManifest)));

  // The SwReporter should not be launched, and an error should be logged.
  EXPECT_TRUE(launched_invocations_.container().empty());
  histograms_.ExpectUniqueSample(kErrorHistogramName,
                                 SW_REPORTER_EXPERIMENT_ERROR_BAD_PARAMS, 1);
}

INSTANTIATE_TEST_CASE_P(
    All,
    ExperimentalSwReporterInstallerTest,
    ::testing::Values(
        SwReporterInvocationType::kPeriodicRun,
        SwReporterInvocationType::kUserInitiatedWithLogsDisallowed,
        SwReporterInvocationType::kUserInitiatedWithLogsAllowed));

}  // namespace component_updater
