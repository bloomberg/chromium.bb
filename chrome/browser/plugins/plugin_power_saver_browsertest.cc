// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/strings/string_piece.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/ui/zoom/zoom_controller.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/ppapi_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "ppapi/shared_impl/ppapi_switches.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/geometry/point.h"

namespace {

std::string RunTestScript(base::StringPiece test_script,
                          content::WebContents* contents,
                          const std::string& element_id) {
  std::string script = base::StringPrintf(
      "var plugin = window.document.getElementById('%s');"
      "if (plugin === undefined ||"
      "    (plugin.nodeName !== 'OBJECT' && plugin.nodeName !== 'EMBED')) {"
      "  window.domAutomationController.send('error');"
      "} else {"
      "  %s"
      "}",
      element_id.c_str(), test_script.data());
  std::string result;
  EXPECT_TRUE(
      content::ExecuteScriptAndExtractString(contents, script, &result));
  return result;
}

// This also tests that we have JavaScript access to the underlying plugin.
bool PluginLoaded(content::WebContents* contents, const char* element_id) {
  std::string result = RunTestScript(
      "if (plugin.postMessage === undefined) {"
      "  window.domAutomationController.send('poster_only');"
      "} else {"
      "  window.domAutomationController.send('plugin_loaded');"
      "}",
      contents, element_id);
  EXPECT_NE("error", result);
  return result == "plugin_loaded";
}

// Also waits for the placeholder UI overlay to finish loading.
void VerifyPluginIsThrottled(content::WebContents* contents,
                             const char* element_id) {
  std::string result = RunTestScript(
      "function handleEvent(event) {"
      "  if (event.data.isPeripheral && event.data.isThrottled && "
      "      event.data.isHiddenForPlaceholder) {"
      "    window.domAutomationController.send('throttled');"
      "    plugin.removeEventListener('message', handleEvent);"
      "  }"
      "}"
      "plugin.addEventListener('message', handleEvent);"
      "if (plugin.postMessage !== undefined) {"
      "  plugin.postMessage('getPowerSaverStatus');"
      "}",
      contents, element_id);
  EXPECT_EQ("throttled", result);

  // Page should continue to have JavaScript access to all throttled plugins.
  EXPECT_TRUE(PluginLoaded(contents, element_id));
}

void VerifyPluginMarkedEssential(content::WebContents* contents,
                                 const char* element_id) {
  std::string result = RunTestScript(
      "function handleEvent(event) {"
      "  if (event.data.isPeripheral === false) {"
      "    window.domAutomationController.send('essential');"
      "    plugin.removeEventListener('message', handleEvent);"
      "  }"
      "}"
      "plugin.addEventListener('message', handleEvent);"
      "if (plugin.postMessage !== undefined) {"
      "  plugin.postMessage('getPowerSaverStatus');"
      "}",
      contents, element_id);
  EXPECT_EQ("essential", result);
  EXPECT_TRUE(PluginLoaded(contents, element_id));
}

scoped_ptr<net::test_server::HttpResponse> RespondWithHTML(
    const std::string& html,
    const net::test_server::HttpRequest& request) {
  scoped_ptr<net::test_server::BasicHttpResponse> response(
      new net::test_server::BasicHttpResponse());
  response->set_content_type("text/html");
  response->set_content(html);
  return response.Pass();
}

}  // namespace

