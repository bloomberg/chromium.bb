// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/extensions/active_script_controller.h"
#include "chrome/browser/extensions/extension_action.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/extensions/extension_test_message_listener.h"
#include "chrome/browser/extensions/location_bar_controller.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/extensions/test_extension_dir.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/common/feature_switch.h"
#include "extensions/common/switches.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace {

const char kAllHostsScheme[] = "*://*/*";
const char kExplicitHostsScheme[] = "http://127.0.0.1/*";
const char kBackgroundScript[] =
    "\"background\": {\"scripts\": [\"script.js\"]}";
const char kBackgroundScriptSource[] =
    "var listener = function(tabId) {\n"
    "  chrome.tabs.onUpdated.removeListener(listener);\n"
    "  chrome.tabs.executeScript(tabId, {\n"
    "    code: \"chrome.test.sendMessage('inject succeeded');\"\n"
    "  });"
    "};\n"
    "chrome.tabs.onUpdated.addListener(listener);";
const char kContentScriptSource[] =
    "chrome.test.sendMessage('inject succeeded');";

const char kInjectSucceeded[] = "inject succeeded";

enum InjectionType {
  CONTENT_SCRIPT,
  EXECUTE_SCRIPT
};

enum HostType {
  ALL_HOSTS,
  EXPLICIT_HOSTS
};

enum RequiresConsent {
  REQUIRES_CONSENT,
  DOES_NOT_REQUIRE_CONSENT
};

// Runs all pending tasks in the renderer associated with |web_contents|.
// Returns true on success.
bool RunAllPendingInRenderer(content::WebContents* web_contents) {
  // This is slight hack to achieve a RunPendingInRenderer() method. Since IPCs
  // are sent synchronously, anything started prior to this method will finish
  // before this method returns (as content::ExecuteScript() is synchronous).
  return content::ExecuteScript(web_contents, "1 == 1;");
}

}  // namespace

class ActiveScriptControllerBrowserTest : public ExtensionBrowserTest {
 public:
  ActiveScriptControllerBrowserTest() {}

  virtual void SetUpCommandLine(base::CommandLine* command_line) OVERRIDE;
  virtual void CleanUpOnMainThread() OVERRIDE;

  // Returns an extension with the given |host_type| and |injection_type|. If
  // one already exists, the existing extension will be returned. Othewrwise,
  // one will be created.
  // This could potentially return NULL if LoadExtension() fails.
  const Extension* CreateExtension(HostType host_type,
                                   InjectionType injection_type);

 private:
  ScopedVector<TestExtensionDir> test_extension_dirs_;
  std::vector<const Extension*> extensions_;
};

void ActiveScriptControllerBrowserTest::SetUpCommandLine(
    base::CommandLine* command_line) {
  ExtensionBrowserTest::SetUpCommandLine(command_line);
  // We append the actual switch to the commandline because it needs to be
  // passed over to the renderer, which a FeatureSwitch::ScopedOverride will
  // not do.
  command_line->AppendSwitch(switches::kEnableScriptsRequireAction);
}

void ActiveScriptControllerBrowserTest::CleanUpOnMainThread() {
  test_extension_dirs_.clear();
}

