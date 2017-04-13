// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accelerators/accelerator_table.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "content/public/test/browser_test_utils.h"
#include "ui/events/keycodes/keyboard_code_conversion.h"

namespace {

class TestWebUIMessageHandler : public content::WebUIMessageHandler {
 public:
  TestWebUIMessageHandler() = default;
  ~TestWebUIMessageHandler() override = default;

  // content::WebUIMessageHandler:
  void RegisterMessages() override {
    web_ui()->RegisterMessageCallback(
        "didPaint", base::Bind(&TestWebUIMessageHandler::HandleDidPaint,
                               base::Unretained(this)));
  }

 private:
  void HandleDidPaint(const base::ListValue*) {}

  DISALLOW_COPY_AND_ASSIGN(TestWebUIMessageHandler);
};

}  // namespace

using KeyboardOverlayUIBrowserTest = InProcessBrowserTest;

IN_PROC_BROWSER_TEST_F(KeyboardOverlayUIBrowserTest,
                       ShouldHaveKeyboardOverlay) {
  ui_test_utils::NavigateToURL(browser(),
                               GURL(chrome::kChromeUIKeyboardOverlayURL));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  web_contents->GetWebUI()->AddMessageHandler(
      base::MakeUnique<TestWebUIMessageHandler>());

  bool is_display_ui_scaling_enabled;
  ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
      web_contents,
      "domAutomationController.send(isDisplayUIScalingEnabled());",
      &is_display_ui_scaling_enabled));

  for (size_t i = 0; i < ash::kAcceleratorDataLength; ++i) {
    const ash::AcceleratorData& entry = ash::kAcceleratorData[i];
    // Skip some accelerators in this test:
    // 1. If the accelerator has no modifier, i.e. ui::EF_NONE, or for "Caps
    // Lock", such as ui::VKEY_MENU and ui::VKEY_LWIN, the logic to show it on
    // the keyboard overlay is not by the mapping of
    // keyboardOverlayData['shortcut'], so it can not be tested by this test.
    // 2. If it has debug modifiers: ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN |
    // ui::EF_SHIFT_DOWN
    if (entry.keycode == ui::VKEY_MENU ||
        entry.keycode == ui::VKEY_LWIN ||
        entry.modifiers == ui::EF_NONE ||
        entry.modifiers ==
            (ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN | ui::EF_SHIFT_DOWN)) {
      continue;
    }

    std::string shortcut;
    ASSERT_TRUE(content::ExecuteScriptAndExtractString(
        web_contents,
        "domAutomationController.send("
        "  (function(number) {"
        "       if (!!KEYCODE_TO_LABEL[number]) {"
        "             return KEYCODE_TO_LABEL[number];"
        "       } else {"
        "             return 'NONE';"
        "       }"
        "  })(" + std::to_string(static_cast<unsigned int>(entry.keycode)) + ")"
        ");",
        &shortcut));
    if (shortcut == "NONE") {
      shortcut = base::ToLowerASCII(
          static_cast<char>(LocatedToNonLocatedKeyboardCode(entry.keycode)));
    }

    // The order of the "if" conditions should not be changed because the
    // modifiers are expected to be alphabetical sorted in the generated
    // shortcut.
    if (entry.modifiers & ui::EF_ALT_DOWN)
      shortcut.append("<>ALT");
    if (entry.modifiers & ui::EF_CONTROL_DOWN)
      shortcut.append("<>CTRL");
    if (entry.modifiers & ui::EF_COMMAND_DOWN)
      shortcut.append("<>SEARCH");
    if (entry.modifiers & ui::EF_SHIFT_DOWN)
      shortcut.append("<>SHIFT");

    if (!is_display_ui_scaling_enabled) {
      if (shortcut == "-<>CTRL<>SHIFT" || shortcut == "+<>CTRL<>SHIFT" ||
          shortcut == "0<>CTRL<>SHIFT")
        continue;
    }

    bool contains;
    ASSERT_TRUE(content::ExecuteScriptAndExtractBool(
        web_contents,
        "domAutomationController.send("
        "  !!keyboardOverlayData['shortcut']['" + shortcut + "']"
        ");",
        &contains));
    ASSERT_TRUE(contains) << "Please add the new accelerators to keyboard "
                             "overlay. Add one entry '" +
                                 shortcut +
                                 "' in the 'shortcut' section"
                                 " at the bottom of the file of "
                                 "'/chrome/browser/resources/chromeos/"
                                 "keyboard_overlay_data.js'. Please keep it in "
                                 "alphabetical order.";
  }
}