class PluginPowerSaverBrowserTest : public InProcessBrowserTest {
 public:
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kEnablePluginPowerSaver);
    command_line->AppendSwitch(switches::kEnablePepperTesting);
    command_line->AppendSwitch(switches::kEnablePluginPlaceholderTesting);
    command_line->AppendSwitchASCII(
        switches::kOverridePluginPowerSaverForTesting, "ignore-list");

    ASSERT_TRUE(ppapi::RegisterPowerSaverTestPlugin(command_line));
  }

 protected:
  void LoadHTML(const std::string& html) {
    ASSERT_TRUE(embedded_test_server()->Started());
    embedded_test_server()->RegisterRequestHandler(
        base::Bind(&RespondWithHTML, html));
    ui_test_utils::NavigateToURL(browser(), embedded_test_server()->base_url());
    EXPECT_TRUE(content::WaitForRenderFrameReady(
        GetActiveWebContents()->GetMainFrame()));
  }

  content::WebContents* GetActiveWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  // This sends a simulated click at |point| and waits for test plugin to send
  // a status message indicating that it is essential. The test plugin sends a
  // status message during:
  //  - Plugin creation, to handle a plugin freshly created from a poster.
  //  - Peripheral status change.
  //  - In response to the explicit 'getPowerSaverStatus' request, in case the
  //    test has missed the above two events.
  void SimulateClickAndAwaitMarkedEssential(const char* element_id,
                                            const gfx::Point& point) {
    // Waits for the placeholder to be ready to be clicked first.
    std::string result = RunTestScript(
        "function handleEvent(event) {"
        "  if (event.data === 'placeholderLoaded') {"
        "    window.domAutomationController.send('ready');"
        "    plugin.removeEventListener('message', handleEvent);"
        "  }"
        "}"
        "plugin.addEventListener('message', handleEvent);"
        "if (plugin.hasAttribute('placeholderLoaded')) {"
        "  window.domAutomationController.send('ready');"
        "  plugin.removeEventListener('message', handleEvent);"
        "}",
        GetActiveWebContents(), element_id);
    ASSERT_EQ("ready", result);

    content::SimulateMouseClickAt(GetActiveWebContents(), 0 /* modifiers */,
                                  blink::WebMouseEvent::ButtonLeft, point);

    VerifyPluginMarkedEssential(GetActiveWebContents(), element_id);
  }
};

IN_PROC_BROWSER_TEST_F(PluginPowerSaverBrowserTest, SmallSameOrigin) {
  LoadHTML(
      "<object id='plugin' data='fake.swf' "
      "    type='application/x-ppapi-tests' width='400' height='100'>"
      "</object>");
  VerifyPluginMarkedEssential(GetActiveWebContents(), "plugin");
}

IN_PROC_BROWSER_TEST_F(PluginPowerSaverBrowserTest, SmallCrossOrigin) {
  LoadHTML(
      "<object id='plugin' data='http://otherorigin.com/fake.swf' "
      "    type='application/x-ppapi-tests' width='400' height='100'>"
      "</object>");
  VerifyPluginIsThrottled(GetActiveWebContents(), "plugin");

  SimulateClickAndAwaitMarkedEssential("plugin", gfx::Point(50, 50));
}

IN_PROC_BROWSER_TEST_F(PluginPowerSaverBrowserTest, LargeCrossOrigin) {
  LoadHTML(
      "<object id='large' data='http://otherorigin.com/fake.swf' "
      "    type='application/x-ppapi-tests' width='400' height='500'>"
      "</object>"
      "<object id='medium_16_9' data='http://otherorigin.com/fake.swf' "
      "    type='application/x-ppapi-tests' width='480' height='270'>"
      "</object>");
  VerifyPluginMarkedEssential(GetActiveWebContents(), "large");
  VerifyPluginMarkedEssential(GetActiveWebContents(), "medium_16_9");
}

IN_PROC_BROWSER_TEST_F(PluginPowerSaverBrowserTest, LargePostersNotThrottled) {
  // This test verifies that small posters are throttled, large posters are not,
  // and that large posters can whitelist origins for other plugins.
  LoadHTML(
      "<object id='poster_small' data='http://a.com/fake.swf' "
      "    type='application/x-ppapi-tests' width='50' height='50' "
      "    poster='click_me.png'></object>"
      "<object id='poster_whitelisted_origin' data='http://b.com/fake.swf' "
      "    type='application/x-ppapi-tests' width='50' height='50' "
      "    poster='click_me.png'></object>"
      "<object id='plugin_whitelisted_origin' data='http://b.com/fake.swf' "
      "    type='application/x-ppapi-tests' width='50' height='50'></object>"
      "<br>"
      "<object id='poster_large' data='http://b.com/fake.swf' "
      "    type='application/x-ppapi-tests' width='400' height='300' "
      "    poster='click_me.png'></object>");

  EXPECT_FALSE(PluginLoaded(GetActiveWebContents(), "poster_small"));
  VerifyPluginMarkedEssential(GetActiveWebContents(),
                              "poster_whitelisted_origin");
  VerifyPluginMarkedEssential(GetActiveWebContents(),
                              "plugin_whitelisted_origin");
  VerifyPluginMarkedEssential(GetActiveWebContents(), "poster_large");
}

IN_PROC_BROWSER_TEST_F(PluginPowerSaverBrowserTest,
                       PluginMarkedEssentialAfterPosterClicked) {
  LoadHTML(
      "<object id='plugin' type='application/x-ppapi-tests' "
      "    width='400' height='100' poster='snapshot1x.png'></object>");
  EXPECT_FALSE(PluginLoaded(GetActiveWebContents(), "plugin"));

  SimulateClickAndAwaitMarkedEssential("plugin", gfx::Point(50, 50));
}

