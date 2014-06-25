// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_CONTENT_BROWSER_TEST_UTILS_H_
#define CONTENT_PUBLIC_TEST_CONTENT_BROWSER_TEST_UTILS_H_

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "ui/gfx/native_widget_types.h"
#include "url/gurl.h"

namespace base {
class FilePath;
}

namespace gfx {
class Rect;
}

// A collections of functions designed for use with content_shell based browser
// tests.
// Note: if a function here also works with browser_tests, it should be in
// content\public\test\browser_test_utils.h

namespace content {

class MessageLoopRunner;
class RenderViewCreatedObserver;
class Shell;
class WebContents;

// Generate the file path for testing a particular test.
// The file for the tests is all located in
// content/test/data/dir/<file>
// The returned path is FilePath format.
base::FilePath GetTestFilePath(const char* dir, const char* file);

// Generate the URL for testing a particular test.
// HTML for the tests is all located in
// test_root_directory/dir/<file>
// The returned path is GURL format.
GURL GetTestUrl(const char* dir, const char* file);

// Navigates the selected tab of |window| to |url|, blocking until the
// navigation finishes.
void NavigateToURL(Shell* window, const GURL& url);
void LoadDataWithBaseURL(Shell* window,
                         const GURL& url,
                         const std::string data,
                         const GURL& base_url);

// Navigates the selected tab of |window| to |url|, blocking until the given
// number of navigations finishes.
void NavigateToURLBlockUntilNavigationsComplete(Shell* window,
                                                const GURL& url,
                                                int number_of_navigations);
// Reloads the selected tab of |window|, blocking until the given number of
// navigations finishes.
void ReloadBlockUntilNavigationsComplete(Shell* window,
                                         int number_of_navigations);

// Wait until an application modal dialog is requested.
void WaitForAppModalDialog(Shell* window);

// Used to wait for a new Shell window to be created. Instantiate this object
// before the operation that will create the window.
class ShellAddedObserver {
 public:
  ShellAddedObserver();
  ~ShellAddedObserver();

  // Will run a message loop to wait for the new window if it hasn't been
  // created since the constructor.
  Shell* GetShell();

 private:
  void ShellCreated(Shell* shell);

  Shell* shell_;
  scoped_refptr<MessageLoopRunner> runner_;

  DISALLOW_COPY_AND_ASSIGN(ShellAddedObserver);
};

// Used to wait for a new WebContents to be created. Instantiate this object
// before the operation that will create the window.
class WebContentsAddedObserver {
 public:
  WebContentsAddedObserver();
  ~WebContentsAddedObserver();

  // Will run a message loop to wait for the new window if it hasn't been
  // created since the constructor
  WebContents* GetWebContents();

  // Will tell whether RenderViewCreated Callback has invoked
  bool RenderViewCreatedCalled();

  base::Callback<void(WebContents*)> web_contents_created_callback_;

 private:
  void WebContentsCreated(WebContents* web_contents);

  // Callback invoked on WebContents creation.
  WebContents* web_contents_;
  scoped_ptr<RenderViewCreatedObserver> child_observer_;
  scoped_refptr<MessageLoopRunner> runner_;

  DISALLOW_COPY_AND_ASSIGN(WebContentsAddedObserver);
};

#if defined OS_MACOSX
void SetWindowBounds(gfx::NativeWindow window, const gfx::Rect& bounds);
#endif

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_CONTENT_BROWSER_TEST_UTILS_H_
