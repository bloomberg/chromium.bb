// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "chrome/browser/extensions/active_script_controller.h"
#include "chrome/browser/extensions/extension_action.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_test_message_listener.h"
#include "chrome/browser/extensions/location_bar_controller.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/ui_test_utils.h"
#include "extensions/common/feature_switch.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

class ActiveScriptControllerBrowserTest : public ExtensionBrowserTest {
 public:
  ActiveScriptControllerBrowserTest()
      : feature_override_(FeatureSwitch::scripts_require_action(),
                          FeatureSwitch::OVERRIDE_ENABLED) {}
 private:
  FeatureSwitch::ScopedOverride feature_override_;
};

class ActiveScriptTester {
 public:
  ActiveScriptTester(const std::string& name,
                     const Extension* extension,
                     bool expect_has_action);
  ~ActiveScriptTester();

  testing::AssertionResult Verify(Browser* browser);

 private:
  // The name of the extension, and also the message it sends.
  std::string name_;

  // The extension associated with this tester.
  const Extension* extension_;

  // Whether or not we expect the extension to have an action for script
  // injection.
  bool expect_has_action_;

  // All of these extensions should inject a script (either through content
  // scripts or through chrome.tabs.executeScript()) that sends a message with
  // their names (for verification).
  // Be prepared to wait for each extension to inject the script.
  linked_ptr<ExtensionTestMessageListener> listener_;
};

ActiveScriptTester::ActiveScriptTester(const std::string& name,
                                       const Extension* extension,
                                       bool expect_has_action)
    : name_(name),
      extension_(extension),
      expect_has_action_(expect_has_action),
      listener_(
          new ExtensionTestMessageListener(name, false /* won't reply */)) {
}

ActiveScriptTester::~ActiveScriptTester() {
}

testing::AssertionResult ActiveScriptTester::Verify(Browser* browser) {
  if (!extension_)
    return testing::AssertionFailure() << "Could not load extension: " << name_;

  listener_->WaitUntilSatisfied();
  content::WebContents* web_contents =
      browser ? browser->tab_strip_model()->GetActiveWebContents() : NULL;

  if (!web_contents)
    return testing::AssertionFailure() << "Could not find web contents.";

  TabHelper* tab_helper = TabHelper::FromWebContents(web_contents);
  if (!tab_helper)
    return testing::AssertionFailure() << "Could not find tab helper.";

  ActiveScriptController* controller =
      tab_helper->location_bar_controller()->active_script_controller();
  if (!controller)
    return testing::AssertionFailure() << "Could not find controller.";

  bool has_action = controller->GetActionForExtension(extension_) != NULL;
  if (expect_has_action_ != has_action) {
    return testing::AssertionFailure()
        << "Improper action status: expected " << expect_has_action_
        << ", found " << has_action;
  }

  return testing::AssertionSuccess();
}

IN_PROC_BROWSER_TEST_F(ActiveScriptControllerBrowserTest,
                       ActiveScriptsAreDisplayed) {
  base::FilePath active_script_path =
      test_data_dir_.AppendASCII("active_script");

  const char* kExtensionNames[] = {
      "content_scripts_all_hosts",
      "inject_scripts_all_hosts",
      "content_scripts_explicit_hosts"
  };

  // First, we load up three extensions:
  // - An extension with a content script that runs on all hosts,
  // - An extension that injects scripts into all hosts,
  // - An extension with a content script that runs on explicit hosts.
  ActiveScriptTester testers[] = {
      ActiveScriptTester(
          kExtensionNames[0],
          LoadExtension(active_script_path.AppendASCII(kExtensionNames[0])),
          true /* expect action */),
      ActiveScriptTester(
          kExtensionNames[1],
          LoadExtension(active_script_path.AppendASCII(kExtensionNames[1])),
          true /* expect action */),
      ActiveScriptTester(
          kExtensionNames[2],
          LoadExtension(active_script_path.AppendASCII(kExtensionNames[2])),
          false /* expect action */)
  };

  // Navigate to an URL (which matches the explicit host specified in the
  // extension content_scripts_explicit_hosts). All three extensions should
  // inject the script.
  ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/extensions/test_file.html"));

  for (size_t i = 0u; i < arraysize(testers); ++i)
    ASSERT_TRUE(testers[i].Verify(browser())) << kExtensionNames[i];
}

}  // namespace extensions