IN_PROC_BROWSER_TEST_F(PluginPowerSaverBrowserTest, OriginWhitelisting) {
  LoadHTML(
      "<object id='plugin_small' data='http://a.com/fake1.swf' "
      "    type='application/x-ppapi-tests' width='100' height='100'></object>"
      "<object id='plugin_small_poster' data='http://a.com/fake1.swf' "
      "    type='application/x-ppapi-tests' width='100' height='100' "
      "    poster='click_me.png'></object>"
      "<object id='plugin_large' data='http://a.com/fake2.swf' "
      "    type='application/x-ppapi-tests' width='400' height='500'>"
      "</object>");
  VerifyPluginMarkedEssential(GetActiveWebContents(), "plugin_small");
  VerifyPluginMarkedEssential(GetActiveWebContents(), "plugin_small_poster");
  VerifyPluginMarkedEssential(GetActiveWebContents(), "plugin_large");
}

IN_PROC_BROWSER_TEST_F(PluginPowerSaverBrowserTest, LargeCrossOriginObscured) {
  LoadHTML(
      "<div id='container' "
      "    style='width: 400px; height: 100px; overflow: hidden;'>"
      "  <object id='plugin' data='http://otherorigin.com/fake.swf' "
      "      type='application/x-ppapi-tests' width='400' height='500'>"
      "  </object>"
      "</div>");
  VerifyPluginIsThrottled(GetActiveWebContents(), "plugin");

  // Test that's unthrottled if it is unobscured.
  std::string script =
      "var container = window.document.getElementById('container');"
      "container.setAttribute('style', 'width: 400px; height: 400px;');";
  ASSERT_TRUE(content::ExecuteScript(GetActiveWebContents(), script));
  VerifyPluginMarkedEssential(GetActiveWebContents(), "plugin");
}

IN_PROC_BROWSER_TEST_F(PluginPowerSaverBrowserTest, ExpandingSmallPlugin) {
  LoadHTML(
      "<object id='plugin' data='http://otherorigin.com/fake.swf' "
      "    type='application/x-ppapi-tests' width='400' height='80'></object>");
  VerifyPluginIsThrottled(GetActiveWebContents(), "plugin");

  std::string script = "window.document.getElementById('plugin').height = 400;";
  ASSERT_TRUE(content::ExecuteScript(GetActiveWebContents(), script));
  VerifyPluginMarkedEssential(GetActiveWebContents(), "plugin");
}

IN_PROC_BROWSER_TEST_F(PluginPowerSaverBrowserTest, BackgroundTabPlugins) {
  std::string html =
      "<object id='same_origin' data='fake.swf' "
      "    type='application/x-ppapi-tests'></object>"
      "<object id='small_cross_origin' data='http://otherorigin.com/fake1.swf' "
      "    type='application/x-ppapi-tests' width='400' height='100'></object>";
  embedded_test_server()->RegisterRequestHandler(
      base::Bind(&RespondWithHTML, html));
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), embedded_test_server()->base_url(), NEW_BACKGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);

  ASSERT_EQ(2, browser()->tab_strip_model()->count());
  content::WebContents* background_contents =
      browser()->tab_strip_model()->GetWebContentsAt(1);
  EXPECT_TRUE(
      content::WaitForRenderFrameReady(background_contents->GetMainFrame()));

  EXPECT_FALSE(PluginLoaded(background_contents, "same_origin"));
  EXPECT_FALSE(PluginLoaded(background_contents, "small_cross_origin"));

  browser()->tab_strip_model()->SelectNextTab();
  EXPECT_EQ(background_contents, GetActiveWebContents());

  VerifyPluginMarkedEssential(background_contents, "same_origin");
  VerifyPluginIsThrottled(background_contents, "small_cross_origin");
}

IN_PROC_BROWSER_TEST_F(PluginPowerSaverBrowserTest, ZoomIndependent) {
  ui_zoom::ZoomController::FromWebContents(GetActiveWebContents())
      ->SetZoomLevel(4.0);
  LoadHTML(
      "<object id='plugin' data='http://otherorigin.com/fake.swf' "
      "    type='application/x-ppapi-tests' width='400' height='200'>"
      "</object>");
  VerifyPluginIsThrottled(GetActiveWebContents(), "plugin");
}
