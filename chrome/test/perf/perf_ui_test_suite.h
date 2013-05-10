// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_PERF_PERF_UI_TEST_SUITE_H_
#define CHROME_TEST_PERF_PERF_UI_TEST_SUITE_H_

#include "base/files/scoped_temp_dir.h"
#include "chrome/test/ui/ui_test_suite.h"

namespace base {
class FilePath;
}

// UITestSuite which creates two testing profiles at Initialize() time. We
// create fake profiles so we don't commit 10-20 megabytes of binary data to
// the repository each time we change the history format (or even worse, don't
// update the test profiles and have incorrect performance data!)
class PerfUITestSuite : public UITestSuite {
 public:
  PerfUITestSuite(int argc, char** argv);
  virtual ~PerfUITestSuite();

  // Profile theme type choices.
  enum ProfileType {
    DEFAULT_THEME = 0,
    COMPLEX_THEME = 1,
  };

  // Returns the directory name where the "typical" user data is that we use
  // for testing.
  static base::FilePath GetPathForProfileType(ProfileType profile_type);

  // Overridden from UITestSuite:
  virtual void Initialize() OVERRIDE;

 private:
  // Builds a "Cached Theme.pak" file in |extension_base|.
  void BuildCachedThemePakIn(const base::FilePath& extension_base);

  base::ScopedTempDir default_profile_dir_;
  base::ScopedTempDir complex_profile_dir_;
};

#endif  // CHROME_TEST_PERF_PERF_UI_TEST_SUITE_H_
