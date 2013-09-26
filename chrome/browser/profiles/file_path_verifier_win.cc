// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/file_path_verifier_win.h"

#include <windows.h>

#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/metrics/histogram.h"

namespace {

// This enum is used in UMA histograms and should never be re-ordered.
enum FileVerificationResult {
  FILE_VERIFICATION_SUCCESS,
  FILE_VERIFICATION_FILE_NOT_FOUND,
  FILE_VERIFICATION_INTERNAL_ERROR,
  FILE_VERIFICATION_FAILED_UNKNOWN,
  FILE_VERIFICATION_FAILED_SAMEBASE,
  FILE_VERIFICATION_FAILED_SAMEDIR,
  NUM_FILE_VERIFICATION_RESULTS
};

// Returns a FileVerificationResult based on the state of |file| on disk.
FileVerificationResult VerifyFileAtPath(const base::FilePath& file) {
  FileVerificationResult file_verification_result =
      FILE_VERIFICATION_FAILED_UNKNOWN;
  base::FilePath normalized_path;
  if (!file_util::NormalizeFilePath(file, &normalized_path)) {
    if (::GetLastError() == ERROR_FILE_NOT_FOUND)
      file_verification_result = FILE_VERIFICATION_FILE_NOT_FOUND;
    else
      file_verification_result = FILE_VERIFICATION_INTERNAL_ERROR;
  } else {
    internal::PathComparisonReason path_comparison_reason =
        internal::ComparePathsIgnoreCase(file, normalized_path);
    switch (path_comparison_reason) {
      case internal::PATH_COMPARISON_EQUAL:
        file_verification_result = FILE_VERIFICATION_SUCCESS;
        break;
      case internal::PATH_COMPARISON_FAILED_SAMEBASE:
        file_verification_result = FILE_VERIFICATION_FAILED_SAMEBASE;
        break;
      case internal::PATH_COMPARISON_FAILED_SAMEDIR:
        file_verification_result = FILE_VERIFICATION_FAILED_SAMEDIR;
        break;
    }
  }
  return file_verification_result;
}

}  // namespace

namespace internal {

PathComparisonReason ComparePathsIgnoreCase(const base::FilePath& path1,
                                            const base::FilePath& path2) {
  PathComparisonReason reason = PATH_COMPARISON_FAILED_UNKNOWN;

  if (base::FilePath::CompareEqualIgnoreCase(path1.value(), path2.value())) {
    reason = PATH_COMPARISON_EQUAL;
  } else if (base::FilePath::CompareEqualIgnoreCase(path1.BaseName().value(),
                                                    path2.BaseName().value())) {
    reason = PATH_COMPARISON_FAILED_SAMEBASE;
  } else if (base::FilePath::CompareEqualIgnoreCase(path1.DirName().value(),
                                                    path2.DirName().value())) {
    reason = PATH_COMPARISON_FAILED_SAMEDIR;
  }

  return reason;
}

}  // namespace internal

void VerifyPreferencesFile(const base::FilePath& pref_file_path) {
  FileVerificationResult file_verification_result =
      VerifyFileAtPath(pref_file_path);
  UMA_HISTOGRAM_ENUMERATION("Stability.FileAtPath.Preferences",
                            file_verification_result,
                            NUM_FILE_VERIFICATION_RESULTS);
}
