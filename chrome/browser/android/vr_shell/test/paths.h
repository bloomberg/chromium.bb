// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_VR_SHELL_TEST_PATHS_H_
#define CHROME_BROWSER_ANDROID_VR_SHELL_TEST_PATHS_H_

namespace vr_shell {
namespace test {

enum {
  PATH_START = 12000,

  // Valid only in testing environments.
  DIR_TEST_DATA,
  PATH_END,
};

// Call once to register the provider for the path keys defined above.
void RegisterPathProvider();

}  // namespace test
}  // namespace vr_shell

#endif  // CHROME_BROWSER_ANDROID_VR_SHELL_TEST_PATHS_H_
