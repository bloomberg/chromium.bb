// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILES_FILE_PATH_VERIFIER_WIN_H_
#define CHROME_BROWSER_PROFILES_FILE_PATH_VERIFIER_WIN_H_

#include <string>

namespace base {
class FilePath;
}

namespace internal {

enum PathComparisonReason {
  PATH_COMPARISON_EQUAL,
  PATH_COMPARISON_FAILED_UNKNOWN,
  PATH_COMPARISON_FAILED_SAMEBASE,  // Not the same path, but same BaseName.
  PATH_COMPARISON_FAILED_SAMEDIR,   // Not the same path, but same DirName.
};

// Returns a PathComparisonReason based on the result of comparing |path1|
// and |path2| case-insensitively.
PathComparisonReason ComparePathsIgnoreCase(const base::FilePath& path1,
                                            const base::FilePath& path2);

}  // namespace internal

// Verifies that |file| is found as expected on disk and reports the result via
// a UMA stat with suffix |metric_suffix|.
void VerifyFileAtPath(const base::FilePath& file,
                      const std::string& metric_suffix);

#endif  // CHROME_BROWSER_PROFILES_FILE_PATH_VERIFIER_WIN_H_
