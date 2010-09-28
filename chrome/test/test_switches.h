// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_TEST_SWITCHES_H_
#define CHROME_TEST_TEST_SWITCHES_H_

namespace switches {

// All switches in alphabetical order. The switches should be documented
// alongside the definition of their values in the .cc file.
extern const char kExtraChromeFlags[];
extern const char kEnableErrorDialogs[];
extern const char kLiveOperationTimeout[];
extern const char kPageCyclerIterations[];
extern const char kTestLargeTimeout[];
extern const char kUiTestActionTimeout[];
extern const char kUiTestActionMaxTimeout[];
extern const char kUiTestCommandExecutionTimeout[];
extern const char kUiTestTerminateTimeout[];
extern const char kUiTestTimeout[];

}  // namespace switches

#endif  // CHROME_TEST_TEST_SWITCHES_H_
