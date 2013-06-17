// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/stringprintf.h"
#include "chrome/browser/extensions/activity_log/activity_log.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/prerender/prerender_handle.h"
#include "chrome/browser/prerender/prerender_manager.h"
#include "chrome/browser/prerender/prerender_manager_factory.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"

namespace extensions {

// Used to fire all of the listeners on the test buttons.
static const char kScriptBeginClickingTestButtons[] =
    "(function() {"
    "  setRunningAsRobot();"
    "  beginClickingTestButtons();"
    "})();";

class ActivityLogExtensionTest : public ExtensionApiTest {
 protected:
  // Make sure the activity log is turned on.
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    ExtensionBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kEnableExtensionActivityLogging);
    command_line->AppendSwitch(switches::kEnableExtensionActivityLogTesting);
    command_line->AppendSwitchASCII(switches::kPrerenderMode,
                                    switches::kPrerenderModeSwitchValueEnabled);
  }
  // Start the test server, load the activity log extension, and navigate
  // the browser to the options page of the extension.
  TabStripModel* StartTestServerAndInitialize() {
    host_resolver()->AddRule("*", "127.0.0.1");
    StartTestServer();

    // Get the extension (chrome/test/data/extensions/activity_log)
    const extensions::Extension* ext =
        LoadExtension(test_data_dir_.AppendASCII("activity_log"));
    CHECK(ext);

    // Open up the Options page.
    ui_test_utils::NavigateToURL(
        browser(),
        GURL("chrome-extension://" + ext->id() + "/options.html"));
    TabStripModel* tab_strip = browser()->tab_strip_model();
    return tab_strip;
  }

  static void Prerender_Arguments(int port,
      scoped_ptr<std::vector<scoped_refptr<Action> > > i) {
    // This is to exit RunLoop (base::MessageLoop::current()->Run()) below
    base::MessageLoop::current()->PostTask(
        FROM_HERE, base::MessageLoop::QuitClosure());

    ASSERT_TRUE(i->size());
    scoped_refptr<Action> last = i->front();

    std::string args = base::StringPrintf(
        "Injected scripts (\"/google_cs.js \") "
        "onto http://www.google.com.bo:%d/test.html (prerender)",
        port);
    // TODO: Replace PrintForDebug with field testing
    // when this feature will be available
    ASSERT_EQ(args, last->PrintForDebug());
  }
};

#if defined(OS_WIN)
// TODO(ataly): test flaky on windows. See Bug: crbug.com/245594
#define MAYBE_ChromeEndToEnd DISABLED_ChromeEndToEnd
#else
#define MAYBE_ChromeEndToEnd ChromeEndToEnd
#endif

IN_PROC_BROWSER_TEST_F(ActivityLogExtensionTest, MAYBE_ChromeEndToEnd) {
  TabStripModel* tab_strip = StartTestServerAndInitialize();
  ResultCatcher catcher;
  // Set the default URL so that is has the correct port number.
  net::HostPortPair host_port = test_server()->host_port_pair();
  std::string url_setting_script = base::StringPrintf(
      "defaultUrl = \'http://www.google.com:%d\';", host_port.port());
  ASSERT_TRUE(content::ExecuteScript(tab_strip->GetActiveWebContents(),
                                     url_setting_script));
  // Set the test buttons array
  std::string test_buttons_setting_script =
      "setTestButtons(document.getElementsByName('chromeButton'))";
  ASSERT_TRUE(content::ExecuteScript(tab_strip->GetActiveWebContents(),
                                     test_buttons_setting_script));
  // Run the test by firing all the buttons.  Wait until completion.
  ASSERT_TRUE(content::ExecuteScript(tab_strip->GetActiveWebContents(),
                                     kScriptBeginClickingTestButtons));
  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
}

#if defined(OS_WIN) || defined(OS_MACOSX)
// TODO(ataly): test flaky on windows and Mac. See Bug: crbug.com/245594
#define MAYBE_DOMEndToEnd DISABLED_DOMEndToEnd
#else
#define MAYBE_DOMEndToEnd DOMEndToEnd
#endif

IN_PROC_BROWSER_TEST_F(ActivityLogExtensionTest, MAYBE_DOMEndToEnd) {
  TabStripModel* tab_strip = StartTestServerAndInitialize();
  ResultCatcher catcher;
  // Set the default URL so that is has the correct port number.
  net::HostPortPair host_port = test_server()->host_port_pair();
  std::string url_setting_script = base::StringPrintf(
      "defaultUrl = \'http://www.google.com:%d\';", host_port.port());
  ASSERT_TRUE(content::ExecuteScript(tab_strip->GetActiveWebContents(),
                                     url_setting_script));
  // Set the test buttons array
  std::string test_buttons_setting_script =
      "setTestButtons(document.getElementsByName('domButton'))";
  ASSERT_TRUE(content::ExecuteScript(tab_strip->GetActiveWebContents(),
                                     test_buttons_setting_script));
  // Run the test by firing all the buttons.  Wait until completion.
  ASSERT_TRUE(content::ExecuteScript(tab_strip->GetActiveWebContents(),
                                     kScriptBeginClickingTestButtons));
  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
}

IN_PROC_BROWSER_TEST_F(ActivityLogExtensionTest, ExtensionPrerender) {
  host_resolver()->AddRule("*", "127.0.0.1");
  StartTestServer();
  int port = test_server()->host_port_pair().port();

  // Get the extension (chrome/test/data/extensions/activity_log)
  const Extension* ext =
      LoadExtension(test_data_dir_.AppendASCII("activity_log"));
  ASSERT_TRUE(ext);

  ActivityLog* activity_log = ActivityLog::GetInstance(profile());
  activity_log->SetDefaultPolicy(ActivityLogPolicy::POLICY_FULLSTREAM);
  ASSERT_TRUE(activity_log);

  //Disable rate limiting in PrerenderManager
  prerender::PrerenderManager* prerender_manager =
      prerender::PrerenderManagerFactory::GetForProfile(profile());
  ASSERT_TRUE(prerender_manager);
  prerender_manager->mutable_config().rate_limit_enabled = false;
  // Increase maximum size of prerenderer, otherwise this test fails
  // on Windows XP.
  prerender_manager->mutable_config().max_bytes = 1000 * 1024 * 1024;

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();;
  ASSERT_TRUE(web_contents);

  content::WindowedNotificationObserver page_observer(
      content::NOTIFICATION_LOAD_STOP,
      content::NotificationService::AllSources());

  GURL url(base::StringPrintf(
      "http://www.google.com.bo:%d/test.html",
      port));

  const gfx::Size kSize(640, 480);
  scoped_ptr<prerender::PrerenderHandle> prerender_handle(
      prerender_manager->AddPrerenderFromLocalPredictor(
          url,
          web_contents->GetController().GetDefaultSessionStorageNamespace(),
          kSize));

  page_observer.Wait();

  activity_log->GetActions(
      ext->id(), 0, base::Bind(
          ActivityLogExtensionTest::Prerender_Arguments, port));

  // Allow invocation of Prerender_Arguments
  base::MessageLoop::current()->Run();
}

}  // namespace extensions
