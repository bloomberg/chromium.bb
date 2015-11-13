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
  EXPECT_TRUE(download_protection_util::IsSupportedBinaryFile(
      base::FilePath(kHandledFilename)));
  EXPECT_FALSE(download_protection_util::IsSupportedBinaryFile(
      base::FilePath(kSafeFilename)));
}

// Verify that disallow-unchecked-dangerous-downloads command line switch causes
// all dangerous file types to be blocked, and that safe files types are still
// allowed.
TEST(UnverifiedDownloadFieldTrialTest, CommandLine_DisallowDangerous) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kDisallowUncheckedDangerousDownloads);
  EXPECT_TRUE(IsUnverifiedDownloadAllowedByFieldTrial(
      base::FilePath(kSafeFilename)));
  EXPECT_FALSE(IsUnverifiedDownloadAllowedByFieldTrial(
      base::FilePath(kHandledFilename)));
}

// Verify that a wildcard blacklist matches all file types.
TEST(UnverifiedDownloadFieldTrialTest, WildCardBlacklist) {
  FieldTrialParameters parameters;
  parameters[kUnverifiedDownloadFieldTrialBlacklistParam] = "*";
  parameters[kUnverifiedDownloadFieldTrialWhitelistParam] = ".xyz";
  ScopedFieldTrialState field_trial(
      kUnverifiedDownloadFieldTrialDisableByParameter, parameters);

  EXPECT_FALSE(IsUnverifiedDownloadAllowedByFieldTrial(
      base::FilePath(kSafeFilename)));
  EXPECT_FALSE(IsUnverifiedDownloadAllowedByFieldTrial(
      base::FilePath(kHandledFilename)));
  EXPECT_FALSE(IsUnverifiedDownloadAllowedByFieldTrial(
      base::FilePath(FILE_PATH_LITERAL("foo.xyz"))));
}

// Verify that allow-unchecked-dangerous-downloads command line option takes
// precedence over a Finch trial specified blacklist.
TEST(UnverifiedDownloadFieldTrialTest, BlacklistVsCommandline) {
  FieldTrialParameters parameters;
  parameters[kUnverifiedDownloadFieldTrialBlacklistParam] = "*";
  parameters[kUnverifiedDownloadFieldTrialWhitelistParam] = ".xyz";
  ScopedFieldTrialState field_trial(
      kUnverifiedDownloadFieldTrialDisableByParameter, parameters);
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kAllowUncheckedDangerousDownloads);

  EXPECT_TRUE(IsUnverifiedDownloadAllowedByFieldTrial(
      base::FilePath(kSafeFilename)));
  EXPECT_TRUE(IsUnverifiedDownloadAllowedByFieldTrial(
      base::FilePath(kHandledFilename)));
}

// Verify that disallow-unchecked-dangerous-downloads command line option takes
// precedence over a Finch trial specified whitelist.
TEST(UnverifiedDownloadFieldTrialTest, WhitelistVsCommandline) {
  FieldTrialParameters parameters;
  parameters[kUnverifiedDownloadFieldTrialBlacklistParam] = ".foo";
  parameters[kUnverifiedDownloadFieldTrialWhitelistParam] = ".exe";
  ScopedFieldTrialState field_trial(
      kUnverifiedDownloadFieldTrialDisableByParameter, parameters);
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kDisallowUncheckedDangerousDownloads);

  EXPECT_FALSE(IsUnverifiedDownloadAllowedByFieldTrial(
      base::FilePath(FILE_PATH_LITERAL("foo.foo"))));
  EXPECT_FALSE(IsUnverifiedDownloadAllowedByFieldTrial(
      base::FilePath(FILE_PATH_LITERAL("foo.exe"))));
  EXPECT_TRUE(IsUnverifiedDownloadAllowedByFieldTrial(
      base::FilePath(FILE_PATH_LITERAL("foo.txt"))));
}

// Verify that wildcards only work if they are specified by themselves.
TEST(UnverifiedDownloadFieldTrialTest, WildcardOnlyByItself) {
  FieldTrialParameters parameters;
  parameters[kUnverifiedDownloadFieldTrialBlacklistParam] = ".foo,*";
  parameters[kUnverifiedDownloadFieldTrialWhitelistParam] = ".xyz";
  ScopedFieldTrialState field_trial(
      kUnverifiedDownloadFieldTrialDisableByParameter, parameters);

  EXPECT_FALSE(IsUnverifiedDownloadAllowedByFieldTrial(
      base::FilePath(FILE_PATH_LITERAL("foo.foo"))));
  EXPECT_TRUE(IsUnverifiedDownloadAllowedByFieldTrial(
      base::FilePath(FILE_PATH_LITERAL("foo.xyz"))));
  EXPECT_TRUE(IsUnverifiedDownloadAllowedByFieldTrial(
      base::FilePath(FILE_PATH_LITERAL("foo.txt"))));
}

