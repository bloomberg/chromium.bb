// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/test/browser_test_utils.h"
#include "grit/browser_resources.h"
#include "ui/base/resource/resource_bundle.h"
#include "url/gurl.h"

namespace {

GURL CreateResource(const std::string& content) {
  base::FilePath path;
  EXPECT_TRUE(base::CreateTemporaryFile(&path));
  EXPECT_TRUE(base::WriteFile(path, content.c_str(), content.size()));
  return GURL("file:///" + path.AsUTF8Unsafe());
}

// Test the CrOS login screen resource loading mechanism.
class ResourceLoaderBrowserTest : public InProcessBrowserTest {
 public:
  ResourceLoaderBrowserTest() {}

 protected:
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    // Needed to load file:// URLs in XHRs.
    command_line->AppendSwitch(switches::kDisableWebSecurity);
  }

  virtual void SetUpOnMainThread() OVERRIDE {
    // Create the root page containing resource_loader.js.
    std::string root_page =
        "<html>"
        "<head>"
        "<script>"
        "  cr = { ui: { login: {} } };"
        "  cr.define = function (path, builder) {"
        "    cr.ui.login.ResourceLoader = builder();"
        "  };"
        "  $ = document.getElementById.bind(document);"
        "</script>"
        "<script>";
    ResourceBundle::GetSharedInstance().GetRawDataResource(
        IDR_OOBE_RESOURCE_LOADER_JS).AppendToString(&root_page);
    root_page +=
        "</script>"
        "</head>"
        "<body>"
        "<div id=\"root\"></div>"
        "</body>"
        "</html>";
    ui_test_utils::NavigateToURL(browser(), CreateResource(root_page));
    JSExpect("!!document.querySelector('#root')");

    // Define global alias for convenience.
    JSEval("ResourceLoader = cr.ui.login.ResourceLoader;");
  }

  void JSEval(const std::string& script) {
    EXPECT_TRUE(content::ExecuteScript(
        browser()->tab_strip_model()->GetActiveWebContents(), script));
  }

  void JSExpect(const std::string& expression) {
    bool result;
    EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
        browser()->tab_strip_model()->GetActiveWebContents(),
        "window.domAutomationController.send(!!(" + expression + "));",
        &result));
    EXPECT_TRUE(result) << expression;
  }

  void JSExpectAsync(const std::string& function) {
    bool result;
    EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
        browser()->tab_strip_model()->GetActiveWebContents(),
        "(" + function + ")(function() {"
        "  window.domAutomationController.send(true);"
        "});",
        &result));
    EXPECT_TRUE(result);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ResourceLoaderBrowserTest);
};

IN_PROC_BROWSER_TEST_F(ResourceLoaderBrowserTest, RegisterAssetsTest) {
  JSExpect("!ResourceLoader.hasDeferredAssets('foo')");
  JSEval("ResourceLoader.registerAssets({ id: 'foo' });");
  JSExpect("ResourceLoader.hasDeferredAssets('foo')");
}

IN_PROC_BROWSER_TEST_F(ResourceLoaderBrowserTest, LoadAssetsTest) {
  // Create a flag to set when the JavaScript is loaded.
  JSEval("stuff = {}");

  // Create the assets.
  std::string html_url = CreateResource("<h1 id=\"bar\">foo</h1>").spec();
  std::string css_url = CreateResource("h1 { color: red; }").spec();
  std::string js_url = CreateResource("stuff.loaded = true;").spec();

  // Register the asset bundle.
  JSEval("ResourceLoader.registerAssets({"
         "  id: 'test-bundle',"
         "  html: [ { url: '" + html_url + "', targetID: 'root' } ]," +
         "  css: [ '" + css_url + "' ]," +
         "  js: [ '" + js_url + "' ]," +
         "});");
  JSExpect("!ResourceLoader.alreadyLoadedAssets('test-bundle')");

  // Load the assets and make sure everything is properly added to the page.
  JSExpectAsync("ResourceLoader.loadAssets.bind(null, 'test-bundle')");
  JSExpect("ResourceLoader.alreadyLoadedAssets('test-bundle')");

  // Check that the HTML was inserted into the root div.
  JSExpect("!!document.querySelector('div#root h1#bar')");

  // Check that the JS was loaded and evaluated.
  JSExpect("stuff.loaded");

  // Check that the styles were loaded.
  JSExpect("!!document.head.querySelector('link').innerHTML.indexOf('red')");
}

}  // namespace
