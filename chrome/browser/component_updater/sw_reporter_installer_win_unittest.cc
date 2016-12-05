// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/sw_reporter_installer_win.h"

#include <map>
#include <memory>
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
#include "chrome/browser/safe_browsing/srt_fetcher_win.h"
#include "components/variations/variations_params_manager.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace component_updater {

namespace {

constexpr char kExperimentGroupName[] = "my_test_engine_group";

constexpr char kEngineExperimentGroupSwitch[] = "engine-experiment-group";
constexpr char kRegistrySuffixSwitch[] = "registry-suffix";
constexpr char kSessionIdSwitch[] = "session-id";

constexpr char kErrorHistogramName[] = "SoftwareReporter.ExperimentErrors";
constexpr char kExperimentTag[] = "experiment_tag";
constexpr char kMissingTag[] = "missing_tag";

using safe_browsing::SwReporterInvocation;

}  // namespace

class SwReporterInstallerTest : public ::testing::Test {
 public:
  SwReporterInstallerTest()
      : launched_callback_(
            base::Bind(&SwReporterInstallerTest::SwReporterLaunched,
                       base::Unretained(this))),
        default_version_("1.2.3"),
        default_path_(L"C:\\full\\path\\to\\download"),
        launched_version_("0.0.0") {}

  ~SwReporterInstallerTest() override {}

 protected:
  void SwReporterLaunched(const safe_browsing::SwReporterQueue& invocations,
                          const base::Version& version) {
    ASSERT_TRUE(launched_invocations_.empty());
    launched_invocations_ = invocations;
    launched_version_ = version;
  }

  base::FilePath MakeTestFilePath(const base::FilePath& path) const {
    return path.Append(L"software_reporter_tool.exe");
  }

  void ExpectEmptyAttributes(const SwReporterInstallerTraits& traits) const {
    update_client::InstallerAttributes attributes =
        traits.GetInstallerAttributes();
    EXPECT_TRUE(attributes.empty());
  }

