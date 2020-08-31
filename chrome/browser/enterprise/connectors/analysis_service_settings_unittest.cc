// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/analysis_service_settings.h"
#include "base/json/json_reader.h"
#include "base/no_destructor.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace enterprise_connectors {

namespace {

struct TestParam {
  TestParam(const char* url,
            const char* settings_value,
            AnalysisSettings* expected_settings)
      : url(url),
        settings_value(settings_value),
        expected_settings(expected_settings) {}

  const char* url;
  const char* settings_value;
  AnalysisSettings* expected_settings;
};

constexpr char kNormalSettings[] = R"({
  "service_provider": "Google",
  "enable": [
    {"url_list": ["*"], "tags": ["dlp", "malware"]},
  ],
  "disable": [
    {"url_list": ["no.dlp.com", "no.dlp.or.malware.ca"], "tags": ["dlp"]},
    {"url_list": ["no.malware.com", "no.dlp.or.malware.ca"],
         "tags": ["malware"]},
    {"url_list": ["scan2.com"], "tags": ["dlp", "malware"]},
  ],
  "block_until_verdict": 1,
  "block_password_protected": true,
  "block_large_files": true,
  "block_unsupported_file_types": true,
})";

constexpr char kOnlyEnabledPatternsSettings[] = R"({
  "service_provider": "Google",
  "enable": [
    {"url_list": ["scan1.com", "scan2.com"], "tags": ["enabled"]},
  ],
})";

constexpr char kNoProviderSettings[] = R"({
  "enable": [
    {"url_list": ["*"], "tags": ["dlp", "malware"]},
  ],
  "disable": [
    {"url_list": ["no.dlp.com", "no.dlp.or.malware.ca"], "tags": ["dlp"]},
    {"url_list": ["no.malware.com", "no.dlp.or.malware.ca"],
         "tags": ["malware"]},
    {"url_list": ["scan2.com"], "tags": ["dlp", "malware"]},
  ],
  "block_until_verdict": 1,
  "block_password_protected": true,
  "block_large_files": true,
  "block_unsupported_file_types": true,
})";

constexpr char kNoEnabledPatternsSettings[] = R"({
  "service_provider": "Google",
  "disable": [
    {"url_list": ["no.dlp.com", "no.dlp.or.malware.ca"], "tags": ["dlp"]},
    {"url_list": ["no.malware.com", "no.dlp.or.malware.ca"],
         "tags": ["malware"]},
    {"url_list": ["scan2.com"], "tags": ["dlp", "malware"]},
  ],
  "block_until_verdict": 1,
  "block_password_protected": true,
  "block_large_files": true,
  "block_unsupported_file_types": true,
})";

constexpr char kScan1DotCom[] = "https://scan1.com";
constexpr char kScan2DotCom[] = "https://scan2.com";
constexpr char kNoDlpDotCom[] = "https://no.dlp.com";
constexpr char kNoMalwareDotCom[] = "https://no.malware.com";
constexpr char kNoDlpOrMalwareDotCa[] = "https://no.dlp.or.malware.ca";

AnalysisSettings* OnlyEnabledSettings() {
  static base::NoDestructor<AnalysisSettings> settings([]() {
    AnalysisSettings settings;
    settings.tags = {"enabled"};
    return settings;
  }());
  return settings.get();
}

AnalysisSettings NormalSettingsWithTags(std::set<std::string> tags) {
  AnalysisSettings settings;
  settings.tags = tags;
  settings.block_until_verdict = BlockUntilVerdict::BLOCK;
  settings.block_password_protected_files = true;
  settings.block_large_files = true;
  settings.block_unsupported_file_types = true;
  return settings;
}

AnalysisSettings* NormalDlpSettings() {
  static base::NoDestructor<AnalysisSettings> settings(
      NormalSettingsWithTags({"dlp"}));
  return settings.get();
}

AnalysisSettings* NormalMalwareSettings() {
  static base::NoDestructor<AnalysisSettings> settings(
      NormalSettingsWithTags({"malware"}));
  return settings.get();
}

