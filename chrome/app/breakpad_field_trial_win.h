// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_APP_BREAKPAD_FIELD_TRIAL_WIN_H_
#define CHROME_APP_BREAKPAD_FIELD_TRIAL_WIN_H_

#include <vector>

#include "base/strings/string16.h"

namespace testing {

// Sets the breakpad experiment chunks for testing. |chunks| is the list of
// values to set, where each entry may contain multiple experiment tuples, with
// the total number of experiments indicated by |experiments_chunks|.
void SetExperimentChunks(const std::vector<string16>& chunks,
                         size_t experiments_count);

}  // namespace testing

#endif  // CHROME_APP_BREAKPAD_FIELD_TRIAL_WIN_H_
