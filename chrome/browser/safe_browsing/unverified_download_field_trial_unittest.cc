// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/unverified_download_field_trial.h"

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/metrics/field_trial.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/safe_browsing/download_protection_util.h"
#include "components/variations/variations_associated_data.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {

namespace {

const base::FilePath::CharType kSafeFilename[] = FILE_PATH_LITERAL("foo.txt");
const base::FilePath::CharType kHandledFilename[] =
    FILE_PATH_LITERAL("foo.exe");

using FieldTrialParameters = std::map<std::string, std::string>;

class ScopedFieldTrialState {
 public:
  ScopedFieldTrialState(const std::string& group_name,
                        const FieldTrialParameters& parameters)
      : field_trial_list_(nullptr) {
    variations::testing::ClearAllVariationIDs();
    variations::testing::ClearAllVariationParams();

    if (!parameters.empty()) {
      EXPECT_TRUE(variations::AssociateVariationParams(
          kUnverifiedDownloadFieldTrialName, group_name, parameters));
    }
    EXPECT_TRUE(base::FieldTrialList::CreateFieldTrial(
        kUnverifiedDownloadFieldTrialName, group_name));
  }

  explicit ScopedFieldTrialState(const std::string& group_name)
      : ScopedFieldTrialState(group_name, FieldTrialParameters()) {}

  ~ScopedFieldTrialState() {
    variations::testing::ClearAllVariationIDs();
    variations::testing::ClearAllVariationParams();
  }

 private:
  base::FieldTrialList field_trial_list_;
};

}  // namespace

// Verify some test assumptions. Namely, that kSafeFilename is not a supported
// binary file and that kHandledFilename is.
TEST(UnverifiedDownloadFieldTrialTest, Assumptions) {
  EXPECT_TRUE(safe_browsing::download_protection_util::IsSupportedBinaryFile(
      base::FilePath(kHandledFilename)));
  EXPECT_FALSE(safe_browsing::download_protection_util::IsSupportedBinaryFile(
      base::FilePath(kSafeFilename)));
}

TEST(UnverifiedDownloadFieldTrialTest, CommandLine_AllowAll) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kAllowUncheckedDangerousDownloads);
  EXPECT_TRUE(safe_browsing::IsUnverifiedDownloadAllowedByFieldTrial(
      base::FilePath(kSafeFilename)));
  EXPECT_TRUE(safe_browsing::IsUnverifiedDownloadAllowedByFieldTrial(
      base::FilePath(kHandledFilename)));
}

TEST(UnverifiedDownloadFieldTrialTest, CommandLine_DisallowDangerous) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kDisallowUncheckedDangerousDownloads);
  EXPECT_TRUE(safe_browsing::IsUnverifiedDownloadAllowedByFieldTrial(
      base::FilePath(kSafeFilename)));
  EXPECT_FALSE(safe_browsing::IsUnverifiedDownloadAllowedByFieldTrial(
      base::FilePath(kHandledFilename)));
}

TEST(UnverifiedDownloadFieldTrialTest, DisableAll) {
  ScopedFieldTrialState field_trial(kUnverifiedDownloadFieldTrialDisableAll);
  EXPECT_FALSE(safe_browsing::IsUnverifiedDownloadAllowedByFieldTrial(
      base::FilePath(kSafeFilename)));
  EXPECT_FALSE(safe_browsing::IsUnverifiedDownloadAllowedByFieldTrial(
      base::FilePath(kHandledFilename)));
}

TEST(UnverifiedDownloadFieldTrialTest, DisableByParam) {
  FieldTrialParameters parameters;
  parameters[kUnverifiedDownloadFieldTrialBlacklistParam] = ".abc,.def";
  parameters[kUnverifiedDownloadFieldTrialWhitelistParam] = ".xyz";
  ScopedFieldTrialState field_trial(
      kUnverifiedDownloadFieldTrialDisableByParameter, parameters);

  EXPECT_TRUE(safe_browsing::IsUnverifiedDownloadAllowedByFieldTrial(
      base::FilePath(kSafeFilename)));
  EXPECT_TRUE(safe_browsing::IsUnverifiedDownloadAllowedByFieldTrial(
      base::FilePath(kHandledFilename)));
  EXPECT_TRUE(safe_browsing::IsUnverifiedDownloadAllowedByFieldTrial(
      base::FilePath(FILE_PATH_LITERAL("foo.xyz"))));
  EXPECT_FALSE(safe_browsing::IsUnverifiedDownloadAllowedByFieldTrial(
      base::FilePath(FILE_PATH_LITERAL("foo.abc"))));
  EXPECT_FALSE(safe_browsing::IsUnverifiedDownloadAllowedByFieldTrial(
      base::FilePath(FILE_PATH_LITERAL("foo.def"))));
}

// Verify that nothing terrible happens if no parameters are specified for a
// group that expects parameters.
TEST(UnverifiedDownloadFieldTrialTest, DisableByParam_NoParameters) {
  ScopedFieldTrialState field_trial(
      kUnverifiedDownloadFieldTrialDisableByParameter);

  EXPECT_TRUE(safe_browsing::IsUnverifiedDownloadAllowedByFieldTrial(
      base::FilePath(kSafeFilename)));
  EXPECT_TRUE(safe_browsing::IsUnverifiedDownloadAllowedByFieldTrial(
      base::FilePath(kHandledFilename)));
}

