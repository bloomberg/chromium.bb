// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "url/gurl.h"

class SitePerProcessInteractiveBrowserTest : public InProcessBrowserTest {
 public:
  SitePerProcessInteractiveBrowserTest() {}
  ~SitePerProcessInteractiveBrowserTest() override {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    content::IsolateAllSitesForTesting(command_line);
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");

    // Add content/test/data for cross_site_iframe_factory.html
    embedded_test_server()->ServeFilesFromSourceDirectory("content/test/data");

    ASSERT_TRUE(embedded_test_server()->Start());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(SitePerProcessInteractiveBrowserTest);
};

// Check that document.hasFocus() works properly with out-of-process iframes.
// The test builds a page with four cross-site frames and then focuses them one
// by one, checking the value of document.hasFocus() in all frames.  For any
// given focused frame, document.hasFocus() should return true for that frame
// and all its ancestor frames.
IN_PROC_BROWSER_TEST_F(SitePerProcessInteractiveBrowserTest, DocumentHasFocus) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b(c),d)"));
  ui_test_utils::NavigateToURL(browser(), main_url);

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  content::RenderFrameHost* main_frame = web_contents->GetMainFrame();
  content::RenderFrameHost* child1 = ChildFrameAt(main_frame, 0);
  ASSERT_NE(nullptr, child1);
  content::RenderFrameHost* child2 = ChildFrameAt(main_frame, 1);
  ASSERT_NE(nullptr, child2);
  content::RenderFrameHost* grandchild = ChildFrameAt(child1, 0);
  ASSERT_NE(nullptr, grandchild);

  EXPECT_NE(main_frame->GetSiteInstance(), child1->GetSiteInstance());
  EXPECT_NE(main_frame->GetSiteInstance(), child2->GetSiteInstance());
  EXPECT_NE(child1->GetSiteInstance(), grandchild->GetSiteInstance());

  // Helper function to check document.hasFocus() for a given frame.
  auto document_has_focus = [](content::RenderFrameHost* rfh) -> bool {
    bool has_focus = false;
    EXPECT_TRUE(ExecuteScriptAndExtractBool(
        rfh,
        "window.domAutomationController.send(document.hasFocus())",
        &has_focus));
    return has_focus;
  };

  // The main frame should be focused to start with.
  EXPECT_EQ(main_frame, web_contents->GetFocusedFrame());

  EXPECT_TRUE(document_has_focus(main_frame));
  EXPECT_FALSE(document_has_focus(child1));
  EXPECT_FALSE(document_has_focus(grandchild));
  EXPECT_FALSE(document_has_focus(child2));

  EXPECT_TRUE(ExecuteScript(child1, "window.focus();"));
  EXPECT_EQ(child1, web_contents->GetFocusedFrame());

  EXPECT_TRUE(document_has_focus(main_frame));
  EXPECT_TRUE(document_has_focus(child1));
  EXPECT_FALSE(document_has_focus(grandchild));
  EXPECT_FALSE(document_has_focus(child2));

  EXPECT_TRUE(ExecuteScript(grandchild, "window.focus();"));
  EXPECT_EQ(grandchild, web_contents->GetFocusedFrame());

  EXPECT_TRUE(document_has_focus(main_frame));
  EXPECT_TRUE(document_has_focus(child1));
  EXPECT_TRUE(document_has_focus(grandchild));
  EXPECT_FALSE(document_has_focus(child2));

  EXPECT_TRUE(ExecuteScript(child2, "window.focus();"));
  EXPECT_EQ(child2, web_contents->GetFocusedFrame());

  EXPECT_TRUE(document_has_focus(main_frame));
  EXPECT_FALSE(document_has_focus(child1));
  EXPECT_FALSE(document_has_focus(grandchild));
  EXPECT_TRUE(document_has_focus(child2));
}

// Ensure that a cross-process subframe can receive keyboard events when in
// focus.
IN_PROC_BROWSER_TEST_F(SitePerProcessInteractiveBrowserTest,
                       SubframeKeyboardEventRouting) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/frame_tree/page_with_one_frame.html"));
  ui_test_utils::NavigateToURL(browser(), main_url);
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  GURL frame_url(
      embedded_test_server()->GetURL("b.com", "/page_with_input_field.html"));
  EXPECT_TRUE(NavigateIframeToURL(web_contents, "child0", frame_url));

  // Focus the subframe and then its input field.  The return value
  // "input-focus" will be sent once the input field's focus event fires.
  content::RenderFrameHost* child =
      ChildFrameAt(web_contents->GetMainFrame(), 0);
  std::string result;
  EXPECT_TRUE(ExecuteScriptAndExtractString(
      child, "window.focus(); focusInputField();", &result));
  EXPECT_EQ("input-focus", result);

  // The subframe should now be focused.
  EXPECT_EQ(child, web_contents->GetFocusedFrame());

  // Generate a few keyboard events and route them to currently focused frame.
  SimulateKeyPress(web_contents, ui::VKEY_F, false, false, false, false);
  SimulateKeyPress(web_contents, ui::VKEY_O, false, false, false, false);
  SimulateKeyPress(web_contents, ui::VKEY_O, false, false, false, false);

  // Verify that the input field in the subframe received the keystrokes.
  EXPECT_TRUE(ExecuteScriptAndExtractString(
      child,
      "window.domAutomationController.send(getInputFieldText());", &result));
  EXPECT_EQ("FOO", result);
}