  // Expects that the SwReporter was launched exactly once, with a session-id
  // switch.
  void ExpectDefaultInvocation() const {
    EXPECT_EQ(default_version_, launched_version_);
    ASSERT_EQ(1U, launched_invocations_.size());

    const SwReporterInvocation& invocation = launched_invocations_.front();
    EXPECT_EQ(MakeTestFilePath(default_path_),
              invocation.command_line.GetProgram());
    EXPECT_EQ(1U, invocation.command_line.GetSwitches().size());
    EXPECT_EQ(
        40U,
        invocation.command_line.GetSwitchValueASCII(kSessionIdSwitch).size());
    EXPECT_TRUE(invocation.command_line.GetArgs().empty());
    EXPECT_TRUE(invocation.suffix.empty());
    EXPECT_EQ(SwReporterInvocation::BEHAVIOUR_LOG_TO_RAPPOR |
                  SwReporterInvocation::BEHAVIOUR_LOG_EXIT_CODE_TO_PREFS |
                  SwReporterInvocation::BEHAVIOUR_TRIGGER_PROMPT |
                  SwReporterInvocation::BEHAVIOUR_ALLOW_SEND_REPORTER_LOGS,
              invocation.supported_behaviours);
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
  safe_browsing::SwReporterQueue launched_invocations_;
  base::Version launched_version_;

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

  void ExpectAttributesWithTag(const SwReporterInstallerTraits& traits,
                               const std::string& tag) {
    update_client::InstallerAttributes attributes =
        traits.GetInstallerAttributes();
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
    EXPECT_EQ(default_version_, launched_version_);
    ASSERT_EQ(1U, launched_invocations_.size());

    const SwReporterInvocation& invocation = launched_invocations_.front();
    EXPECT_EQ(MakeTestFilePath(default_path_),
              invocation.command_line.GetProgram());
    EXPECT_EQ(
        40U,
        invocation.command_line.GetSwitchValueASCII(kSessionIdSwitch).size());
    EXPECT_EQ(kExperimentGroupName, invocation.command_line.GetSwitchValueASCII(
                                        kEngineExperimentGroupSwitch));

    if (expected_suffix.empty()) {
      EXPECT_EQ(2U, invocation.command_line.GetSwitches().size());
      EXPECT_TRUE(invocation.suffix.empty());
    } else {
      EXPECT_EQ(3U, invocation.command_line.GetSwitches().size());
      EXPECT_EQ(expected_suffix, invocation.command_line.GetSwitchValueASCII(
                                     kRegistrySuffixSwitch));
      EXPECT_EQ(expected_suffix, invocation.suffix);
    }

    if (expected_additional_argument.empty()) {
      EXPECT_TRUE(invocation.command_line.GetArgs().empty());
    } else {
      EXPECT_EQ(1U, invocation.command_line.GetArgs().size());
      EXPECT_EQ(expected_additional_argument,
                invocation.command_line.GetArgs()[0]);
    }

    EXPECT_EQ(0U, invocation.supported_behaviours);
    histograms_.ExpectTotalCount(kErrorHistogramName, 0);
  }

  // Expects that the SwReporter was launched with the given |expected_suffix|,
  // |expected_engine|, and |expected_behaviours|, as part of a series of
  // multiple invocations.
  void ExpectExperimentalInvocationInSeries(
      const std::string& expected_suffix,
      const std::string& expected_engine,
      SwReporterInvocation::Behaviours expected_behaviours,
      std::string* out_session_id) {
    SCOPED_TRACE("Invocation with suffix " + expected_suffix);
    SwReporterInvocation invocation = launched_invocations_.front();
    launched_invocations_.pop();
    EXPECT_EQ(MakeTestFilePath(default_path_),
              invocation.command_line.GetProgram());
    // There should be one switch added from the manifest, plus registry-suffix
    // added automatically.
    EXPECT_EQ(4U, invocation.command_line.GetSwitches().size());
    EXPECT_EQ(expected_engine,
              invocation.command_line.GetSwitchValueASCII("engine"));
    EXPECT_EQ(expected_suffix, invocation.command_line.GetSwitchValueASCII(
                                   kRegistrySuffixSwitch));
    *out_session_id =
        invocation.command_line.GetSwitchValueASCII(kSessionIdSwitch);
    EXPECT_EQ(40U, out_session_id->size());
    EXPECT_EQ(kExperimentGroupName, invocation.command_line.GetSwitchValueASCII(
                                        kEngineExperimentGroupSwitch));
    ASSERT_TRUE(invocation.command_line.GetArgs().empty());
    EXPECT_EQ(expected_suffix, invocation.suffix);
    EXPECT_EQ(expected_behaviours, invocation.supported_behaviours);
  }

  void ExpectLaunchError() {
    // The SwReporter should not be launched, and an error should be logged.
    EXPECT_TRUE(launched_invocations_.empty());
    histograms_.ExpectUniqueSample(kErrorHistogramName,
                                   SW_REPORTER_EXPERIMENT_ERROR_BAD_PARAMS, 1);
  }

  std::unique_ptr<variations::testing::VariationParamsManager> variations_;
  base::test::ScopedFeatureList scoped_feature_list_;
  base::HistogramTester histograms_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ExperimentalSwReporterInstallerTest);
};

TEST_F(SwReporterInstallerTest, Default) {
  SwReporterInstallerTraits traits(launched_callback_, false);
  ExpectEmptyAttributes(traits);
  traits.ComponentReady(default_version_, default_path_,
                        std::make_unique<base::DictionaryValue>());
  ExpectDefaultInvocation();
}

TEST_F(ExperimentalSwReporterInstallerTest, NoExperimentConfig) {
  // Even if the experiment is supported on this hardware, the user shouldn't
  // be enrolled unless enabled through variations.
  SwReporterInstallerTraits traits(launched_callback_, true);
  ExpectEmptyAttributes(traits);
  traits.ComponentReady(default_version_, default_path_,
                        std::make_unique<base::DictionaryValue>());
  ExpectDefaultInvocation();
}

TEST_F(ExperimentalSwReporterInstallerTest, ExperimentUnsupported) {
  // Even if the experiment config is enabled in variations, the user shouldn't
  // be enrolled if the hardware doesn't support it.
  SwReporterInstallerTraits traits(launched_callback_, false);
  CreateFeatureWithTag(kExperimentTag);
  ExpectEmptyAttributes(traits);
  traits.ComponentReady(default_version_, default_path_,
                        std::make_unique<base::DictionaryValue>());
  ExpectDefaultInvocation();
}