const Extension* ActiveScriptControllerBrowserTest::CreateExtension(
    HostType host_type, InjectionType injection_type) {
  std::string name =
      base::StringPrintf(
          "%s %s",
          injection_type == CONTENT_SCRIPT ?
              "content_script" : "execute_script",
          host_type == ALL_HOSTS ? "all_hosts" : "explicit_hosts");

  const char* permission_scheme =
      host_type == ALL_HOSTS ? kAllHostsScheme : kExplicitHostsScheme;

  std::string permissions = base::StringPrintf(
      "\"permissions\": [\"tabs\", \"%s\"]", permission_scheme);

  std::string scripts;
  std::string script_source;
  if (injection_type == CONTENT_SCRIPT) {
    scripts = base::StringPrintf(
        "\"content_scripts\": ["
        " {"
        "  \"matches\": [\"%s\"],"
        "  \"js\": [\"script.js\"],"
        "  \"run_at\": \"document_start\""
        " }"
        "]",
        permission_scheme);
  } else {
    scripts = kBackgroundScript;
  }

  std::string manifest = base::StringPrintf(
      "{"
      " \"name\": \"%s\","
      " \"version\": \"1.0\","
      " \"manifest_version\": 2,"
      " %s,"
      " %s"
      "}",
      name.c_str(),
      permissions.c_str(),
      scripts.c_str());

  scoped_ptr<TestExtensionDir> dir(new TestExtensionDir);
  dir->WriteManifest(manifest);
  dir->WriteFile(FILE_PATH_LITERAL("script.js"),
                 injection_type == CONTENT_SCRIPT ? kContentScriptSource :
                                                    kBackgroundScriptSource);

  const Extension* extension = LoadExtension(dir->unpacked_path());
  if (extension) {
    test_extension_dirs_.push_back(dir.release());
    extensions_.push_back(extension);
  }

  // If extension is NULL here, it will be caught later in the test.
  return extension;
}

class ActiveScriptTester {
 public:
  ActiveScriptTester(const std::string& name,
                     const Extension* extension,
                     Browser* browser,
                     RequiresConsent requires_consent,
                     InjectionType type);
  ~ActiveScriptTester();

  testing::AssertionResult Verify();

 private:
  // Returns the location bar controller, or NULL if one does not exist.
  LocationBarController* GetLocationBarController();

  // Returns the active script controller, or NULL if one does not exist.
  ActiveScriptController* GetActiveScriptController();

  // Get the ExtensionAction for this extension, or NULL if one does not exist.
  ExtensionAction* GetAction();

  // The name of the extension, and also the message it sends.
  std::string name_;

  // The extension associated with this tester.
  const Extension* extension_;

  // The browser the tester is running in.
  Browser* browser_;

  // Whether or not the extension has permission to run the script without
  // asking the user.
  RequiresConsent requires_consent_;

  // The type of injection this tester uses.
  InjectionType type_;

  // All of these extensions should inject a script (either through content
  // scripts or through chrome.tabs.executeScript()) that sends a message with
  // the |kInjectSucceeded| message.
  linked_ptr<ExtensionTestMessageListener> inject_success_listener_;
};

ActiveScriptTester::ActiveScriptTester(const std::string& name,
                                       const Extension* extension,
                                       Browser* browser,
                                       RequiresConsent requires_consent,
                                       InjectionType type)
    : name_(name),
      extension_(extension),
      browser_(browser),
      requires_consent_(requires_consent),
      type_(type),
      inject_success_listener_(
          new ExtensionTestMessageListener(kInjectSucceeded,
                                           false /* won't reply */)) {
  inject_success_listener_->set_extension_id(extension->id());
}

ActiveScriptTester::~ActiveScriptTester() {
}

