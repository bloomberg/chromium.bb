// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_test_message_listener.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace {
static const char kDomain[] = "a.com";
static const char kSitesDir[] = "automation/sites";
static const char kGotTree[] = "got_tree";
}  // anonymous namespace

class AutomationApiTest : public ExtensionApiTest {
 protected:
  GURL GetURLForPath(const std::string& host, const std::string& path) {
    std::string port = base::IntToString(embedded_test_server()->port());
    GURL::Replacements replacements;
    replacements.SetHostStr(host);
    replacements.SetPortStr(port);
    GURL url =
        embedded_test_server()->GetURL(path).ReplaceComponents(replacements);
    return url;
  }

  void StartEmbeddedTestServer() {
    base::FilePath test_data;
    ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &test_data));
    embedded_test_server()->ServeFilesFromDirectory(
        test_data.AppendASCII("extensions/api_test")
        .AppendASCII(kSitesDir));
    ASSERT_TRUE(ExtensionApiTest::StartEmbeddedTestServer());
    host_resolver()->AddRule("*", embedded_test_server()->base_url().host());
  }

  void LoadPage() {
    StartEmbeddedTestServer();
    const GURL url = GetURLForPath(kDomain, "/index.html");
    ui_test_utils::NavigateToURL(browser(), url);
  }

 public:
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    ExtensionApiTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(::switches::kEnableAutomationAPI);
  }

  virtual void SetUpInProcessBrowserTestFixture() OVERRIDE {
    ExtensionApiTest::SetUpInProcessBrowserTestFixture();
  }
};

IN_PROC_BROWSER_TEST_F(AutomationApiTest, TestRendererAccessibilityEnabled) {
  LoadPage();

  ASSERT_EQ(1, browser()->tab_strip_model()->count());
  content::WebContents* const tab =
      browser()->tab_strip_model()->GetWebContentsAt(0);
  content::RenderWidgetHost* rwh =
      tab->GetRenderWidgetHostView()->GetRenderWidgetHost();
  ASSERT_NE((content::RenderWidgetHost*)NULL, rwh)
      << "Couldn't get RenderWidgetHost";
  ASSERT_FALSE(rwh->IsFullAccessibilityModeForTesting());
  ASSERT_FALSE(rwh->IsTreeOnlyAccessibilityModeForTesting());

  base::FilePath extension_path =
      test_data_dir_.AppendASCII("automation/basic");
  ExtensionTestMessageListener got_tree(kGotTree, false /* no reply */);
  LoadExtension(extension_path);
  ASSERT_TRUE(got_tree.WaitUntilSatisfied());

  rwh = tab->GetRenderWidgetHostView()->GetRenderWidgetHost();
  ASSERT_NE((content::RenderWidgetHost*)NULL, rwh)
      << "Couldn't get RenderWidgetHost";
  ASSERT_FALSE(rwh->IsFullAccessibilityModeForTesting());
  ASSERT_TRUE(rwh->IsTreeOnlyAccessibilityModeForTesting());
}

// TODO(dtseng): See crbug.com/360297.
#if defined(OS_MACOSX)
#define MAYBE_SanityCheck DISABLED_SanityCheck
#else
#define MAYBE_SanityCheck SanityCheck
#endif  // defined(OS_MACOSX)
IN_PROC_BROWSER_TEST_F(AutomationApiTest, MAYBE_SanityCheck) {
  StartEmbeddedTestServer();
  ASSERT_TRUE(RunExtensionSubtest("automation/tests", "sanity_check.html"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(AutomationApiTest, Events) {
  LoadPage();
  ASSERT_TRUE(RunExtensionSubtest("automation/tests", "events.html"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(AutomationApiTest, Actions) {
  LoadPage();
  ASSERT_TRUE(RunExtensionSubtest("automation/tests", "actions.html"))
      << message_;
}

}  // namespace extensions