// Verify that nothing terrible happens if the parameters set for a field trial
// are malformed.
TEST(UnverifiedDownloadFieldTrialTest, DisableByParam_BadFormat) {
  FieldTrialParameters parameters;
  parameters[kUnverifiedDownloadFieldTrialBlacklistParam] = "abcasdfa#??# ~def";
  parameters[kUnverifiedDownloadFieldTrialWhitelistParam] =
      "Robert'); DROP TABLE FinchTrials;--";
  ScopedFieldTrialState field_trial(
      kUnverifiedDownloadFieldTrialDisableByParameter, parameters);

  EXPECT_TRUE(safe_browsing::IsUnverifiedDownloadAllowedByFieldTrial(
      base::FilePath(kSafeFilename)));
  EXPECT_TRUE(safe_browsing::IsUnverifiedDownloadAllowedByFieldTrial(
      base::FilePath(kHandledFilename)));
}

// Verify that nothing terrible happens if the parameters set for a field trial
// are empty.
TEST(UnverifiedDownloadFieldTrialTest, DisableByParam_Empty) {
  FieldTrialParameters parameters;
  parameters[kUnverifiedDownloadFieldTrialBlacklistParam] = "";
  parameters[kUnverifiedDownloadFieldTrialWhitelistParam] = "";
  ScopedFieldTrialState field_trial(
      kUnverifiedDownloadFieldTrialDisableByParameter, parameters);

  EXPECT_TRUE(safe_browsing::IsUnverifiedDownloadAllowedByFieldTrial(
      base::FilePath(kSafeFilename)));
  EXPECT_TRUE(safe_browsing::IsUnverifiedDownloadAllowedByFieldTrial(
      base::FilePath(kHandledFilename)));
}

TEST(UnverifiedDownloadFieldTrialTest, DisableByParam_CaseSensitive) {
  FieldTrialParameters parameters;
  parameters[kUnverifiedDownloadFieldTrialBlacklistParam] = ".ABC,.xyz";
  parameters[kUnverifiedDownloadFieldTrialWhitelistParam] = "";
  ScopedFieldTrialState field_trial(
      kUnverifiedDownloadFieldTrialDisableByParameter, parameters);

  EXPECT_FALSE(safe_browsing::IsUnverifiedDownloadAllowedByFieldTrial(
      base::FilePath(FILE_PATH_LITERAL("foo.abc"))));
  EXPECT_FALSE(safe_browsing::IsUnverifiedDownloadAllowedByFieldTrial(
      base::FilePath(FILE_PATH_LITERAL("FOO.ABC"))));
  EXPECT_FALSE(safe_browsing::IsUnverifiedDownloadAllowedByFieldTrial(
      base::FilePath(FILE_PATH_LITERAL("FOO.XYZ"))));
  EXPECT_TRUE(safe_browsing::IsUnverifiedDownloadAllowedByFieldTrial(
      base::FilePath(FILE_PATH_LITERAL("FOO.txt"))));
}

TEST(UnverifiedDownloadFieldTrialTest, DisableSBTypesAndByParameter) {
  FieldTrialParameters parameters;
  parameters[kUnverifiedDownloadFieldTrialBlacklistParam] = ".abc,.def";
  parameters[kUnverifiedDownloadFieldTrialWhitelistParam] = ".xyz";
  ScopedFieldTrialState field_trial(
      kUnverifiedDownloadFieldTrialDisableSBTypesAndByParameter, parameters);

  EXPECT_TRUE(safe_browsing::IsUnverifiedDownloadAllowedByFieldTrial(
      base::FilePath(kSafeFilename)));
  EXPECT_FALSE(safe_browsing::IsUnverifiedDownloadAllowedByFieldTrial(
      base::FilePath(kHandledFilename)));
  EXPECT_TRUE(safe_browsing::IsUnverifiedDownloadAllowedByFieldTrial(
      base::FilePath(FILE_PATH_LITERAL("foo.xyz"))));
  EXPECT_FALSE(safe_browsing::IsUnverifiedDownloadAllowedByFieldTrial(
      base::FilePath(FILE_PATH_LITERAL("foo.abc"))));
  EXPECT_FALSE(safe_browsing::IsUnverifiedDownloadAllowedByFieldTrial(
      base::FilePath(FILE_PATH_LITERAL("foo.def"))));
}

// Verify that a whitelist is able to override the SafeBrowsing file type list.
TEST(UnverifiedDownloadFieldTrialTest,
     DisableSBTypesAndByParameter_OverrideSB) {
  FieldTrialParameters parameters;
  parameters[kUnverifiedDownloadFieldTrialBlacklistParam] = ".abc,.def";
  parameters[kUnverifiedDownloadFieldTrialWhitelistParam] = ".exe";
  ScopedFieldTrialState field_trial(
      kUnverifiedDownloadFieldTrialDisableSBTypesAndByParameter, parameters);

  EXPECT_TRUE(safe_browsing::IsUnverifiedDownloadAllowedByFieldTrial(
      base::FilePath(kSafeFilename)));
  EXPECT_TRUE(safe_browsing::IsUnverifiedDownloadAllowedByFieldTrial(
      base::FilePath(kHandledFilename)));
}

}  // namespace safe_browsing
