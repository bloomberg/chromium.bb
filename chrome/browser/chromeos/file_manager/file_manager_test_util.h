// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_FILE_MANAGER_FILE_MANAGER_TEST_UTIL_H_
#define CHROME_BROWSER_CHROMEOS_FILE_MANAGER_FILE_MANAGER_TEST_UTIL_H_

class Profile;

namespace file_manager {
namespace test {

// Load the default set of component extensions used on ChromeOS. This should be
// done in an override of InProcessBrowserTest::SetUpOnMainThread().
void AddDefaultComponentExtensionsOnMainThread(Profile* profile);

}  // namespace test
}  // namespace file_manager

#endif  // CHROME_BROWSER_CHROMEOS_FILE_MANAGER_FILE_MANAGER_TEST_UTIL_H_