testing::AssertionResult ActiveScriptTester::Verify() {
  if (!extension_)
    return testing::AssertionFailure() << "Could not load extension: " << name_;

  content::WebContents* web_contents =
      browser_ ? browser_->tab_strip_model()->GetActiveWebContents() : NULL;
  if (!web_contents)
    return testing::AssertionFailure() << "No web contents.";

  // Give the extension plenty of time to inject.
  if (!RunAllPendingInRenderer(web_contents))
    return testing::AssertionFailure() << "Could not run pending in renderer.";

  // Make sure all running tasks are complete.
  content::RunAllPendingInMessageLoop();

  LocationBarController* location_bar_controller = GetLocationBarController();
  if (!location_bar_controller) {
    return testing::AssertionFailure()
        << "Could not find location bar controller";
  }

  ActiveScriptController* controller =
      location_bar_controller->active_script_controller();
  if (!controller)
    return testing::AssertionFailure() << "Could not find controller.";

  ExtensionAction* action = GetAction();
  bool has_action = action != NULL;

  // An extension should have an action displayed iff it requires user consent.
  if ((requires_consent_ == REQUIRES_CONSENT && !has_action) ||
      (requires_consent_ == DOES_NOT_REQUIRE_CONSENT && has_action)) {
    return testing::AssertionFailure()
        << "Improper action status for " << name_ << ": expected "
        << (requires_consent_ == REQUIRES_CONSENT) << ", found " << has_action;
  }

  // If the extension has permission, we should be able to simply wait for it
  // to execute.
  if (requires_consent_ == DOES_NOT_REQUIRE_CONSENT) {
    inject_success_listener_->WaitUntilSatisfied();
    return testing::AssertionSuccess();
  }

  // Otherwise, we don't have permission, and have to grant it. Ensure the
  // script has *not* already executed.
  if (inject_success_listener_->was_satisfied()) {
    return testing::AssertionFailure() <<
        name_ << "'s script ran without permission.";
  }

  // If we reach this point, we should always have an action.
  DCHECK(action);

  // Grant permission by clicking on the extension action.
  location_bar_controller->OnClicked(action);

  // Now, the extension should be able to inject the script.
  inject_success_listener_->WaitUntilSatisfied();

  // The Action should have disappeared.
  has_action = GetAction() != NULL;
  if (has_action) {
    return testing::AssertionFailure()
        << "Extension " << name_ << " has lingering action.";
  }

  return testing::AssertionSuccess();
}

LocationBarController* ActiveScriptTester::GetLocationBarController() {
  content::WebContents* web_contents =
      browser_ ? browser_->tab_strip_model()->GetActiveWebContents() : NULL;

  if (!web_contents)
    return NULL;

  TabHelper* tab_helper = TabHelper::FromWebContents(web_contents);
  return tab_helper ? tab_helper->location_bar_controller() : NULL;
}

ActiveScriptController* ActiveScriptTester::GetActiveScriptController() {
  LocationBarController* location_bar_controller = GetLocationBarController();
  return location_bar_controller ?
      location_bar_controller->active_script_controller() : NULL;
}

ExtensionAction* ActiveScriptTester::GetAction() {
  ActiveScriptController* controller = GetActiveScriptController();
  return controller ? controller->GetActionForExtension(extension_) : NULL;
}

IN_PROC_BROWSER_TEST_F(ActiveScriptControllerBrowserTest,
                       ActiveScriptsAreDisplayedAndDelayExecution) {
  base::FilePath active_script_path =
      test_data_dir_.AppendASCII("active_script");

  const char* kExtensionNames[] = {
      "inject_scripts_all_hosts",
      "inject_scripts_explicit_hosts",
      "content_scripts_all_hosts",
      "content_scripts_explicit_hosts"
  };

  // First, we load up three extensions:
  // - An extension that injects scripts into all hosts,
  // - An extension that injects scripts into explicit hosts,
  // - An extension with a content script that runs on all hosts,
  // - An extension with a content script that runs on explicit hosts.
  // The extensions that operate on explicit hosts have permission; the ones
  // that request all hosts require user consent.
  ActiveScriptTester testers[] = {
      ActiveScriptTester(
          kExtensionNames[0],
          CreateExtension(ALL_HOSTS, EXECUTE_SCRIPT),
          browser(),
          REQUIRES_CONSENT,
          EXECUTE_SCRIPT),
      ActiveScriptTester(
          kExtensionNames[1],
          CreateExtension(EXPLICIT_HOSTS, EXECUTE_SCRIPT),
          browser(),
          DOES_NOT_REQUIRE_CONSENT,
          EXECUTE_SCRIPT),
      ActiveScriptTester(
          kExtensionNames[2],
          CreateExtension(ALL_HOSTS, CONTENT_SCRIPT),
          browser(),
          REQUIRES_CONSENT,
          CONTENT_SCRIPT),
      ActiveScriptTester(
          kExtensionNames[3],
          CreateExtension(EXPLICIT_HOSTS, CONTENT_SCRIPT),
          browser(),
          DOES_NOT_REQUIRE_CONSENT,
          CONTENT_SCRIPT),
  };

  // Navigate to an URL (which matches the explicit host specified in the
  // extension content_scripts_explicit_hosts). All four extensions should
  // inject the script.
  ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/extensions/test_file.html"));

  for (size_t i = 0u; i < arraysize(testers); ++i)
    EXPECT_TRUE(testers[i].Verify()) << kExtensionNames[i];
}

