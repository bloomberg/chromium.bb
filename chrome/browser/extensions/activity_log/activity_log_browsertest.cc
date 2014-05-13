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
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

namespace extensions {

// Only the prerender tests are in this file. To add tests for activity
// logging please see:
//    chrome/test/data/extensions/api_test/activity_log_private/README

class ActivityLogPrerenderTest : public ExtensionApiTest {
 protected:
  // Make sure the activity log is turned on.
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    ExtensionBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kEnableExtensionActivityLogging);
    command_line->AppendSwitchASCII(switches::kPrerenderMode,
                                    switches::kPrerenderModeSwitchValueEnabled);
  }

  static void Prerender_Arguments(
      const std::string& extension_id,
      int port,
      scoped_ptr<std::vector<scoped_refptr<Action> > > i) {
    // This is to exit RunLoop (base::MessageLoop::current()->Run()) below
    base::MessageLoop::current()->PostTask(
        FROM_HERE, base::MessageLoop::QuitClosure());

    ASSERT_TRUE(i->size());
    scoped_refptr<Action> last = i->front();

    ASSERT_EQ(extension_id, last->extension_id());
    ASSERT_EQ(Action::ACTION_CONTENT_SCRIPT, last->action_type());
    ASSERT_EQ("[\"/google_cs.js\"]",
              ActivityLogPolicy::Util::Serialize(last->args()));
    ASSERT_EQ(
        base::StringPrintf("http://www.google.com.bo:%d/test.html", port),
        last->SerializePageUrl());
    ASSERT_EQ(
        base::StringPrintf("www.google.com.bo:%d/test.html", port),
        last->page_title());
    ASSERT_EQ("{\"prerender\":true}",
              ActivityLogPolicy::Util::Serialize(last->other()));
    ASSERT_EQ("", last->api_name());
    ASSERT_EQ("", last->SerializeArgUrl());
  }
};

IN_PROC_BROWSER_TEST_F(ActivityLogPrerenderTest, TestScriptInjected) {
  host_resolver()->AddRule("*", "127.0.0.1");
  ASSERT_TRUE(StartEmbeddedTestServer());
  int port = embedded_test_server()->port();

  // Get the extension (chrome/test/data/extensions/activity_log)
  const Extension* ext =
      LoadExtension(test_data_dir_.AppendASCII("activity_log"));
  ASSERT_TRUE(ext);

  ActivityLog* activity_log = ActivityLog::GetInstance(profile());
  ASSERT_TRUE(activity_log);

  //Disable rate limiting in PrerenderManager
  prerender::PrerenderManager* prerender_manager =
      prerender::PrerenderManagerFactory::GetForProfile(profile());
  ASSERT_TRUE(prerender_manager);
  prerender_manager->mutable_config().rate_limit_enabled = false;
  // Increase prerenderer limits, otherwise this test fails
  // on Windows XP.
  prerender_manager->mutable_config().max_bytes = 1000 * 1024 * 1024;
  prerender_manager->mutable_config().time_to_live =
      base::TimeDelta::FromMinutes(10);
  prerender_manager->mutable_config().abandon_time_to_live =
      base::TimeDelta::FromMinutes(10);

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);

  content::WindowedNotificationObserver page_observer(
      content::NOTIFICATION_LOAD_STOP,
      content::NotificationService::AllSources());

  GURL url(base::StringPrintf(
      "http://www.google.com.bo:%d/test.html",
      port));

  if (!prerender_manager->cookie_store_loaded()) {
    base::RunLoop loop;
    prerender_manager->set_on_cookie_store_loaded_cb_for_testing(
        loop.QuitClosure());
    loop.Run();
  }

  const gfx::Size kSize(640, 480);
  scoped_ptr<prerender::PrerenderHandle> prerender_handle(
      prerender_manager->AddPrerenderFromLocalPredictor(
          url,
          web_contents->GetController().GetDefaultSessionStorageNamespace(),
          kSize));

  page_observer.Wait();

  activity_log->GetFilteredActions(
      ext->id(),
      Action::ACTION_ANY,
      "",
      "",
      "",
      -1,
      base::Bind(
          ActivityLogPrerenderTest::Prerender_Arguments, ext->id(), port));

  // Allow invocation of Prerender_Arguments
  base::MessageLoop::current()->Run();
}

}  // namespace extensions