TEST_F(ExperimentalSwReporterInstallerTest, ExperimentMissingTag) {
  SwReporterInstallerTraits traits(launched_callback_, true);
  CreateFeatureWithoutTag();
  ExpectAttributesWithTag(traits, kMissingTag);
  histograms_.ExpectUniqueSample(kErrorHistogramName,
                                 SW_REPORTER_EXPERIMENT_ERROR_BAD_TAG, 1);
}

TEST_F(ExperimentalSwReporterInstallerTest, ExperimentInvalidTag) {
  SwReporterInstallerTraits traits(launched_callback_, true);
  CreateFeatureWithTag("tag with invalid whitespace chars");
  ExpectAttributesWithTag(traits, kMissingTag);
  histograms_.ExpectUniqueSample(kErrorHistogramName,
                                 SW_REPORTER_EXPERIMENT_ERROR_BAD_TAG, 1);
}

TEST_F(ExperimentalSwReporterInstallerTest, ExperimentTagTooLong) {
  SwReporterInstallerTraits traits(launched_callback_, true);
  std::string tag_too_long(500, 'x');
  CreateFeatureWithTag(tag_too_long);
  ExpectAttributesWithTag(traits, kMissingTag);
  histograms_.ExpectUniqueSample(kErrorHistogramName,
                                 SW_REPORTER_EXPERIMENT_ERROR_BAD_TAG, 1);
}

TEST_F(ExperimentalSwReporterInstallerTest, ExperimentEmptyTag) {
  SwReporterInstallerTraits traits(launched_callback_, true);
  CreateFeatureWithTag("");
  ExpectAttributesWithTag(traits, kMissingTag);
  histograms_.ExpectUniqueSample(kErrorHistogramName,
                                 SW_REPORTER_EXPERIMENT_ERROR_BAD_TAG, 1);
}

TEST_F(ExperimentalSwReporterInstallerTest, SingleInvocation) {
  SwReporterInstallerTraits traits(launched_callback_, true);
  CreateFeatureWithTag(kExperimentTag);
  ExpectAttributesWithTag(traits, kExperimentTag);

  static constexpr char kTestManifest[] =
      "{\"launch_params\": ["
      "  {"
      "    \"arguments\": [\"--engine=experimental\", \"random argument\"],"
      "    \"suffix\": \"TestSuffix\","
      "    \"prompt\": false"
      "  }"
      "]}";
  traits.ComponentReady(
      default_version_, default_path_,
      base::DictionaryValue::From(base::JSONReader::Read(kTestManifest)));

  // The SwReporter should be launched once with the given arguments.
  EXPECT_EQ(default_version_, launched_version_);
  ASSERT_EQ(1U, launched_invocations_.size());

  const SwReporterInvocation& invocation = launched_invocations_.front();
  EXPECT_EQ(MakeTestFilePath(default_path_),
            invocation.command_line.GetProgram());
  EXPECT_EQ(4U, invocation.command_line.GetSwitches().size());
  EXPECT_EQ("experimental",
            invocation.command_line.GetSwitchValueASCII("engine"));
  EXPECT_EQ("TestSuffix",
            invocation.command_line.GetSwitchValueASCII(kRegistrySuffixSwitch));
  EXPECT_EQ(
      40U,
      invocation.command_line.GetSwitchValueASCII(kSessionIdSwitch).size());
  EXPECT_EQ(kExperimentGroupName, invocation.command_line.GetSwitchValueASCII(
                                      kEngineExperimentGroupSwitch));
  ASSERT_EQ(1U, invocation.command_line.GetArgs().size());
  EXPECT_EQ(L"random argument", invocation.command_line.GetArgs()[0]);
  EXPECT_EQ("TestSuffix", invocation.suffix);
  EXPECT_EQ(0U, invocation.supported_behaviours);
  histograms_.ExpectTotalCount(kErrorHistogramName, 0);
}

