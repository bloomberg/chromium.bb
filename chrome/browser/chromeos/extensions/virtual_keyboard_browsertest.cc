/*
 * Copyright 2013 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#include "chrome/browser/chromeos/extensions/virtual_keyboard_browsertest.h"

#include <vector>

#include "ash/shell.h"
#include "base/command_line.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host_iterator.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/common/extension.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/base/ime/input_method.h"
#include "ui/keyboard/keyboard_controller.h"
#include "ui/keyboard/keyboard_switches.h"

namespace {
const base::FilePath::CharType kWebuiTestDir[] = FILE_PATH_LITERAL("webui");

const base::FilePath::CharType kMockController[] =
    FILE_PATH_LITERAL("mock_controller.js");

const base::FilePath::CharType kMockTimer[] =
    FILE_PATH_LITERAL("mock_timer.js");

const char kVirtualKeyboardTestDir[] = "chromeos/virtual_keyboard";

const char kBaseKeyboardTestFramework[] = "virtual_keyboard_test_base.js";

const char kExtensionId[] = "mppnpdlheglhdfmldimlhpnegondlapf";

const char kVirtualKeyboardURL[] = "chrome://keyboard";

}  // namespace

VirtualKeyboardBrowserTestConfig::VirtualKeyboardBrowserTestConfig()
    : base_framework_(kBaseKeyboardTestFramework),
      extension_id_(kExtensionId),
      test_dir_(kVirtualKeyboardTestDir),
      url_(kVirtualKeyboardURL) {
}

VirtualKeyboardBrowserTestConfig::~VirtualKeyboardBrowserTestConfig() {};

void VirtualKeyboardBrowserTest::SetUpCommandLine(CommandLine* command_line) {
  command_line->AppendSwitch(keyboard::switches::kEnableVirtualKeyboard);
}

void VirtualKeyboardBrowserTest::RunTest(
    const base::FilePath& file,
    const VirtualKeyboardBrowserTestConfig& config) {
  ui_test_utils::NavigateToURL(browser(), GURL(config.url_));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::WaitForLoadStop(web_contents);
  ASSERT_TRUE(web_contents);

  // Inject testing scripts.
  InjectJavascript(base::FilePath(kWebuiTestDir),
                   base::FilePath(kMockController));
  InjectJavascript(base::FilePath(kWebuiTestDir), base::FilePath(kMockTimer));
  InjectJavascript(base::FilePath(FILE_PATH_LITERAL(config.test_dir_)),
                   base::FilePath(FILE_PATH_LITERAL(config.base_framework_)));
  InjectJavascript(base::FilePath(FILE_PATH_LITERAL(config.test_dir_)), file);

  ASSERT_TRUE(content::ExecuteScript(web_contents, utf8_content_));

  // Inject DOM-automation test harness and run tests.
  std::vector<int> resource_ids;
  EXPECT_TRUE(ExecuteWebUIResourceTest(web_contents, resource_ids));
}

void VirtualKeyboardBrowserTest::ShowVirtualKeyboard() {
  aura::Window* window = ash::Shell::GetPrimaryRootWindow();
  ui::InputMethod* input_method =
      window->GetProperty(aura::client::kRootWindowInputMethodKey);
  ASSERT_TRUE(input_method);
  input_method->ShowImeIfNeeded();
}

content::RenderViewHost* VirtualKeyboardBrowserTest::GetKeyboardRenderViewHost(
    const std::string& id) {
  ShowVirtualKeyboard();
  GURL url = extensions::Extension::GetBaseURLFromExtensionId(id);
  scoped_ptr<content::RenderWidgetHostIterator> widgets(
      content::RenderWidgetHost::GetRenderWidgetHosts());
  while (content::RenderWidgetHost* widget = widgets->GetNextHost()) {
    if (widget->IsRenderView()) {
      content::RenderViewHost* view = content::RenderViewHost::From(widget);
      if (url == view->GetSiteInstance()->GetSiteURL()) {
        content::WebContents* wc =
            content::WebContents::FromRenderViewHost(view);
        // Waits for Polymer to load.
        content::WaitForLoadStop(wc);
        return view;
      }
    }
  }
  LOG(ERROR) << "Extension not found:" << url;
  return NULL;
}

void VirtualKeyboardBrowserTest::InjectJavascript(const base::FilePath& dir,
                                                  const base::FilePath& file) {
  base::FilePath path = ui_test_utils::GetTestFilePath(dir, file);
  std::string library_content;
  ASSERT_TRUE(base::ReadFileToString(path, &library_content)) << path.value();
  utf8_content_.append(library_content);
  utf8_content_.append(";\n");
}

// crbug.com/367817. Either this feature or just the test are depending
// on the presense of Object.observe which is presently disabled by default.
IN_PROC_BROWSER_TEST_F(VirtualKeyboardBrowserTest, DISABLED_AttributesTest) {
  RunTest(base::FilePath(FILE_PATH_LITERAL("attributes_test.js")),
          VirtualKeyboardBrowserTestConfig());
}

// crbug.com/387372. This test started failing at Blink r176582.
IN_PROC_BROWSER_TEST_F(VirtualKeyboardBrowserTest, DISABLED_TypingTest) {
  RunTest(base::FilePath(FILE_PATH_LITERAL("typing_test.js")),
          VirtualKeyboardBrowserTestConfig());
}

// crbug.com/387372. This test started failing at Blink r176582.
IN_PROC_BROWSER_TEST_F(VirtualKeyboardBrowserTest, DISABLED_ControlKeysTest) {
  RunTest(base::FilePath(FILE_PATH_LITERAL("control_keys_test.js")),
          VirtualKeyboardBrowserTestConfig());
}

IN_PROC_BROWSER_TEST_F(VirtualKeyboardBrowserTest, HideKeyboardKeyTest) {
  RunTest(base::FilePath(FILE_PATH_LITERAL("hide_keyboard_key_test.js")),
          VirtualKeyboardBrowserTestConfig());
}

// http://crbug.com/396326
IN_PROC_BROWSER_TEST_F(VirtualKeyboardBrowserTest,
                       DISABLED_KeysetTransitionTest) {
  RunTest(base::FilePath(FILE_PATH_LITERAL("keyset_transition_test.js")),
          VirtualKeyboardBrowserTestConfig());
}

// Fails when enabling Object.observe. See http://crbug.com/370004
#if defined(OS_CHROMEOS)
#define MAYBE_IsKeyboardLoaded DISABLED_IsKeyboardLoaded
#else
#define MAYBE_IsKeyboardLoaded IsKeyboardLoaded
#endif
IN_PROC_BROWSER_TEST_F(VirtualKeyboardBrowserTest, MAYBE_IsKeyboardLoaded) {
  content::RenderViewHost* keyboard_rvh =
      GetKeyboardRenderViewHost(kExtensionId);
  ASSERT_TRUE(keyboard_rvh);
  bool loaded = false;
  std::string script = "!!chrome.virtualKeyboardPrivate";
  EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
      keyboard_rvh,
      "window.domAutomationController.send(" + script + ");",
      &loaded));
  // Catches the regression in crbug.com/308653.
  ASSERT_TRUE(loaded);
}

IN_PROC_BROWSER_TEST_F(VirtualKeyboardBrowserTest, DISABLED_EndToEndTest) {
  // Get the virtual keyboard's render view host.
  content::RenderViewHost* keyboard_rvh =
      GetKeyboardRenderViewHost(kExtensionId);
  ASSERT_TRUE(keyboard_rvh);

  // Get the test page's render view host.
  content::RenderViewHost* browser_rvh = browser()->tab_strip_model()->
      GetActiveWebContents()->GetRenderViewHost();
  ASSERT_TRUE(browser_rvh);

  // Set up the test page.
  GURL url = ui_test_utils::GetTestUrl(
      base::FilePath(),
      base::FilePath(FILE_PATH_LITERAL(
          "chromeos/virtual_keyboard/end_to_end_test.html")));
  ui_test_utils::NavigateToURL(browser(), url);

  // Press 'a' on keyboard.
  base::FilePath path = ui_test_utils::GetTestFilePath(
      base::FilePath(FILE_PATH_LITERAL(kVirtualKeyboardTestDir)),
      base::FilePath(FILE_PATH_LITERAL("end_to_end_test.js")));
  std::string script;
  ASSERT_TRUE(base::ReadFileToString(path, &script));
  EXPECT_TRUE(content::ExecuteScript(keyboard_rvh, script));
  // Verify 'a' appeared on test page.
  bool success = false;
  EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
      browser_rvh,
      "success ? verifyInput('a') : waitForInput('a');",
      &success));
  ASSERT_TRUE(success);
}

// TODO(kevers|rsadam|bshe):  Add UI tests for remaining virtual keyboard
// functionality.
