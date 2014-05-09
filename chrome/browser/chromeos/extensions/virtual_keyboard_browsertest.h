// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_VIRTUAL_KEYBOARD_BROWSERTEST_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_VIRTUAL_KEYBOARD_BROWSERTEST_H_

#include "base/files/file_path.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "url/gurl.h"

namespace base {
class CommandLine;
class FilePath;
}

namespace content {
class RenderViewHost;
class WebContents;
}

// See the .cc for default values.
struct VirtualKeyboardBrowserTestConfig {
  VirtualKeyboardBrowserTestConfig();

  ~VirtualKeyboardBrowserTestConfig();

  // The filename of the base framework. This file should be in |test_dir_|.
  std::string base_framework_;

  // The virtual keyboard's extension id.
  std::string extension_id_;

  // Path to the test directory.
  std::string test_dir_;

  // URL of the keyboard extension.
  std::string url_;
};

class VirtualKeyboardBrowserTest : public InProcessBrowserTest {
 public:
  // Injects javascript in |file| into the keyboard page and runs the methods in
  // |file| whose names match the expression "test*".
  void RunTest(const base::FilePath& file,
               const VirtualKeyboardBrowserTestConfig& config);

  void ShowVirtualKeyboard();

  // Returns the render view host that the keyboard with extension |id| is in.
  content::RenderViewHost* GetKeyboardRenderViewHost(const std::string& id);

  // InProcessBrowserTest.
  // Ensure that the virtual keyboard is enabled.
  virtual void SetUpCommandLine(base::CommandLine* command_line) OVERRIDE;

 protected:
  // Accumulates the javascript and injects it when the test starts. The test
  // |file| is in directory |dir| relative to the root testing directory.
  void InjectJavascript(const base::FilePath& dir, const base::FilePath& file);

 private:
  std::string utf8_content_;
};

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_VIRTUAL_KEYBOARD_BROWSERTEST_H_
