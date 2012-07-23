// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_CONTENT_BROWSER_TEST_UTILS_H_
#define CONTENT_TEST_CONTENT_BROWSER_TEST_UTILS_H_

#include "googleurl/src/gurl.h"
#include "ui/gfx/native_widget_types.h"

class FilePath;

namespace gfx {
class Rect;
}

// A collections of functions designed for use with content_browsertests.
// Note: if a function here also works with browser_tests, it should be in
// content\public\test\browser_test_utils.h

namespace content {

class Shell;

// Generate the file path for testing a particular test.
// The file for the tests is all located in
// content/test/data/dir/<file>
// The returned path is FilePath format.
FilePath GetTestFilePath(const char* dir, const char* file);

// Generate the URL for testing a particular test.
// HTML for the tests is all located in
// test_root_directory/dir/<file>
// The returned path is GURL format.
GURL GetTestUrl(const char* dir, const char* file);

// Navigates the selected tab of |window| to |url|, blocking until the
// navigation finishes.
void NavigateToURL(Shell* window, const GURL& url);

// Wait until an application modal dialog is requested.
void WaitForAppModalDialog(Shell* window);

#if defined OS_MACOSX
void SetWindowBounds(gfx::NativeWindow window, const gfx::Rect& bounds);
#endif

}  // namespace content

#endif  // CONTENT_TEST_CONTENT_BROWSER_TEST_UTILS_H_
