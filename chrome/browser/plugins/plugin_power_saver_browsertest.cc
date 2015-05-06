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
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/ppapi_test_utils.h"
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

void VerifyPluginIsPlaceholderOnly(content::WebContents* contents,
                                   const char* element_id) {
  std::string result = RunTestScript(
      "if (plugin.postMessage === undefined) {"
      "  window.domAutomationController.send('placeholder');"
      "} else {"
      "  window.domAutomationController.send('plugin_loaded');"
      "}",
      contents, element_id);
  EXPECT_EQ("placeholder", result);
}

void VerifyPluginIsPeripheral(content::WebContents* contents,
                              const char* element_id) {
  std::string result = RunTestScript(
      "function handleEvent(event) {"
      "  if (event.data.isPeripheral !== undefined && "
      "      event.data.source === 'getPowerSaverStatusResponse') {"
      "    window.domAutomationController.send("
      "        event.data.isPeripheral ? 'peripheral' : 'essential');"
      "    plugin.removeEventListener('message', handleEvent);"
      "  }"
      "}"
      "if (plugin.postMessage === undefined) {"
      "  window.domAutomationController.send('peripheral');"
      "} else {"
      "  plugin.addEventListener('message', handleEvent);"
      "  plugin.postMessage('getPowerSaverStatus');"
      "}",
      contents, element_id);
  EXPECT_EQ("peripheral", result);
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
}

}  // namespace

class PluginPowerSaverBrowserTest : virtual public InProcessBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kEnablePluginPowerSaver);
    command_line->AppendSwitch(switches::kEnablePepperTesting);

    ASSERT_TRUE(ppapi::RegisterPowerSaverTestPlugin(command_line));
  }

 protected:
  void LoadHTML(const char* html) {
    std::string url_str = "data:text/html;charset=utf-8,";
    url_str.append(html);
    ui_test_utils::NavigateToURL(browser(), GURL(url_str));
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
  VerifyPluginIsPeripheral(GetActiveWebContents(), "plugin");

  SimulateClickAndAwaitMarkedEssential("plugin", gfx::Point(50, 50));
}

IN_PROC_BROWSER_TEST_F(PluginPowerSaverBrowserTest, LargeCrossOrigin) {
  LoadHTML(
      "<object id='plugin' data='http://otherorigin.com/fake.swf' "
      "    type='application/x-ppapi-tests' width='400' height='500'>"
      "</object>");
  VerifyPluginMarkedEssential(GetActiveWebContents(), "plugin");
}

IN_PROC_BROWSER_TEST_F(PluginPowerSaverBrowserTest,
                       LargePluginsPeripheralWhenPosterSpecified) {
  LoadHTML(
      "<object id='plugin_src' type='application/x-ppapi-tests' "
      "    width='400' height='500' poster='snapshot1x.png'></object>"
      "<object id='plugin_srcset' type='application/x-ppapi-tests' "
      "    width='400' height='500' "
      "    poster='snapshot1x.png 1x, snapshot2x.png 2x'></object>"
      "<object id='plugin_legacy_syntax' type='application/x-ppapi-tests' "
      "    width='400' height='500'>"
      "  <param name='poster' value='snapshot1x.png 1x, snapshot2x.png 2x'>"
      "</object>"
      "<embed id='plugin_embed_src' type='application/x-ppapi-tests' "
      "    width='400' height='500' poster='snapshot1x.png'></embed>"
      "<embed id='plugin_embed_srcset' type='application/x-ppapi-tests' "
      "    width='400' height='500'"
      "    poster='snapshot1x.png 1x, snapshot2x.png 2x'></embed>");

  VerifyPluginIsPlaceholderOnly(GetActiveWebContents(), "plugin_src");
  VerifyPluginIsPlaceholderOnly(GetActiveWebContents(), "plugin_srcset");
  VerifyPluginIsPlaceholderOnly(GetActiveWebContents(), "plugin_legacy_syntax");
  VerifyPluginIsPlaceholderOnly(GetActiveWebContents(), "plugin_embed_src");
  VerifyPluginIsPlaceholderOnly(GetActiveWebContents(), "plugin_embed_srcset");
}

// flaky: crbug.com/481687
IN_PROC_BROWSER_TEST_F(PluginPowerSaverBrowserTest,
                       DISABLED_PluginMarkedEssentialAfterPosterClicked) {
  LoadHTML(
      "<object id='plugin' type='application/x-ppapi-tests' "
      "    width='400' height='100' poster='snapshot1x.png'></object>");
  VerifyPluginIsPlaceholderOnly(GetActiveWebContents(), "plugin");

  SimulateClickAndAwaitMarkedEssential("plugin", gfx::Point(50, 50));
}

IN_PROC_BROWSER_TEST_F(PluginPowerSaverBrowserTest, OriginWhitelisting) {
  LoadHTML(
      "<object id='plugin1' data='http://otherorigin.com/fake1.swf' "
      "    type='application/x-ppapi-tests' width='400' height='100'></object>"
      "<object id='plugin2' data='http://otherorigin.com/fake2.swf' "
      "    type='application/x-ppapi-tests' width='400' height='500'>"
      "</object>");
  VerifyPluginMarkedEssential(GetActiveWebContents(), "plugin1");
  VerifyPluginMarkedEssential(GetActiveWebContents(), "plugin2");
}

IN_PROC_BROWSER_TEST_F(PluginPowerSaverBrowserTest, LargeCrossOriginObscured) {
  LoadHTML(
      "<div style='width: 100px; height: 100px; overflow: hidden;'>"
      "  <object id='plugin' data='http://otherorigin.com/fake.swf' "
      "      type='application/x-ppapi-tests' width='400' height='500'>"
      "  </object>"
      "</div>");
  VerifyPluginIsPeripheral(GetActiveWebContents(), "plugin");
}

IN_PROC_BROWSER_TEST_F(PluginPowerSaverBrowserTest, BackgroundTabPlugins) {
  std::string url_str =
      "data:text/html;charset=utf-8,"
      "<object id='same_origin' type='application/x-ppapi-tests'></object>"
      "<object id='small_cross_origin' data='http://otherorigin.com/fake1.swf' "
      "   type='application/x-ppapi-tests' width='400' height='100'></object>";
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(url_str), NEW_BACKGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);

  ASSERT_EQ(2, browser()->tab_strip_model()->count());
  content::WebContents* background_contents =
      browser()->tab_strip_model()->GetWebContentsAt(1);
  EXPECT_TRUE(
      content::WaitForRenderFrameReady(background_contents->GetMainFrame()));

  VerifyPluginIsPlaceholderOnly(background_contents, "same_origin");
  VerifyPluginIsPlaceholderOnly(background_contents, "small_cross_origin");

  browser()->tab_strip_model()->SelectNextTab();
  EXPECT_EQ(background_contents, GetActiveWebContents());

  VerifyPluginMarkedEssential(background_contents, "same_origin");
  VerifyPluginIsPeripheral(background_contents, "small_cross_origin");
}
