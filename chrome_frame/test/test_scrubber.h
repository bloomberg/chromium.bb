// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The test scrubber cleans up machine state between tests.  This includes
// killing all Internet Explorer processes, killing all Chrome processes spawned
// by Chrome Frame, and deleting the user data directory.  The scrubber must be
// installed before tests are run via |InstallTestScrubber|.
//
// Tests that lead to Chrome using a user data directory other than that used by
// Chrome Frame in IE must provide that directory to the scrubber via
// |OverrideDataDirectoryForThisTest()|.  Failure to do so will lead to the
// scrubber failing to delete that directory.

#ifndef CHROME_FRAME_TEST_TEST_SCRUBBER_H_
#define CHROME_FRAME_TEST_TEST_SCRUBBER_H_

#include "base/strings/string_piece.h"

namespace testing {
class UnitTest;
}

namespace chrome_frame_test {

// Install the scrubber in |unit_test| (call before RUN_ALL_TESTS).
void InstallTestScrubber(testing::UnitTest* unit_test);

// Override the user data directory that will be deleted by the scrubber at the
// completion of the current test.
void OverrideDataDirectoryForThisTest(const base::StringPiece16& user_data_dir);

}  // namespace chrome_frame_test

#endif  // CHROME_FRAME_TEST_TEST_SCRUBBER_H_
