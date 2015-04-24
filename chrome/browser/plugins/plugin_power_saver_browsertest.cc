// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
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
#include "ui/gfx/geometry/point.h"

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

  bool IsPluginPeripheral(const char* element_id) {
    std::string script = base::StringPrintf(
        "var plugin = window.document.getElementById('%s');"
        "function handleEvent() {"
        "  if (event.data.isPeripheral != undefined &&"
        "      event.data.source == 'getPeripheralStatusResponse') {"
        "    window.domAutomationController.send("
        "        event.data.isPeripheral ? 'peripheral' : 'essential');"
        "    plugin.removeEventListener('message', handleEvent);"
        "  }"
        "}"
        "if (plugin == undefined ||"
        "    (plugin.nodeName != 'OBJECT' && plugin.nodeName != 'EMBED')) {"
        "  window.domAutomationController.send('error');"
        "} else if (plugin.postMessage == undefined) {"
        "  window.domAutomationController.send('peripheral');"
        "} else {"
        "  plugin.addEventListener('message', handleEvent);"
        "  plugin.postMessage('getPeripheralStatus');"
        "}",
        element_id);

    std::string result;
    EXPECT_TRUE(content::ExecuteScriptAndExtractString(GetActiveWebContents(),
                                                       script, &result));
    EXPECT_NE("error", result);
    return result == "peripheral";
  }

  // This sends a simulated click at |point| and waits for test plugin to send
  // a status message indicating that it is essential. The test plugin sends a
  // status message during:
  //  - Plugin creation, to handle a plugin freshly created from a poster.
  //  - Peripheral status change.
  //  - In response to the explicit 'getPeripheralStatus' request, in case the
  //    test has missed the above two events.
  void SimulateClickAndAwaitMarkedEssential(const char* element_id,
                                            const gfx::Point& point) {
    content::SimulateMouseClickAt(GetActiveWebContents(), 0 /* modifiers */,
                                  blink::WebMouseEvent::ButtonLeft, point);

    std::string script = base::StringPrintf(
        "var plugin = window.document.getElementById('%s');"
        "function handleEvent() {"
        "  if (event.data.isPeripheral == false) {"
        "    window.domAutomationController.send('essential');"
        "    plugin.removeEventListener('message', handleEvent);"
        "  }"
        "}"
        "if (plugin == undefined ||"
        "    (plugin.nodeName != 'OBJECT' && plugin.nodeName != 'EMBED')) {"
        "  window.domAutomationController.send('error');"
        "} else {"
        "  plugin.addEventListener('message', handleEvent);"
        "  if (plugin.postMessage != undefined) {"
        "    plugin.postMessage('getPeripheralStatus');"
        "  }"
        "}",
        element_id);

    std::string result;
    EXPECT_TRUE(content::ExecuteScriptAndExtractString(GetActiveWebContents(),
                                                       script, &result));
    EXPECT_EQ("essential", result);
  }
};

IN_PROC_BROWSER_TEST_F(PluginPowerSaverBrowserTest, SmallSameOrigin) {
  LoadHTML(
      "<object id='plugin' data='fake.swf' "
      "type='application/x-ppapi-tests' width='400' height='100'></object>");
  EXPECT_FALSE(IsPluginPeripheral("plugin"));
}

IN_PROC_BROWSER_TEST_F(PluginPowerSaverBrowserTest, SmallCrossOrigin) {
  LoadHTML(
      "<object id='plugin' data='http://otherorigin.com/fake.swf' "
      "type='application/x-ppapi-tests' width='400' height='100'></object>");
  EXPECT_TRUE(IsPluginPeripheral("plugin"));

  SimulateClickAndAwaitMarkedEssential("plugin", gfx::Point(50, 50));
}

IN_PROC_BROWSER_TEST_F(PluginPowerSaverBrowserTest, LargeCrossOrigin) {
  LoadHTML(
      "<object id='plugin' data='http://otherorigin.com/fake.swf' "
      "type='application/x-ppapi-tests' width='400' height='500'></object>");
  EXPECT_FALSE(IsPluginPeripheral("plugin"));
}

IN_PROC_BROWSER_TEST_F(PluginPowerSaverBrowserTest, LargePluginsUsePosters) {
  LoadHTML(
      "<object id='plugin' type='application/x-ppapi-tests' "
      "    width='400' height='500'>"
      "  <param name='poster' value='snapshot1x.png 1x, snapshot2x.png 2x' />"
      "</object>");
  EXPECT_TRUE(IsPluginPeripheral("plugin"));
}

IN_PROC_BROWSER_TEST_F(PluginPowerSaverBrowserTest, OriginWhitelisting) {
  LoadHTML(
      "<object id='plugin1' data='http://otherorigin.com/fake1.swf' "
      "type='application/x-ppapi-tests' width='400' height='100'></object>"
      "<object id='plugin2' data='http://otherorigin.com/fake2.swf' "
      "type='application/x-ppapi-tests' width='400' height='500'></object>");
  EXPECT_FALSE(IsPluginPeripheral("plugin1"));
  EXPECT_FALSE(IsPluginPeripheral("plugin2"));
}

IN_PROC_BROWSER_TEST_F(PluginPowerSaverBrowserTest, LargeCrossOriginObscured) {
  LoadHTML(
      "<div style='width: 100px; height: 100px; overflow: hidden;'>"
      "  <object id='plugin' data='http://otherorigin.com/fake.swf' "
      "  type='application/x-ppapi-tests' width='400' height='500'></object>"
      "</div>");
  EXPECT_TRUE(IsPluginPeripheral("plugin"));
}