TEST_F(ExperimentalSwReporterInstallerTest, MultipleInvocations) {
  SwReporterInstallerTraits traits(launched_callback_, true);
  CreateFeatureWithTag(kExperimentTag);
  ExpectAttributesWithTag(traits, kExperimentTag);

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
  traits.ComponentReady(
      default_version_, default_path_,
      base::DictionaryValue::From(base::JSONReader::Read(kTestManifest)));

  // The SwReporter should be launched four times with the given arguments.
  EXPECT_EQ(default_version_, launched_version_);
  ASSERT_EQ(4U, launched_invocations_.size());
  std::string out_session_id;
  ExpectExperimentalInvocationInSeries(
      "TestSuffix", "experimental",
      SwReporterInvocation::BEHAVIOUR_ALLOW_SEND_REPORTER_LOGS,
      &out_session_id);

  const std::string first_session_id(out_session_id);

  ExpectExperimentalInvocationInSeries(
      "SecondSuffix", "second", SwReporterInvocation::BEHAVIOUR_TRIGGER_PROMPT,
      &out_session_id);
  EXPECT_EQ(first_session_id, out_session_id);

  ExpectExperimentalInvocationInSeries("ThirdSuffix", "third", 0U,
                                       &out_session_id);
  EXPECT_EQ(first_session_id, out_session_id);

  ExpectExperimentalInvocationInSeries(
      "FourthSuffix", "fourth",
      SwReporterInvocation::BEHAVIOUR_ALLOW_SEND_REPORTER_LOGS |
          SwReporterInvocation::BEHAVIOUR_TRIGGER_PROMPT,
      &out_session_id);
  EXPECT_EQ(first_session_id, out_session_id);

  histograms_.ExpectTotalCount(kErrorHistogramName, 0);
}

TEST_F(ExperimentalSwReporterInstallerTest, MissingSuffix) {
  SwReporterInstallerTraits traits(launched_callback_, true);
  CreateFeatureWithTag(kExperimentTag);

  static constexpr char kTestManifest[] =
      "{\"launch_params\": ["
      "  {"
      "    \"arguments\": [\"random argument\"]"
      "  }"
      "]}";
  traits.ComponentReady(
      default_version_, default_path_,
      base::DictionaryValue::From(base::JSONReader::Read(kTestManifest)));

  ExpectLaunchError();
}

TEST_F(ExperimentalSwReporterInstallerTest, EmptySuffix) {
  SwReporterInstallerTraits traits(launched_callback_, true);
  CreateFeatureWithTag(kExperimentTag);

  static constexpr char kTestManifest[] =
      "{\"launch_params\": ["
      "  {"
      "    \"suffix\": \"\","
      "    \"arguments\": [\"random argument\"]"
      "  }"
      "]}";
  traits.ComponentReady(
      default_version_, default_path_,
      base::DictionaryValue::From(base::JSONReader::Read(kTestManifest)));

  ExpectExperimentalInvocation("", L"random argument");
}

TEST_F(ExperimentalSwReporterInstallerTest, MissingSuffixAndArgs) {
  SwReporterInstallerTraits traits(launched_callback_, true);
  CreateFeatureWithTag(kExperimentTag);

  static constexpr char kTestManifest[] =
      "{\"launch_params\": ["
      "  {"
      "  }"
      "]}";
  traits.ComponentReady(
      default_version_, default_path_,
      base::DictionaryValue::From(base::JSONReader::Read(kTestManifest)));

  ExpectLaunchError();
}

TEST_F(ExperimentalSwReporterInstallerTest, EmptySuffixAndArgs) {
  SwReporterInstallerTraits traits(launched_callback_, true);
  CreateFeatureWithTag(kExperimentTag);

  static constexpr char kTestManifest[] =
      "{\"launch_params\": ["
      "  {"
      "    \"suffix\": \"\","
      "    \"arguments\": []"
      "  }"
      "]}";
  traits.ComponentReady(
      default_version_, default_path_,
      base::DictionaryValue::From(base::JSONReader::Read(kTestManifest)));

  ExpectExperimentalInvocation("", L"");
}

TEST_F(ExperimentalSwReporterInstallerTest, EmptySuffixAndArgs2) {
  SwReporterInstallerTraits traits(launched_callback_, true);
  CreateFeatureWithTag(kExperimentTag);

  static constexpr char kTestManifest[] =
      "{\"launch_params\": ["
      "  {"
      "    \"suffix\": \"\","
      "    \"arguments\": [\"\"]"
      "  }"
      "]}";
  traits.ComponentReady(
      default_version_, default_path_,
      base::DictionaryValue::From(base::JSONReader::Read(kTestManifest)));

  ExpectExperimentalInvocation("", L"");
}