// Verify that the blacklist takes precedence over whitelist.
TEST(UnverifiedDownloadFieldTrialTest, WhitelistVsBlacklist) {
  FieldTrialParameters parameters;
  parameters[kUnverifiedDownloadFieldTrialBlacklistParam] = ".abc,.def";
  parameters[kUnverifiedDownloadFieldTrialWhitelistParam] = ".xyz";
  ScopedFieldTrialState field_trial(
      kUnverifiedDownloadFieldTrialDisableByParameter, parameters);

  EXPECT_TRUE(IsUnverifiedDownloadAllowedByFieldTrial(
      base::FilePath(kSafeFilename)));
  EXPECT_TRUE(IsUnverifiedDownloadAllowedByFieldTrial(
      base::FilePath(kHandledFilename)));
  EXPECT_TRUE(IsUnverifiedDownloadAllowedByFieldTrial(
      base::FilePath(FILE_PATH_LITERAL("foo.xyz"))));
  EXPECT_FALSE(IsUnverifiedDownloadAllowedByFieldTrial(
      base::FilePath(FILE_PATH_LITERAL("foo.abc"))));
  EXPECT_FALSE(IsUnverifiedDownloadAllowedByFieldTrial(
      base::FilePath(FILE_PATH_LITERAL("foo.def"))));
}

// Verify that nothing terrible happens if no parameters are specified.
TEST(UnverifiedDownloadFieldTrialTest, MissingParameters) {
  ScopedFieldTrialState field_trial(
      kUnverifiedDownloadFieldTrialDisableByParameter);

  EXPECT_TRUE(IsUnverifiedDownloadAllowedByFieldTrial(
      base::FilePath(kSafeFilename)));
  EXPECT_TRUE(IsUnverifiedDownloadAllowedByFieldTrial(
      base::FilePath(kHandledFilename)));
}

// Verify that nothing terrible happens if the parameters set for a field trial
// are malformed.
TEST(UnverifiedDownloadFieldTrialTest, MalformedParameters) {
  FieldTrialParameters parameters;
  parameters[kUnverifiedDownloadFieldTrialBlacklistParam] = "abcasdfa#??# ~def";
  parameters[kUnverifiedDownloadFieldTrialWhitelistParam] =
      "Robert'); DROP TABLE FinchTrials;--";
  ScopedFieldTrialState field_trial(
      kUnverifiedDownloadFieldTrialDisableByParameter, parameters);

  EXPECT_TRUE(IsUnverifiedDownloadAllowedByFieldTrial(
      base::FilePath(kSafeFilename)));
  EXPECT_TRUE(IsUnverifiedDownloadAllowedByFieldTrial(
      base::FilePath(kHandledFilename)));
}

// Verify that nothing terrible happens if the parameters set for a field trial
// are empty.
TEST(UnverifiedDownloadFieldTrialTest, DisableByParam_Empty) {
  FieldTrialParameters parameters;
  parameters[kUnverifiedDownloadFieldTrialBlacklistParam] = "";
  parameters[kUnverifiedDownloadFieldTrialWhitelistParam] = "";
  parameters[kUnverifiedDownloadFieldTrialBlockSBTypesParam] = "";
  ScopedFieldTrialState field_trial(
      kUnverifiedDownloadFieldTrialDisableByParameter, parameters);

  EXPECT_TRUE(IsUnverifiedDownloadAllowedByFieldTrial(
      base::FilePath(kSafeFilename)));
  EXPECT_TRUE(IsUnverifiedDownloadAllowedByFieldTrial(
      base::FilePath(kHandledFilename)));
}

// Verified that file types specified via white/blacklists are case insensitive.
TEST(UnverifiedDownloadFieldTrialTest, CaseInsensitive) {
  FieldTrialParameters parameters;
  parameters[kUnverifiedDownloadFieldTrialBlacklistParam] = ".ABC,.xyz";
  ScopedFieldTrialState field_trial(
      kUnverifiedDownloadFieldTrialDisableByParameter, parameters);

  EXPECT_FALSE(IsUnverifiedDownloadAllowedByFieldTrial(
      base::FilePath(FILE_PATH_LITERAL("foo.abc"))));
  EXPECT_FALSE(IsUnverifiedDownloadAllowedByFieldTrial(
      base::FilePath(FILE_PATH_LITERAL("FOO.ABC"))));
  EXPECT_FALSE(IsUnverifiedDownloadAllowedByFieldTrial(
      base::FilePath(FILE_PATH_LITERAL("FOO.XYZ"))));
  EXPECT_TRUE(IsUnverifiedDownloadAllowedByFieldTrial(
      base::FilePath(FILE_PATH_LITERAL("FOO.txt"))));
}