// Test that removing an extension with pending injections a) removes the
// pending injections for that extension, and b) does not affect pending
// injections for other extensions.
IN_PROC_BROWSER_TEST_F(ActiveScriptControllerBrowserTest,
                       RemoveExtensionWithPendingInjections) {
  // Load up two extensions, each with content scripts.
  const Extension* extension1 = CreateExtension(ALL_HOSTS, CONTENT_SCRIPT);
  ASSERT_TRUE(extension1);
  const Extension* extension2 = CreateExtension(ALL_HOSTS, CONTENT_SCRIPT);
  ASSERT_TRUE(extension2);

  ASSERT_NE(extension1->id(), extension2->id());

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);
  ActiveScriptController* active_script_controller =
      ActiveScriptController::GetForWebContents(web_contents);
  ASSERT_TRUE(active_script_controller);

  ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/extensions/test_file.html"));

  // Both extensions should have pending requests.
  EXPECT_TRUE(active_script_controller->GetActionForExtension(extension1));
  EXPECT_TRUE(active_script_controller->GetActionForExtension(extension2));

  // Unload one of the extensions.
  UnloadExtension(extension2->id());

  EXPECT_TRUE(RunAllPendingInRenderer(web_contents));

  // We should have pending requests for extension1, but not the removed
  // extension2.
  EXPECT_TRUE(active_script_controller->GetActionForExtension(extension1));
  EXPECT_FALSE(active_script_controller->GetActionForExtension(extension2));

  // We should still be able to run the request for extension1.
  ExtensionTestMessageListener inject_success_listener(
      new ExtensionTestMessageListener(kInjectSucceeded,
                                       false /* won't reply */));
  inject_success_listener.set_extension_id(extension1->id());
  active_script_controller->OnClicked(extension1);
  inject_success_listener.WaitUntilSatisfied();
}

// A version of the test with the flag off, in order to test that everything
// still works as expected.
class FlagOffActiveScriptControllerBrowserTest
    : public ActiveScriptControllerBrowserTest {
 private:
  // Simply don't append the flag.
  virtual void SetUpCommandLine(base::CommandLine* command_line) OVERRIDE {
    ExtensionBrowserTest::SetUpCommandLine(command_line);
  }
};

IN_PROC_BROWSER_TEST_F(FlagOffActiveScriptControllerBrowserTest,
                       ScriptsExecuteWhenFlagAbsent) {
  const char* kExtensionNames[] = {
    "content_scripts_all_hosts",
    "inject_scripts_all_hosts",
  };
  ActiveScriptTester testers[] = {
    ActiveScriptTester(
          kExtensionNames[0],
          CreateExtension(ALL_HOSTS, CONTENT_SCRIPT),
          browser(),
          DOES_NOT_REQUIRE_CONSENT,
          CONTENT_SCRIPT),
      ActiveScriptTester(
          kExtensionNames[1],
          CreateExtension(ALL_HOSTS, EXECUTE_SCRIPT),
          browser(),
          DOES_NOT_REQUIRE_CONSENT,
          EXECUTE_SCRIPT),
  };

  ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/extensions/test_file.html"));

  for (size_t i = 0u; i < arraysize(testers); ++i)
    EXPECT_TRUE(testers[i].Verify()) << kExtensionNames[i];
}

}  // namespace extensions