TEST_F(ExperimentalSwReporterInstallerTest, MissingArguments) {
  SwReporterInstallerTraits traits(launched_callback_, true);
  CreateFeatureWithTag(kExperimentTag);

  static constexpr char kTestManifest[] =
      "{\"launch_params\": ["
      "  {"
      "    \"suffix\": \"TestSuffix\""
      "  }"
      "]}";
  traits.ComponentReady(
      default_version_, default_path_,
      base::DictionaryValue::From(base::JSONReader::Read(kTestManifest)));

  ExpectLaunchError();
}

TEST_F(ExperimentalSwReporterInstallerTest, EmptyArguments) {
  SwReporterInstallerTraits traits(launched_callback_, true);
  CreateFeatureWithTag(kExperimentTag);

  static constexpr char kTestManifest[] =
      "{\"launch_params\": ["
      "  {"
      "    \"suffix\": \"TestSuffix\","
      "    \"arguments\": []"
      "  }"
      "]}";
  traits.ComponentReady(
      default_version_, default_path_,
      base::DictionaryValue::From(base::JSONReader::Read(kTestManifest)));

  ExpectExperimentalInvocation("TestSuffix", L"");
}

TEST_F(ExperimentalSwReporterInstallerTest, EmptyArguments2) {
  SwReporterInstallerTraits traits(launched_callback_, true);
  CreateFeatureWithTag(kExperimentTag);

  static constexpr char kTestManifest[] =
      "{\"launch_params\": ["
      "  {"
      "    \"suffix\": \"TestSuffix\","
      "    \"arguments\": [\"\"]"
      "  }"
      "]}";
  traits.ComponentReady(
      default_version_, default_path_,
      base::DictionaryValue::From(base::JSONReader::Read(kTestManifest)));

  ExpectExperimentalInvocation("TestSuffix", L"");
}

TEST_F(ExperimentalSwReporterInstallerTest, EmptyManifest) {
  SwReporterInstallerTraits traits(launched_callback_, true);
  CreateFeatureWithTag(kExperimentTag);

  static constexpr char kTestManifest[] = "{}";
  traits.ComponentReady(
      default_version_, default_path_,
      base::DictionaryValue::From(base::JSONReader::Read(kTestManifest)));

  // The SwReporter should not be launched, but no error should be logged.
  // (This tests the case where a non-experimental version of the reporter,
  // which does not have "launch_params" in its manifest, is already present.)
  EXPECT_TRUE(launched_invocations_.empty());
  histograms_.ExpectTotalCount(kErrorHistogramName, 0);
}

TEST_F(ExperimentalSwReporterInstallerTest, EmptyLaunchParams) {
  SwReporterInstallerTraits traits(launched_callback_, true);
  CreateFeatureWithTag(kExperimentTag);

  static constexpr char kTestManifest[] = "{\"launch_params\": []}";
  traits.ComponentReady(
      default_version_, default_path_,
      base::DictionaryValue::From(base::JSONReader::Read(kTestManifest)));

  // The SwReporter should not be launched, and an error should be logged.
  EXPECT_TRUE(launched_invocations_.empty());
  histograms_.ExpectUniqueSample(kErrorHistogramName,
                                 SW_REPORTER_EXPERIMENT_ERROR_BAD_PARAMS, 1);
}

TEST_F(ExperimentalSwReporterInstallerTest, EmptyLaunchParams2) {
  SwReporterInstallerTraits traits(launched_callback_, true);
  CreateFeatureWithTag(kExperimentTag);

  static constexpr char kTestManifest[] = "{\"launch_params\": {}}";
  traits.ComponentReady(
      default_version_, default_path_,
      base::DictionaryValue::From(base::JSONReader::Read(kTestManifest)));

  // The SwReporter should not be launched, and an error should be logged.
  EXPECT_TRUE(launched_invocations_.empty());
  histograms_.ExpectUniqueSample(kErrorHistogramName,
                                 SW_REPORTER_EXPERIMENT_ERROR_BAD_PARAMS, 1);
}