// Verify functionality when all parameters are specified.
TEST(UnverifiedDownloadFieldTrialTest, WhitelistVsBlacklistVsSBTypes) {
  FieldTrialParameters parameters;
  parameters[kUnverifiedDownloadFieldTrialBlacklistParam] = ".abc,.def";
  parameters[kUnverifiedDownloadFieldTrialWhitelistParam] = ".xyz";
  parameters[kUnverifiedDownloadFieldTrialBlockSBTypesParam] = "*";
  ScopedFieldTrialState field_trial(
      kUnverifiedDownloadFieldTrialDisableByParameter, parameters);

  EXPECT_TRUE(IsUnverifiedDownloadAllowedByFieldTrial(
      base::FilePath(kSafeFilename)));
  EXPECT_FALSE(IsUnverifiedDownloadAllowedByFieldTrial(
      base::FilePath(kHandledFilename)));
  EXPECT_TRUE(IsUnverifiedDownloadAllowedByFieldTrial(
      base::FilePath(FILE_PATH_LITERAL("foo.xyz"))));
  EXPECT_FALSE(IsUnverifiedDownloadAllowedByFieldTrial(
      base::FilePath(FILE_PATH_LITERAL("foo.abc"))));
  EXPECT_FALSE(IsUnverifiedDownloadAllowedByFieldTrial(
      base::FilePath(FILE_PATH_LITERAL("foo.def"))));
}

// Verify that block_sb_types parameter being empty is equivalent to it not
// being specified.
TEST(UnverifiedDownloadFieldTrialTest, DisableSBTypesEmpty) {
  FieldTrialParameters parameters;
  parameters[kUnverifiedDownloadFieldTrialBlockSBTypesParam] = "";
  ScopedFieldTrialState field_trial(
      kUnverifiedDownloadFieldTrialDisableByParameter, parameters);

  EXPECT_TRUE(IsUnverifiedDownloadAllowedByFieldTrial(
      base::FilePath(kSafeFilename)));
  EXPECT_TRUE(IsUnverifiedDownloadAllowedByFieldTrial(
      base::FilePath(kHandledFilename)));
}

// Verify that a whitelist is able to override the SafeBrowsing file type list.
TEST(UnverifiedDownloadFieldTrialTest, ListsOverrideSBTypes) {
  FieldTrialParameters parameters;
  parameters[kUnverifiedDownloadFieldTrialBlacklistParam] = ".abc,.def";
  parameters[kUnverifiedDownloadFieldTrialWhitelistParam] = ".exe";
  parameters[kUnverifiedDownloadFieldTrialBlockSBTypesParam] = "*";
  ScopedFieldTrialState field_trial(
      kUnverifiedDownloadFieldTrialDisableByParameter, parameters);

  EXPECT_TRUE(IsUnverifiedDownloadAllowedByFieldTrial(
      base::FilePath(kSafeFilename)));
  EXPECT_TRUE(IsUnverifiedDownloadAllowedByFieldTrial(
      base::FilePath(kHandledFilename)));
  EXPECT_FALSE(IsUnverifiedDownloadAllowedByFieldTrial(
      base::FilePath(FILE_PATH_LITERAL("foo.abc"))));
}

// Verify that the field trial is only sensitive to the prefix of the group
// name.
TEST(UnverifiedDownloadFieldTrialTest, FieldTrialGroupPrefix) {
  FieldTrialParameters parameters;
  parameters[kUnverifiedDownloadFieldTrialBlacklistParam] = ".abc,.def";
  parameters[kUnverifiedDownloadFieldTrialBlockSBTypesParam] = "*";
  ScopedFieldTrialState field_trial(
      std::string(kUnverifiedDownloadFieldTrialDisableByParameter) + "FooBar",
      parameters);

  EXPECT_TRUE(IsUnverifiedDownloadAllowedByFieldTrial(
      base::FilePath(kSafeFilename)));
  EXPECT_FALSE(IsUnverifiedDownloadAllowedByFieldTrial(
      base::FilePath(kHandledFilename)));
  EXPECT_FALSE(IsUnverifiedDownloadAllowedByFieldTrial(
      base::FilePath(FILE_PATH_LITERAL("foo.abc"))));
}

}  // namespace safe_browsing
