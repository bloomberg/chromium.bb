// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/test_switches.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/crx_file/id_util.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "extensions/common/extensions_client.h"
#include "extensions/test/extension_test_message_listener.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

namespace extensions {

namespace {
const std::string kAllUrlsTarget = "/extensions/api_test/all_urls/index.html";
}

class AllUrlsApiTest : public ExtensionApiTest,
                       public ExtensionRegistryObserver {
 protected:
  AllUrlsApiTest() : wait_until_reload_(false),
                     content_script_is_reloaded_(false),
                     execute_script_is_reloaded_(false) {}
  ~AllUrlsApiTest() override {}

  const Extension* content_script() const { return content_script_.get(); }
  const Extension* execute_script() const { return execute_script_.get(); }

  // ExtensionRegistryObserver implementation
  void OnExtensionLoaded(
      content::BrowserContext*,
      const Extension* extension) override {
    if (!wait_until_reload_)
      return;

    if (extension->id() == content_script_->id())
      content_script_is_reloaded_ = true;
    else if (extension->id() == execute_script_->id())
      execute_script_is_reloaded_ = true;
    if (content_script_is_reloaded_ && execute_script_is_reloaded_) {
      base::MessageLoop::current()->QuitWhenIdle();
      wait_until_reload_ = false;
    }
  }

  void WhitelistExtensions() {
    ScopedObserver<ExtensionRegistry, ExtensionRegistryObserver> observer(this);
    observer.Add(ExtensionRegistry::Get(browser()->profile()));

    ExtensionsClient::ScriptingWhitelist whitelist;
    whitelist.push_back(content_script_->id());
    whitelist.push_back(execute_script_->id());
    ExtensionsClient::Get()->SetScriptingWhitelist(whitelist);
    // Extensions will have certain permissions withheld at initialization if
    // they aren't whitelisted, so we need to reload them.
    content_script_is_reloaded_ = false;
    execute_script_is_reloaded_ = false;
    wait_until_reload_ = true;
    extension_service()->ReloadExtension(content_script_->id());
    extension_service()->ReloadExtension(execute_script_->id());
    base::MessageLoop::current()->Run();
  }

 private:
  void SetUpOnMainThread() override {
    ExtensionApiTest::SetUpOnMainThread();
    base::FilePath data_dir = test_data_dir_.AppendASCII("all_urls");
    content_script_ = LoadExtension(data_dir.AppendASCII("content_script"));
    ASSERT_TRUE(content_script_);
    execute_script_ = LoadExtension(data_dir.AppendASCII("execute_script"));
    ASSERT_TRUE(execute_script_);
  }

  scoped_refptr<const Extension> content_script_;
  scoped_refptr<const Extension> execute_script_;

  bool wait_until_reload_;
  bool content_script_is_reloaded_;
  bool execute_script_is_reloaded_;

  DISALLOW_COPY_AND_ASSIGN(AllUrlsApiTest);
};

IN_PROC_BROWSER_TEST_F(AllUrlsApiTest, WhitelistedExtension) {
#if defined(OS_WIN) && defined(USE_ASH)
  // Disable this test in Metro+Ash for now (http://crbug.com/262796).
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kAshBrowserTests))
    return;
#endif

  WhitelistExtensions();

  std::string url;

  // Now verify we run content scripts on chrome://newtab/.
  url = "chrome://newtab/";
  ExtensionTestMessageListener listener1a("content script: " + url, false);
  ExtensionTestMessageListener listener1b("execute: " + url, false);
  ui_test_utils::NavigateToURL(browser(), GURL(url));
  ASSERT_TRUE(listener1a.WaitUntilSatisfied());
  ASSERT_TRUE(listener1b.WaitUntilSatisfied());

  // Now verify data: urls.
  url = "data:text/html;charset=utf-8,<html>asdf</html>";
  ExtensionTestMessageListener listener2a("content script: " + url, false);
  ExtensionTestMessageListener listener2b("execute: " + url, false);
  ui_test_utils::NavigateToURL(browser(), GURL(url));
  ASSERT_TRUE(listener2a.WaitUntilSatisfied());
  ASSERT_TRUE(listener2b.WaitUntilSatisfied());

  // Now verify chrome://version/.
  url = "chrome://version/";
  ExtensionTestMessageListener listener3a("content script: " + url, false);
  ExtensionTestMessageListener listener3b("execute: " + url, false);
  ui_test_utils::NavigateToURL(browser(), GURL(url));
  ASSERT_TRUE(listener3a.WaitUntilSatisfied());
  ASSERT_TRUE(listener3b.WaitUntilSatisfied());

  // Now verify about:blank.
  url = "about:blank";
  ExtensionTestMessageListener listener4a("content script: " + url, false);
  ExtensionTestMessageListener listener4b("execute: " + url, false);
  ui_test_utils::NavigateToURL(browser(), GURL(url));
  ASSERT_TRUE(listener4a.WaitUntilSatisfied());
  ASSERT_TRUE(listener4b.WaitUntilSatisfied());

  // Now verify we can script a regular http page.
  ASSERT_TRUE(StartEmbeddedTestServer());
  GURL page_url = embedded_test_server()->GetURL(kAllUrlsTarget);
  ExtensionTestMessageListener listener5a("content script: " + page_url.spec(),
                                          false);
  ExtensionTestMessageListener listener5b("execute: " + page_url.spec(), false);
  ui_test_utils::NavigateToURL(browser(), page_url);
  ASSERT_TRUE(listener5a.WaitUntilSatisfied());
  ASSERT_TRUE(listener5b.WaitUntilSatisfied());
}

// Test that an extension NOT whitelisted for scripting can ask for <all_urls>
// and run scripts on non-restricted all pages.
IN_PROC_BROWSER_TEST_F(AllUrlsApiTest, RegularExtensions) {
  // Now verify we can script a regular http page.
  ASSERT_TRUE(StartEmbeddedTestServer());
  GURL page_url = embedded_test_server()->GetURL(kAllUrlsTarget);
  ExtensionTestMessageListener listener1a("content script: " + page_url.spec(),
                                          false);
  ExtensionTestMessageListener listener1b("execute: " + page_url.spec(), false);
  ui_test_utils::NavigateToURL(browser(), page_url);
  ASSERT_TRUE(listener1a.WaitUntilSatisfied());
  ASSERT_TRUE(listener1b.WaitUntilSatisfied());
}

// Disabled because sometimes bystander doesn't load.
IN_PROC_BROWSER_TEST_F(AllUrlsApiTest,
                       WhitelistedExtensionRunsOnExtensionPages) {
  WhitelistExtensions();
  const Extension* bystander =
      LoadExtension(test_data_dir_.AppendASCII("all_urls")
                                  .AppendASCII("bystander"));
  ASSERT_TRUE(bystander);

  GURL url(bystander->GetResourceURL("page.html"));
  ExtensionTestMessageListener listenerA(
      "content script: " + url.spec(), false);
  ExtensionTestMessageListener listenerB("execute: " + url.spec(), false);
  ui_test_utils::NavigateToURL(browser(), GURL(url));
  ASSERT_TRUE(listenerA.WaitUntilSatisfied());
  ASSERT_TRUE(listenerB.WaitUntilSatisfied());
}

}  // namespace extensions
