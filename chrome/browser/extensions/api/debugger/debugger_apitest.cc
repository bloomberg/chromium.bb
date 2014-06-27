// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/command_line.h"
#include "base/memory/ref_counted.h"
#include "base/path_service.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/extensions/api/debugger/debugger_api.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_function_test_utils.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/ui_test_utils.h"
#include "extensions/browser/extension_function.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/switches.h"
#include "extensions/common/value_builder.h"

namespace extensions {

class DebuggerApiTest : public ExtensionApiTest {
 protected:
  virtual ~DebuggerApiTest() {}

  virtual void SetUpCommandLine(base::CommandLine* command_line) OVERRIDE;
  virtual void SetUpOnMainThread() OVERRIDE;

  // Run the attach function. If |expected_error| is not empty, then the
  // function should fail with the error. Otherwise, the function is expected
  // to succeed.
  testing::AssertionResult RunAttachFunction(const GURL& url,
                                             const std::string& expected_error);

  const Extension* extension() const { return extension_; }
  base::CommandLine* command_line() const { return command_line_; }

 private:
  // The command-line for the test process, preserved in order to modify
  // mid-test.
  base::CommandLine* command_line_;

  // A basic extension with the debugger permission.
  scoped_refptr<const Extension> extension_;
};

void DebuggerApiTest::SetUpCommandLine(base::CommandLine* command_line) {
  ExtensionApiTest::SetUpCommandLine(command_line);
  // We need to hold onto |command_line| in order to modify it during the test.
  command_line_ = command_line;
}

void DebuggerApiTest::SetUpOnMainThread() {
  ExtensionApiTest::SetUpOnMainThread();
  extension_ =
      ExtensionBuilder().SetManifest(
          DictionaryBuilder().Set("name", "debugger")
                             .Set("version", "0.1")
                             .Set("manifest_version", 2)
                             .Set("permissions",
                                  ListBuilder().Append("debugger"))).Build();
}

testing::AssertionResult DebuggerApiTest::RunAttachFunction(
    const GURL& url, const std::string& expected_error) {
  ui_test_utils::NavigateToURL(browser(), url);
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  int tab_id = SessionID::IdForTab(web_contents);
  scoped_refptr<DebuggerAttachFunction> attach_function =
      new DebuggerAttachFunction();
  attach_function->set_extension(extension_);
  std::string args = base::StringPrintf("[{\"tabId\": %d}, \"1.1\"]", tab_id);

  if (!expected_error.empty()) {
    std::string actual_error =
        extension_function_test_utils::RunFunctionAndReturnError(
            attach_function, args, browser());
    if (actual_error != expected_error) {
      return testing::AssertionFailure() << "Did not get correct error: "
          << "expected: " << expected_error << ", found: " << actual_error;
    }
  } else {
    if (!RunFunction(attach_function,
                     args,
                     browser(),
                     extension_function_test_utils::NONE)) {
      return testing::AssertionFailure() << "Could not run function: "
          << attach_function->GetError();
    }

    // Clean up and detach.
    scoped_refptr<DebuggerDetachFunction> detach_function =
        new DebuggerDetachFunction();
    detach_function->set_extension(extension_);
    if (!RunFunction(detach_function,
                     base::StringPrintf("[{\"tabId\": %d}]", tab_id),
                     browser(),
                     extension_function_test_utils::NONE)) {
      return testing::AssertionFailure() << "Could not detach: "
          << detach_function->GetError();
    }
  }
  return testing::AssertionSuccess();
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, Debugger) {
  ASSERT_TRUE(RunExtensionTest("debugger")) << message_;
}

IN_PROC_BROWSER_TEST_F(DebuggerApiTest,
                       DebuggerNotAllowedOnOtherExtensionPages) {
  // Load another arbitrary extension with an associated resource (popup.html).
  base::FilePath path;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &path));
  path = path.AppendASCII("extensions").AppendASCII("good_unpacked");
  const Extension* another_extension = LoadExtension(path);
  ASSERT_TRUE(another_extension);

  GURL other_ext_url =
      GURL(base::StringPrintf("chrome-extension://%s/popup.html",
                              another_extension->id().c_str()));

  // This extension should not be able to access another extension.
  EXPECT_TRUE(RunAttachFunction(
      other_ext_url, manifest_errors::kCannotAccessExtensionUrl));

  // This extension *should* be able to debug itself.
  EXPECT_TRUE(RunAttachFunction(
                  GURL(base::StringPrintf("chrome-extension://%s/foo.html",
                                          extension()->id().c_str())),
                  std::string()));

  // Append extensions on chrome urls switch. The extension should now be able
  // to debug any extension.
  command_line()->AppendSwitch(switches::kExtensionsOnChromeURLs);
  EXPECT_TRUE(RunAttachFunction(other_ext_url, std::string()));
}

}  // namespace extensions
