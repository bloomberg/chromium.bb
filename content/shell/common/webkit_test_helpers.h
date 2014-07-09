// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_COMMON_WEBKIT_TEST_HELPERS_H_
#define CONTENT_SHELL_COMMON_WEBKIT_TEST_HELPERS_H_

#include <string>
#include <vector>

namespace base {
class FilePath;
}

namespace content {

struct TestPreferences;
struct WebPreferences;

// The TestRunner library keeps its settings in a WebPreferenes object.
// The content_shell, however, uses WebPreferences. This method exports the
// settings from the WebTestRunner library which are relevant for layout tests.
void ExportLayoutTestSpecificPreferences(const TestPreferences& from,
                                         WebPreferences* to);

// Applies settings that differ between layout tests and regular mode.
void ApplyLayoutTestDefaultPreferences(WebPreferences* prefs);

// Returns the root of the Blink checkout.
base::FilePath GetWebKitRootDirFilePath();

// Returns list of extra font files to be made accessible to the renderer.
std::vector<std::string> GetSideloadFontFiles();

}  // namespace content

#endif  // CONTENT_SHELL_COMMON_WEBKIT_TEST_HELPERS_H_
