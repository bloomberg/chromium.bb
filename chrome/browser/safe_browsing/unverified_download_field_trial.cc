// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/unverified_download_field_trial.h"

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/metrics/field_trial.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/safe_browsing/download_protection_util.h"
#include "components/variations/variations_associated_data.h"

namespace safe_browsing {

const char kUnverifiedDownloadFieldTrialName[] =
    "SafeBrowsingUnverifiedDownloads";

const char kUnverifiedDownloadFieldTrialDisableByParameter[] =
    "DisableByParameter";

const char kUnverifiedDownloadFieldTrialWhitelistParam[] = "whitelist";
const char kUnverifiedDownloadFieldTrialBlacklistParam[] = "blacklist";
const char kUnverifiedDownloadFieldTrialBlockSBTypesParam[] = "block_sb_types";

namespace {

bool MatchesExtensionList(const base::FilePath& needle,
                          const std::string& haystack) {
#if defined(OS_WIN)
  const base::FilePath::StringType comma_separated_extensions =
      base::UTF8ToUTF16(haystack);
#else
  const base::FilePath::StringType& comma_separated_extensions = haystack;
#endif
  std::vector<base::FilePath::StringPieceType> extension_list =
      base::SplitStringPiece(comma_separated_extensions, FILE_PATH_LITERAL(","),
                             base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

  // A single '*' matches everything.
  if (extension_list.size() == 1 && extension_list[0] == FILE_PATH_LITERAL("*"))
    return true;

  for (const auto& extension : extension_list) {
    // This shouldn't happen, but check anyway in case the parameter is
    // accidentally malformed. The underlying FilePath implementation expects
    // the extension to begin with an extension separator.
    if (extension.size() == 0 ||
        extension[0] != base::FilePath::kExtensionSeparator)
      continue;
    if (needle.MatchesExtension(extension))
      return true;
  }
  return false;
}

}  // namespace

bool IsUnverifiedDownloadAllowedByFieldTrial(const base::FilePath& file) {
  std::string group_name =
      base::FieldTrialList::FindFullName(kUnverifiedDownloadFieldTrialName);

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kAllowUncheckedDangerousDownloads))
    return true;

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisallowUncheckedDangerousDownloads) &&
      download_protection_util::IsSupportedBinaryFile(file))
    return false;

  if (base::StartsWith(group_name,
                       kUnverifiedDownloadFieldTrialDisableByParameter,
                       base::CompareCase::SENSITIVE)) {
    std::map<std::string, std::string> parameters;
    variations::GetVariationParams(kUnverifiedDownloadFieldTrialName,
                                   &parameters);

    if (parameters.count(kUnverifiedDownloadFieldTrialBlacklistParam) &&
        MatchesExtensionList(
            file, parameters[kUnverifiedDownloadFieldTrialBlacklistParam]))
      return false;

    if (parameters.count(kUnverifiedDownloadFieldTrialWhitelistParam) &&
        MatchesExtensionList(
            file, parameters[kUnverifiedDownloadFieldTrialWhitelistParam]))
      return true;

    if (parameters.count(kUnverifiedDownloadFieldTrialBlockSBTypesParam) &&
        !parameters[kUnverifiedDownloadFieldTrialBlockSBTypesParam].empty() &&
        download_protection_util::IsSupportedBinaryFile(file))
      return false;
  }

  return true;
}

}  // namespace safe_browsing
