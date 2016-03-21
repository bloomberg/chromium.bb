// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_BASE_MASH_BROWSER_TESTS_MAIN_H_
#define CHROME_TEST_BASE_MASH_BROWSER_TESTS_MAIN_H_

// Return true if mash browser tests were run, false otherwise. If the tests
// were run |exit_code| is set appropriately.
bool RunMashBrowserTests(int argc, char** argv, int* exit_code);

#endif  // CHROME_TEST_BASE_MASH_BROWSER_TESTS_MAIN_H_
