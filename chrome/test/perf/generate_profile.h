// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_PERF_GENERATE_PROFILE_H_
#define CHROME_TEST_PERF_GENERATE_PROFILE_H_

#include "base/compiler_specific.h"

namespace base {
class FilePath;
}

// Addition types data can be generated for. By default only urls/visits are
// added.
enum GenerateProfileTypes {
  TOP_SITES = 1 << 0
};

// Generates a user profile and history by psuedo-randomly generating data and
// feeding it to the history service. (srand is initialized with whatever
// urlcount is before profile is generated for deterministic output; it is
// reset to time() afterwards.) Returns true if successful.
bool GenerateProfile(GenerateProfileTypes types,
                     int urlcount,
                     const base::FilePath& dst_dir) WARN_UNUSED_RESULT;

#endif  // CHROME_TEST_PERF_GENERATE_PROFILE_H_