AnalysisSettings* NormalDlpAndMalwareSettings() {
  static base::NoDestructor<AnalysisSettings> settings(
      NormalSettingsWithTags({"dlp", "malware"}));
  return settings.get();
}

AnalysisSettings* NoSettings() {
  return nullptr;
}

}  // namespace

class AnalysisServiceSettingsTest : public testing::TestWithParam<TestParam> {
 public:
  GURL url() const { return GURL(GetParam().url); }
  const char* settings_value() const { return GetParam().settings_value; }
  AnalysisSettings* expected_settings() const {
    return GetParam().expected_settings;
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
};

TEST_P(AnalysisServiceSettingsTest, Test) {
  // auto value = base::Value::AsDictionaryValue(base::Value(pref_value()));
  auto settings = base::JSONReader::Read(settings_value(),
                                         base::JSON_ALLOW_TRAILING_COMMAS);
  ASSERT_TRUE(settings.has_value());

  AnalysisServiceSettings service_settings(settings.value());

  auto analysis_settings = service_settings.GetAnalysisSettings(url());
  ASSERT_EQ((expected_settings() != nullptr), analysis_settings.has_value());
  if (analysis_settings.has_value()) {
    ASSERT_EQ(analysis_settings.value().tags, expected_settings()->tags);
    ASSERT_EQ(analysis_settings.value().block_until_verdict,
              expected_settings()->block_until_verdict);
    ASSERT_EQ(analysis_settings.value().block_password_protected_files,
              expected_settings()->block_password_protected_files);
    ASSERT_EQ(analysis_settings.value().block_large_files,
              expected_settings()->block_large_files);
    ASSERT_EQ(analysis_settings.value().block_unsupported_file_types,
              expected_settings()->block_unsupported_file_types);
  }
}

INSTANTIATE_TEST_CASE_P(
    ,
    AnalysisServiceSettingsTest,
    testing::Values(
        // Validate that the enabled patterns match the expected patterns.
        TestParam(kScan1DotCom,
                  kOnlyEnabledPatternsSettings,
                  OnlyEnabledSettings()),
        TestParam(kScan2DotCom,
                  kOnlyEnabledPatternsSettings,
                  OnlyEnabledSettings()),
        TestParam(kNoDlpDotCom, kOnlyEnabledPatternsSettings, NoSettings()),
        TestParam(kNoMalwareDotCom, kOnlyEnabledPatternsSettings, NoSettings()),
        TestParam(kNoDlpOrMalwareDotCa,
                  kOnlyEnabledPatternsSettings,
                  NoSettings()),

        // Validate that each URL gets the correct tag on the normal settings.
        TestParam(kScan1DotCom, kNormalSettings, NormalDlpAndMalwareSettings()),
        TestParam(kScan2DotCom, kNormalSettings, NoSettings()),
        TestParam(kNoDlpDotCom, kNormalSettings, NormalMalwareSettings()),
        TestParam(kNoMalwareDotCom, kNormalSettings, NormalDlpSettings()),
        TestParam(kNoDlpOrMalwareDotCa, kNormalSettings, NoSettings()),

        // Validate that each URL gets no settings when either the provider is
        // absent or when there are no enabled patterns.
        TestParam(kScan1DotCom, kNoProviderSettings, NoSettings()),
        TestParam(kScan2DotCom, kNoProviderSettings, NoSettings()),
        TestParam(kNoDlpDotCom, kNoProviderSettings, NoSettings()),
        TestParam(kNoMalwareDotCom, kNoProviderSettings, NoSettings()),
        TestParam(kNoDlpOrMalwareDotCa, kNoProviderSettings, NoSettings()),

        TestParam(kScan1DotCom, kNoEnabledPatternsSettings, NoSettings()),
        TestParam(kScan2DotCom, kNoEnabledPatternsSettings, NoSettings()),
        TestParam(kNoDlpDotCom, kNoEnabledPatternsSettings, NoSettings()),
        TestParam(kNoMalwareDotCom, kNoEnabledPatternsSettings, NoSettings()),
        TestParam(kNoDlpOrMalwareDotCa,
                  kNoEnabledPatternsSettings,
                  NoSettings())));

}  // namespace enterprise_connectors