TEST_F(ExperimentalSwReporterInstallerTest, BadSuffix) {
  SwReporterInstallerTraits traits(launched_callback_, true);
  CreateFeatureWithTag(kExperimentTag);

  static constexpr char kTestManifest[] =
      "{\"launch_params\": ["
      "  {"
      "    \"arguments\": [\"--engine=experimental\"],"
      "    \"suffix\": \"invalid whitespace characters\""
      "  }"
      "]}";
  traits.ComponentReady(
      default_version_, default_path_,
      base::DictionaryValue::From(base::JSONReader::Read(kTestManifest)));

  // The SwReporter should not be launched, and an error should be logged.
  EXPECT_TRUE(launched_invocations_.empty());
  histograms_.ExpectUniqueSample(kErrorHistogramName,
                                 SW_REPORTER_EXPERIMENT_ERROR_BAD_PARAMS, 1);
}

TEST_F(ExperimentalSwReporterInstallerTest, SuffixTooLong) {
  SwReporterInstallerTraits traits(launched_callback_, true);
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
  traits.ComponentReady(
      default_version_, default_path_,
      base::DictionaryValue::From(base::JSONReader::Read(manifest)));

  // The SwReporter should not be launched, and an error should be logged.
  EXPECT_TRUE(launched_invocations_.empty());
  histograms_.ExpectUniqueSample(kErrorHistogramName,
                                 SW_REPORTER_EXPERIMENT_ERROR_BAD_PARAMS, 1);
}

TEST_F(ExperimentalSwReporterInstallerTest, BadTypesInManifest) {
  SwReporterInstallerTraits traits(launched_callback_, true);
  CreateFeatureWithTag(kExperimentTag);

  // This has a string instead of a list for "arguments".
  static constexpr char kTestManifest[] =
      "{\"launch_params\": ["
      "  {"
      "    \"arguments\": \"--engine=experimental\","
      "    \"suffix\": \"TestSuffix\""
      "  }"
      "]}";
  traits.ComponentReady(
      default_version_, default_path_,
      base::DictionaryValue::From(base::JSONReader::Read(kTestManifest)));

  // The SwReporter should not be launched, and an error should be logged.
  EXPECT_TRUE(launched_invocations_.empty());
  histograms_.ExpectUniqueSample(kErrorHistogramName,
                                 SW_REPORTER_EXPERIMENT_ERROR_BAD_PARAMS, 1);
}

TEST_F(ExperimentalSwReporterInstallerTest, BadTypesInManifest2) {
  SwReporterInstallerTraits traits(launched_callback_, true);
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
  traits.ComponentReady(
      default_version_, default_path_,
      base::DictionaryValue::From(base::JSONReader::Read(kTestManifest)));

  // The SwReporter should not be launched, and an error should be logged.
  EXPECT_TRUE(launched_invocations_.empty());
  histograms_.ExpectUniqueSample(kErrorHistogramName,
                                 SW_REPORTER_EXPERIMENT_ERROR_BAD_PARAMS, 1);
}

TEST_F(ExperimentalSwReporterInstallerTest, BadTypesInManifest3) {
  SwReporterInstallerTraits traits(launched_callback_, true);
  CreateFeatureWithTag(kExperimentTag);

  // This has a list for suffix as well as for arguments.
  static constexpr char kTestManifest[] =
      "{\"launch_params\": ["
      "  {"
      "    \"arguments\": [\"--engine=experimental\"],"
      "    \"suffix\": [\"TestSuffix\"]"
      "  }"
      "]}";
  traits.ComponentReady(
      default_version_, default_path_,
      base::DictionaryValue::From(base::JSONReader::Read(kTestManifest)));

  // The SwReporter should not be launched, and an error should be logged.
  EXPECT_TRUE(launched_invocations_.empty());
  histograms_.ExpectUniqueSample(kErrorHistogramName,
                                 SW_REPORTER_EXPERIMENT_ERROR_BAD_PARAMS, 1);
}

TEST_F(ExperimentalSwReporterInstallerTest, BadTypesInManifest4) {
  SwReporterInstallerTraits traits(launched_callback_, true);
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
  traits.ComponentReady(
      default_version_, default_path_,
      base::DictionaryValue::From(base::JSONReader::Read(kTestManifest)));

  // The SwReporter should not be launched, and an error should be logged.
  EXPECT_TRUE(launched_invocations_.empty());
  histograms_.ExpectUniqueSample(kErrorHistogramName,
                                 SW_REPORTER_EXPERIMENT_ERROR_BAD_PARAMS, 1);
}
}  // namespace component_updater