// Ensure that sequential focus navigation (advancing focused elements with
// <tab> and <shift-tab>) works across cross-process subframes.
// The test sets up six inputs fields in a page with two cross-process
// subframes:
//                 child1            child2
//             /------------\    /------------\.
//             | 2. <input> |    | 4. <input> |
//  1. <input> | 3. <input> |    | 5. <input> |  6. <input>
//             \------------/    \------------/.
//
// The test then presses <tab> six times to cycle through focused elements 1-6.
// The test then repeats this with <shift-tab> to cycle in reverse order.
IN_PROC_BROWSER_TEST_F(SitePerProcessInteractiveBrowserTest,
                       SequentialFocusNavigation) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b,c)"));
  ui_test_utils::NavigateToURL(browser(), main_url);

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  content::RenderFrameHost* main_frame = web_contents->GetMainFrame();
  content::RenderFrameHost* child1 = ChildFrameAt(main_frame, 0);
  ASSERT_NE(nullptr, child1);
  content::RenderFrameHost* child2 = ChildFrameAt(main_frame, 1);
  ASSERT_NE(nullptr, child2);

  // Assign a name to each frame.  This will be sent along in test messages
  // from focus events.
  EXPECT_TRUE(ExecuteScript(main_frame, "window.name = 'root';"));
  EXPECT_TRUE(ExecuteScript(child1, "window.name = 'child1';"));
  EXPECT_TRUE(ExecuteScript(child2, "window.name = 'child2';"));

  // This script will insert two <input> fields in the document, one at the
  // beginning and one at the end.  For root frame, this means that we will
  // have an <input>, then two <iframe> elements, then another <input>.
  std::string script =
      "function onFocus(e) {"
      "  domAutomationController.setAutomationId(0);"
      "  domAutomationController.send(window.name + '-focused-' + e.target.id);"
      "}"
      "var input1 = document.createElement('input');"
      "input1.id = 'input1';"
      "var input2 = document.createElement('input');"
      "input2.id = 'input2';"
      "document.body.insertBefore(input1, document.body.firstChild);"
      "document.body.appendChild(input2);"
      "input1.addEventListener('focus', onFocus, false);"
      "input2.addEventListener('focus', onFocus, false);";

  // Add two input fields to each of the three frames.
  EXPECT_TRUE(ExecuteScript(main_frame, script));
  EXPECT_TRUE(ExecuteScript(child1, script));
  EXPECT_TRUE(ExecuteScript(child2, script));

  // Helper to simulate a tab press and wait for a focus message.
  auto press_tab_and_wait_for_message = [web_contents](bool reverse) {
    content::DOMMessageQueue msg_queue;
    std::string reply;
    SimulateKeyPress(web_contents, ui::VKEY_TAB, false, reverse /* shift */,
                     false, false);
    EXPECT_TRUE(msg_queue.WaitForMessage(&reply));
    return reply;
  };

  // Press <tab> six times to focus each of the <input> elements in turn.
  EXPECT_EQ("\"root-focused-input1\"", press_tab_and_wait_for_message(false));
  EXPECT_EQ(main_frame, web_contents->GetFocusedFrame());
  EXPECT_EQ("\"child1-focused-input1\"", press_tab_and_wait_for_message(false));
  EXPECT_EQ(child1, web_contents->GetFocusedFrame());
  EXPECT_EQ("\"child1-focused-input2\"", press_tab_and_wait_for_message(false));
  EXPECT_EQ("\"child2-focused-input1\"", press_tab_and_wait_for_message(false));
  EXPECT_EQ(child2, web_contents->GetFocusedFrame());
  EXPECT_EQ("\"child2-focused-input2\"", press_tab_and_wait_for_message(false));
  EXPECT_EQ("\"root-focused-input2\"", press_tab_and_wait_for_message(false));
  EXPECT_EQ(main_frame, web_contents->GetFocusedFrame());

  // Now, press <shift-tab> to navigate focus in the reverse direction.
  EXPECT_EQ("\"child2-focused-input2\"", press_tab_and_wait_for_message(true));
  EXPECT_EQ(child2, web_contents->GetFocusedFrame());
  EXPECT_EQ("\"child2-focused-input1\"", press_tab_and_wait_for_message(true));
  EXPECT_EQ("\"child1-focused-input2\"", press_tab_and_wait_for_message(true));
  EXPECT_EQ(child1, web_contents->GetFocusedFrame());
  EXPECT_EQ("\"child1-focused-input1\"", press_tab_and_wait_for_message(true));
  EXPECT_EQ("\"root-focused-input1\"", press_tab_and_wait_for_message(true));
  EXPECT_EQ(main_frame, web_contents->GetFocusedFrame());
}

