// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu_browsertest_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/exclusive_access/fullscreen_controller_test.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/guest_view/browser/guest_view_manager_delegate.h"
#include "components/guest_view/browser/test_guest_view_manager.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/api/extensions_api_client.h"
#include "extensions/common/constants.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
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

  gfx::Size GetScreenSize() {
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    const display::Display display =
        display::Screen::GetScreen()->GetDisplayNearestWindow(
            web_contents->GetRenderWidgetHostView()->GetNativeView());
    return display.bounds().size();
  }

  enum class FullscreenExitMethod {
    JS_CALL,
    ESC_PRESS,
  };

  void FullscreenElementInABA(FullscreenExitMethod exit_method);

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
  std::string script =
      "function onInput(e) {"
      "  domAutomationController.setAutomationId(0);"
      "  domAutomationController.send(getInputFieldText());"
      "}"
      "inputField = document.getElementById('text-field');"
      "inputField.addEventListener('input', onInput, false);";
  EXPECT_TRUE(ExecuteScript(child, script));
  EXPECT_TRUE(ExecuteScriptAndExtractString(
      child, "window.focus(); focusInputField();", &result));
  EXPECT_EQ("input-focus", result);

  // The subframe should now be focused.
  EXPECT_EQ(child, web_contents->GetFocusedFrame());

  // Generate a few keyboard events and route them to currently focused frame.
  // We wait for replies to be sent back from the page, since keystrokes may
  // take time to propagate to the renderer's main thread.
  content::DOMMessageQueue msg_queue;
  std::string reply;
  SimulateKeyPress(web_contents, ui::DomKey::FromCharacter('f'),
                   ui::DomCode::US_F, ui::VKEY_F, false, false, false, false);
  EXPECT_TRUE(msg_queue.WaitForMessage(&reply));
  EXPECT_EQ("\"F\"", reply);

  SimulateKeyPress(web_contents, ui::DomKey::FromCharacter('O'),
                   ui::DomCode::US_O, ui::VKEY_O, false, false, false, false);
  EXPECT_TRUE(msg_queue.WaitForMessage(&reply));
  EXPECT_EQ("\"FO\"", reply);

  SimulateKeyPress(web_contents, ui::DomKey::FromCharacter('O'),
                   ui::DomCode::US_O, ui::VKEY_O, false, false, false, false);
  EXPECT_TRUE(msg_queue.WaitForMessage(&reply));
  EXPECT_EQ("\"FOO\"", reply);
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
    SimulateKeyPress(web_contents, ui::DomKey::TAB, ui::DomCode::TAB,
                     ui::VKEY_TAB, false, reverse /* shift */, false, false);
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

namespace {

// Helper to retrieve the frame's (window.innerWidth, window.innerHeight).
gfx::Size GetFrameSize(content::RenderFrameHost* frame) {
  int width = 0;
  EXPECT_TRUE(ExecuteScriptAndExtractInt(
      frame, "domAutomationController.send(window.innerWidth);", &width));

  int height = 0;
  EXPECT_TRUE(ExecuteScriptAndExtractInt(
      frame, "domAutomationController.send(window.innerHeight);", &height));

  return gfx::Size(width, height);
}

// Helper to check |frame|'s document.webkitFullscreenElement and return its ID
// if it's defined (which is the case when |frame| is in fullscreen mode), or
// "none" otherwise.
std::string GetFullscreenElementId(content::RenderFrameHost* frame) {
  std::string fullscreen_element;
  EXPECT_TRUE(ExecuteScriptAndExtractString(
      frame,
      "domAutomationController.send("
      "    document.webkitFullscreenElement ? "
      "        document.webkitFullscreenElement.id : 'none')",
      &fullscreen_element));
  return fullscreen_element;
}

// Helper to check if an element with ID |element_id| has the
// :-webkit-full-screen style.
bool ElementHasFullscreenStyle(content::RenderFrameHost* frame,
                               const std::string& element_id) {
  bool has_style = false;
  std::string script = base::StringPrintf(
      "domAutomationController.send("
      "    document.querySelectorAll('#%s:-webkit-full-screen').length == 1)",
      element_id.c_str());
  EXPECT_TRUE(ExecuteScriptAndExtractBool(frame, script, &has_style));
  return has_style;
}

// Helper to check if an element with ID |element_id| has the
// :-webkit-full-screen-ancestor style.
bool ElementHasFullscreenAncestorStyle(content::RenderFrameHost* host,
                                       const std::string& element_id) {
  bool has_style = false;
  std::string script = base::StringPrintf(
      "domAutomationController.send("
      "    document.querySelectorAll("
      "        '#%s:-webkit-full-screen-ancestor').length == 1)",
      element_id.c_str());
  EXPECT_TRUE(ExecuteScriptAndExtractBool(host, script, &has_style));
  return has_style;
}

// Set the allowFullscreen attribute on the <iframe> element identified by
// |frame_id|.
void SetAllowFullscreenForFrame(content::RenderFrameHost* host,
                                const std::string& frame_id) {
  EXPECT_TRUE(ExecuteScript(
      host, base::StringPrintf(
                "document.getElementById('%s').allowFullscreen = true;",
                frame_id.c_str())));
}

// Add a listener that will send back a message whenever the (prefixed)
// fullscreenchange event fires.  The message will be "fullscreenchange",
// followed by a space and the provided |id|.
void AddFullscreenChangeListener(content::RenderFrameHost* frame,
                                 const std::string& id) {
  std::string script = base::StringPrintf(
      "document.addEventListener('webkitfullscreenchange', function() {"
      "    domAutomationController.setAutomationId(0);"
      "    domAutomationController.send('fullscreenchange %s');});",
      id.c_str());
  EXPECT_TRUE(ExecuteScript(frame, script));
}

// Helper to add a listener that will send back a "resize" message when the
// target |frame| is resized to |expected_size|.
void AddResizeListener(content::RenderFrameHost* frame,
                       const gfx::Size& expected_size) {
  std::string script =
      base::StringPrintf("addResizeListener(%d, %d);",
                         expected_size.width(), expected_size.height());
  EXPECT_TRUE(ExecuteScript(frame, script));
}

// Helper to wait for a toggle fullscreen operation to complete in all affected
// frames.  This means waiting for:
// 1. All fullscreenchange events with id's matching the list in
//    |expected_fullscreen_event_ids|.  Typically the list will correspond to
//    events from the actual fullscreen element and all of its ancestor
//    <iframe> elements.
// 2. A resize event.  This will verify that the frame containing the
//    fullscreen element is properly resized.  This assumes that the expected
//    size is already registered via AddResizeListener().
void WaitForMultipleFullscreenEvents(
    const std::set<std::string>& expected_fullscreen_event_ids,
    content::DOMMessageQueue& queue) {
  std::set<std::string> remaining_events(expected_fullscreen_event_ids);
  bool resize_validated = false;
  std::string response;
  while (queue.WaitForMessage(&response)) {
    base::TrimString(response, "\"", &response);
    std::vector<std::string> response_params = base::SplitString(
        response, " ", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
    if (response_params[0] == "fullscreenchange") {
      EXPECT_TRUE(base::ContainsKey(remaining_events, response_params[1]));
      remaining_events.erase(response_params[1]);
    } else if (response_params[0] == "resize") {
      resize_validated = true;
    }
    if (remaining_events.empty() && resize_validated)
      break;
  }
}

}  // namespace

// Check that an element in a cross-process subframe can enter and exit
// fullscreen.  The test will verify that:
// - the subframe is properly resized
// - the WebContents properly enters/exits fullscreen.
// - document.webkitFullscreenElement is correctly updated in both frames.
// - fullscreenchange events fire in both frames.
// - fullscreen CSS is applied correctly in both frames.
//
IN_PROC_BROWSER_TEST_F(SitePerProcessInteractiveBrowserTest,
                       FullscreenElementInSubframe) {
  // Start on a page with one subframe (id "child-0") that has
  // "allowfullscreen" enabled.
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/page_with_allowfullscreen_frame.html"));
  ui_test_utils::NavigateToURL(browser(), main_url);
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Navigate the subframe cross-site to a page with a fullscreenable <div>.
  GURL frame_url(
      embedded_test_server()->GetURL("b.com", "/fullscreen_frame.html"));
  EXPECT_TRUE(NavigateIframeToURL(web_contents, "child-0", frame_url));

  content::RenderFrameHost* main_frame = web_contents->GetMainFrame();
  content::RenderFrameHost* child = ChildFrameAt(main_frame, 0);
  gfx::Size original_child_size = GetFrameSize(child);

  // Fullscreen the <div> inside the cross-site child frame.  Wait until:
  // (1) the fullscreenchange events in main frame and child send a response,
  // (2) the child frame is resized to fill the whole screen.
  // (3) the browser has finished the fullscreen transition.
  AddFullscreenChangeListener(main_frame, "main_frame");
  AddFullscreenChangeListener(child, "child");
  std::set<std::string> expected_events = {"main_frame", "child"};
  AddResizeListener(child, GetScreenSize());
  {
    content::DOMMessageQueue queue;
    std::unique_ptr<FullscreenNotificationObserver> observer(
        new FullscreenNotificationObserver());
    EXPECT_TRUE(ExecuteScript(child, "activateFullscreen()"));
    WaitForMultipleFullscreenEvents(expected_events, queue);
    observer->Wait();
  }

  // Verify that the browser has entered fullscreen for the current tab.
  EXPECT_TRUE(browser()->window()->IsFullscreen());
  EXPECT_TRUE(browser()->exclusive_access_manager()
                       ->fullscreen_controller()
                       ->IsFullscreenForTabOrPending(web_contents));

  // Verify that the <div> has fullscreen style (:-webkit-full-screen) in the
  // subframe.
  EXPECT_TRUE(ElementHasFullscreenStyle(child, "fullscreen-div"));

  // Verify that the main frame has applied proper fullscreen styles to the
  // <iframe> element (:-webkit-full-screen and :-webkit-full-screen-ancestor).
  // This is what causes the <iframe> to stretch and fill the whole viewport.
  EXPECT_TRUE(ElementHasFullscreenStyle(main_frame, "child-0"));
  EXPECT_TRUE(ElementHasFullscreenAncestorStyle(main_frame, "child-0"));

  // Check document.webkitFullscreenElement.  For main frame, it should point
  // to the subframe, and for subframe, it should point to the fullscreened
  // <div>.
  EXPECT_EQ("child-0", GetFullscreenElementId(main_frame));
  EXPECT_EQ("fullscreen-div", GetFullscreenElementId(child));

  // Now exit fullscreen from the subframe.  Wait for two fullscreenchange
  // events from both frames, and also for the child to be resized to its
  // original size.
  AddResizeListener(child, original_child_size);
  {
    content::DOMMessageQueue queue;
    std::unique_ptr<FullscreenNotificationObserver> observer(
        new FullscreenNotificationObserver());
    EXPECT_TRUE(ExecuteScript(child, "exitFullscreen()"));
    WaitForMultipleFullscreenEvents(expected_events, queue);
    observer->Wait();
  }

  EXPECT_FALSE(browser()->window()->IsFullscreen());

  // Verify that the fullscreen styles were removed from the <div> and its
  // container <iframe>.
  EXPECT_FALSE(ElementHasFullscreenStyle(child, "fullscreen-div"));
  EXPECT_FALSE(ElementHasFullscreenStyle(main_frame, "child-0"));
  EXPECT_FALSE(ElementHasFullscreenAncestorStyle(main_frame, "child-0"));

  // Check that both frames cleared their document.webkitFullscreenElement.
  EXPECT_EQ("none", GetFullscreenElementId(main_frame));
  EXPECT_EQ("none", GetFullscreenElementId(child));
}

// Check that on a page with A-embed-B-embed-A frame hierarchy, an element in
// the bottom frame can enter and exit fullscreen.  |exit_method| specifies
// whether to use browser-initiated vs. renderer-initiated fullscreen exit
// (i.e., pressing escape vs. a JS call), since they trigger different code
// paths on the Blink side.
void SitePerProcessInteractiveBrowserTest::FullscreenElementInABA(
    FullscreenExitMethod exit_method) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b(a))"));
  ui_test_utils::NavigateToURL(browser(), main_url);
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  content::RenderFrameHost* main_frame = web_contents->GetMainFrame();
  content::RenderFrameHost* child = ChildFrameAt(main_frame, 0);
  content::RenderFrameHost* grandchild = ChildFrameAt(child, 0);

  // Navigate the bottom frame to a page that has a fullscreenable <div>.
  content::TestNavigationObserver observer(web_contents);
  EXPECT_TRUE(
      ExecuteScript(grandchild, "location.href = '/fullscreen_frame.html'"));
  observer.Wait();
  EXPECT_EQ(embedded_test_server()->GetURL("a.com", "/fullscreen_frame.html"),
            grandchild->GetLastCommittedURL());

  // Add allowFullscreen attribute to both <iframe> elements.
  SetAllowFullscreenForFrame(main_frame, "child-0");
  SetAllowFullscreenForFrame(child, "child-0");

  // Make fullscreenchange events in all three frames send a message.
  AddFullscreenChangeListener(main_frame, "main_frame");
  AddFullscreenChangeListener(child, "child");
  AddFullscreenChangeListener(grandchild, "grandchild");

  // Add a resize event handler that will send a message when the grandchild
  // frame is resized to the screen size.  Also save its original size.
  AddResizeListener(grandchild, GetScreenSize());
  gfx::Size original_grandchild_size = GetFrameSize(grandchild);

  // Fullscreen a <div> inside the bottom subframe.  This will block until
  // (1) the fullscreenchange events in all frames send a response, and
  // (2) the frame is resized to fill the whole screen.
  // (3) the browser has finished the fullscreen transition.
  std::set<std::string> expected_events = {"main_frame", "child", "grandchild"};
  {
    content::DOMMessageQueue queue;
    std::unique_ptr<FullscreenNotificationObserver> observer(
        new FullscreenNotificationObserver());
    EXPECT_TRUE(ExecuteScript(grandchild, "activateFullscreen()"));
    WaitForMultipleFullscreenEvents(expected_events, queue);
    observer->Wait();
  }

  // Verify that the browser has entered fullscreen for the current tab.
  EXPECT_TRUE(browser()->window()->IsFullscreen());
  EXPECT_TRUE(browser()->exclusive_access_manager()
                       ->fullscreen_controller()
                       ->IsFullscreenForTabOrPending(web_contents));

  // Verify that the <div> has fullscreen style in the bottom frame, and that
  // the proper <iframe> elements have fullscreen style in its ancestor frames.
  EXPECT_TRUE(ElementHasFullscreenStyle(grandchild, "fullscreen-div"));
  EXPECT_TRUE(ElementHasFullscreenStyle(child, "child-0"));
  EXPECT_TRUE(ElementHasFullscreenAncestorStyle(child, "child-0"));
  EXPECT_TRUE(ElementHasFullscreenStyle(main_frame, "child-0"));
  EXPECT_TRUE(ElementHasFullscreenAncestorStyle(main_frame, "child-0"));

  // Check document.webkitFullscreenElement in all frames.
  EXPECT_EQ("child-0", GetFullscreenElementId(main_frame));
  EXPECT_EQ("child-0", GetFullscreenElementId(child));
  EXPECT_EQ("fullscreen-div", GetFullscreenElementId(grandchild));

  // Now exit fullscreen from the subframe.
  AddResizeListener(grandchild, original_grandchild_size);
  {
    content::DOMMessageQueue queue;
    std::unique_ptr<FullscreenNotificationObserver> observer(
        new FullscreenNotificationObserver());
    switch (exit_method) {
      case FullscreenExitMethod::JS_CALL:
        EXPECT_TRUE(ExecuteScript(grandchild, "exitFullscreen()"));
        break;
      case FullscreenExitMethod::ESC_PRESS:
        ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
            browser(), ui::VKEY_ESCAPE, false, false, false, false));
        break;
      default:
        NOTREACHED();
    }
    WaitForMultipleFullscreenEvents(expected_events, queue);
    observer->Wait();
  }

  EXPECT_FALSE(browser()->window()->IsFullscreen());

  // Verify that the fullscreen styles were removed from the <div> and its
  // container <iframe>'s.
  EXPECT_FALSE(ElementHasFullscreenStyle(grandchild, "fullscreen-div"));
  EXPECT_FALSE(ElementHasFullscreenStyle(child, "child-0"));
  EXPECT_FALSE(ElementHasFullscreenAncestorStyle(child, "child-0"));
  EXPECT_FALSE(ElementHasFullscreenStyle(main_frame, "child-0"));
  EXPECT_FALSE(ElementHasFullscreenAncestorStyle(main_frame, "child-0"));

  // Check that document.webkitFullscreenElement was cleared in all three
  // frames.
  EXPECT_EQ("none", GetFullscreenElementId(main_frame));
  EXPECT_EQ("none", GetFullscreenElementId(child));
  EXPECT_EQ("none", GetFullscreenElementId(grandchild));
}

IN_PROC_BROWSER_TEST_F(SitePerProcessInteractiveBrowserTest,
                       FullscreenElementInABAAndExitViaEscapeKey) {
  FullscreenElementInABA(FullscreenExitMethod::ESC_PRESS);
}

IN_PROC_BROWSER_TEST_F(SitePerProcessInteractiveBrowserTest,
                       FullscreenElementInABAAndExitViaJS) {
  FullscreenElementInABA(FullscreenExitMethod::JS_CALL);
}

// Check that fullscreen works on a more complex page hierarchy with multiple
// local and remote ancestors.  The test uses this frame tree:
//
//             A (a_top)
//             |
//             A (a_bottom)
//            / \   .
// (b_first) B   B (b_second)
//               |
//               C (c_top)
//               |
//               C (c_middle) <- fullscreen target
//               |
//               C (c_bottom)
//
// The c_middle frame will trigger fullscreen for its <div> element.  The test
// verifies that its ancestor chain is properly updated for fullscreen, and
// that the b_first node that's not on the chain is not affected.
//
// The test also exits fullscreen by simulating pressing ESC rather than using
// document.webkitExitFullscreen(), which tests the browser-initiated
// fullscreen exit path.
IN_PROC_BROWSER_TEST_F(SitePerProcessInteractiveBrowserTest,
                       FullscreenElementInMultipleSubframes) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(a(b,b(c(c))))"));
  ui_test_utils::NavigateToURL(browser(), main_url);
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  content::RenderFrameHost* a_top = web_contents->GetMainFrame();
  content::RenderFrameHost* a_bottom = ChildFrameAt(a_top, 0);
  content::RenderFrameHost* b_first = ChildFrameAt(a_bottom, 0);
  content::RenderFrameHost* b_second = ChildFrameAt(a_bottom, 1);
  content::RenderFrameHost* c_top = ChildFrameAt(b_second, 0);
  content::RenderFrameHost* c_middle = ChildFrameAt(c_top, 0);

  // Allow fullscreen in all iframes descending to |c_middle|.  This relies on
  // IDs that cross_site_iframe_factory assigns to child frames.
  SetAllowFullscreenForFrame(a_top, "child-0");
  SetAllowFullscreenForFrame(a_bottom, "child-1");
  SetAllowFullscreenForFrame(b_second, "child-0");
  SetAllowFullscreenForFrame(c_top, "child-0");

  // Navigate |c_middle| to a page that has a fullscreenable <div> and another
  // frame.
  content::TestNavigationObserver observer(web_contents);
  EXPECT_TRUE(
      ExecuteScript(c_middle, "location.href = '/fullscreen_frame.html'"));
  observer.Wait();
  EXPECT_EQ(embedded_test_server()->GetURL("c.com", "/fullscreen_frame.html"),
            c_middle->GetLastCommittedURL());
  content::RenderFrameHost* c_bottom = ChildFrameAt(c_middle, 0);

  // Save the size of the frame to be fullscreened.
  gfx::Size c_middle_original_size = GetFrameSize(c_middle);

  // Add fullscreenchange and resize event handlers to all frames.
  AddFullscreenChangeListener(a_top, "a_top");
  AddFullscreenChangeListener(a_bottom, "a_bottom");
  AddFullscreenChangeListener(b_first, "b_first");
  AddFullscreenChangeListener(b_second, "b_second");
  AddFullscreenChangeListener(c_top, "c_top");
  AddFullscreenChangeListener(c_middle, "c_middle");
  AddFullscreenChangeListener(c_bottom, "c_bottom");
  AddResizeListener(c_middle, GetScreenSize());

  // Note that expected fullscreenchange events do NOT include |b_first| and
  // |c_bottom|, which aren't on the ancestor chain of |c_middle|.
  // WaitForMultipleFullscreenEvents() below will fail if it hears an
  // unexpected fullscreenchange from one of these frames.
  std::set<std::string> expected_events = {"a_top", "a_bottom", "b_second",
                                           "c_top", "c_middle"};

  // Fullscreen a <div> inside |c_middle|.  Block until (1) the
  // fullscreenchange events in |c_middle| and all its ancestors send a
  // response, (2) |c_middle| is resized to fill the whole screen, and (3) the
  // browser finishes the fullscreen transition.
  {
    content::DOMMessageQueue queue;
    std::unique_ptr<FullscreenNotificationObserver> observer(
        new FullscreenNotificationObserver());
    EXPECT_TRUE(ExecuteScript(c_middle, "activateFullscreen()"));
    WaitForMultipleFullscreenEvents(expected_events, queue);
    observer->Wait();
  }

  // Verify that the browser has entered fullscreen for the current tab.
  EXPECT_TRUE(browser()->window()->IsFullscreen());
  EXPECT_TRUE(browser()->exclusive_access_manager()
                       ->fullscreen_controller()
                       ->IsFullscreenForTabOrPending(web_contents));

  // Check document.webkitFullscreenElement.  It should point to corresponding
  // <iframe> element IDs on |c_middle|'s ancestor chain, and it should be null
  // in b_first and c_bottom.
  EXPECT_EQ("child-0", GetFullscreenElementId(a_top));
  EXPECT_EQ("child-1", GetFullscreenElementId(a_bottom));
  EXPECT_EQ("child-0", GetFullscreenElementId(b_second));
  EXPECT_EQ("child-0", GetFullscreenElementId(c_top));
  EXPECT_EQ("fullscreen-div", GetFullscreenElementId(c_middle));
  EXPECT_EQ("none", GetFullscreenElementId(b_first));
  EXPECT_EQ("none", GetFullscreenElementId(c_bottom));

  // Verify that the fullscreen element and all <iframe> elements on its
  // ancestor chain have fullscreen style, but other frames do not.
  EXPECT_TRUE(ElementHasFullscreenStyle(a_top, "child-0"));
  EXPECT_FALSE(ElementHasFullscreenStyle(a_bottom, "child-0"));
  EXPECT_TRUE(ElementHasFullscreenStyle(a_bottom, "child-1"));
  EXPECT_TRUE(ElementHasFullscreenStyle(b_second, "child-0"));
  EXPECT_TRUE(ElementHasFullscreenStyle(c_top, "child-0"));
  EXPECT_TRUE(ElementHasFullscreenStyle(c_middle, "fullscreen-div"));
  EXPECT_FALSE(ElementHasFullscreenStyle(c_middle, "child-0"));

  // Now exit fullscreen by pressing escape.  Wait for all fullscreenchange
  // events fired for fullscreen exit and verify that the bottom frame was
  // resized back to its original size.
  AddResizeListener(c_middle, c_middle_original_size);
  {
    content::DOMMessageQueue queue;
    std::unique_ptr<FullscreenNotificationObserver> observer(
        new FullscreenNotificationObserver());
    ASSERT_TRUE(ui_test_utils::SendKeyPressSync(browser(), ui::VKEY_ESCAPE,
                                                false, false, false, false));
    WaitForMultipleFullscreenEvents(expected_events, queue);
    observer->Wait();
  }

  EXPECT_FALSE(browser()->window()->IsFullscreen());

  // Check that document.webkitFullscreenElement has been cleared in all
  // frames.
  EXPECT_EQ("none", GetFullscreenElementId(a_top));
  EXPECT_EQ("none", GetFullscreenElementId(a_bottom));
  EXPECT_EQ("none", GetFullscreenElementId(b_first));
  EXPECT_EQ("none", GetFullscreenElementId(b_second));
  EXPECT_EQ("none", GetFullscreenElementId(c_top));
  EXPECT_EQ("none", GetFullscreenElementId(c_middle));
  EXPECT_EQ("none", GetFullscreenElementId(c_bottom));

  // Verify that all fullscreen styles have been cleared.
  EXPECT_FALSE(ElementHasFullscreenStyle(a_top, "child-0"));
  EXPECT_FALSE(ElementHasFullscreenStyle(a_bottom, "child-0"));
  EXPECT_FALSE(ElementHasFullscreenStyle(a_bottom, "child-1"));
  EXPECT_FALSE(ElementHasFullscreenStyle(b_second, "child-0"));
  EXPECT_FALSE(ElementHasFullscreenStyle(c_top, "child-0"));
  EXPECT_FALSE(ElementHasFullscreenStyle(c_middle, "fullscreen-div"));
  EXPECT_FALSE(ElementHasFullscreenStyle(c_middle, "child-0"));
}

// Test that deleting a RenderWidgetHost that holds the mouse lock won't cause a
// crash. https://crbug.com/619571.
IN_PROC_BROWSER_TEST_F(SitePerProcessInteractiveBrowserTest,
                       RenderWidgetHostDeletedWhileMouseLocked) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  ui_test_utils::NavigateToURL(browser(), main_url);

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  content::RenderFrameHost* main_frame = web_contents->GetMainFrame();
  content::RenderFrameHost* child = ChildFrameAt(main_frame, 0);

  EXPECT_TRUE(ExecuteScript(child, "document.body.requestPointerLock()"));
  bool mouse_locked = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(child,
                                          "window.domAutomationController.send("
                                          "document.body == "
                                          "document.pointerLockElement)",
                                          &mouse_locked));
  EXPECT_TRUE(mouse_locked);
  EXPECT_TRUE(main_frame->GetView()->IsMouseLocked());

  EXPECT_TRUE(ExecuteScript(main_frame,
                            "document.querySelector('iframe').parentNode."
                            "removeChild(document.querySelector('iframe'))"));
  EXPECT_FALSE(main_frame->GetView()->IsMouseLocked());
}

// Base test class for interactive tests which load and test PDF files.
class SitePerProcessInteractivePDFTest
    : public SitePerProcessInteractiveBrowserTest {
 public:
  SitePerProcessInteractivePDFTest() : test_guest_view_manager_(nullptr) {}
  ~SitePerProcessInteractivePDFTest() override {}

  void SetUpOnMainThread() override {
    SitePerProcessInteractiveBrowserTest::SetUpOnMainThread();
    guest_view::GuestViewManager::set_factory_for_testing(&factory_);
    test_guest_view_manager_ = static_cast<guest_view::TestGuestViewManager*>(
        guest_view::GuestViewManager::CreateWithDelegate(
            browser()->profile(),
            extensions::ExtensionsAPIClient::Get()
                ->CreateGuestViewManagerDelegate(browser()->profile())));
  }

 protected:
  guest_view::TestGuestViewManager* test_guest_view_manager() const {
    return test_guest_view_manager_;
  }

 private:
  guest_view::TestGuestViewManagerFactory factory_;
  guest_view::TestGuestViewManager* test_guest_view_manager_;

  DISALLOW_COPY_AND_ASSIGN(SitePerProcessInteractivePDFTest);
};

// This class observes a WebContents for a navigation to an extension scheme to
// finish.
class NavigationToExtensionSchemeObserver
    : public content::WebContentsObserver {
 public:
  explicit NavigationToExtensionSchemeObserver(content::WebContents* contents)
      : content::WebContentsObserver(contents),
        extension_loaded_(contents->GetLastCommittedURL().SchemeIs(
            extensions::kExtensionScheme)) {}

  void Wait() {
    if (extension_loaded_)
      return;
    message_loop_runner_ = new content::MessageLoopRunner();
    message_loop_runner_->Run();
  }

 private:
  void DidFinishNavigation(content::NavigationHandle* handle) override {
    if (!handle->GetURL().SchemeIs(extensions::kExtensionScheme) ||
        !handle->HasCommitted() || handle->IsErrorPage())
      return;
    extension_loaded_ = true;
    message_loop_runner_->Quit();
  }

  bool extension_loaded_;
  scoped_refptr<content::MessageLoopRunner> message_loop_runner_;

  DISALLOW_COPY_AND_ASSIGN(NavigationToExtensionSchemeObserver);
};

// This test loads a PDF inside an OOPIF and then verifies that context menu
// shows up at the correct position.
IN_PROC_BROWSER_TEST_F(SitePerProcessInteractivePDFTest,
                       ContextMenuPositionForEmbeddedPDFInCrossOriginFrame) {
  // Navigate to a page with an <iframe>.
  GURL main_url(embedded_test_server()->GetURL("a.com", "/iframe.html"));
  ui_test_utils::NavigateToURL(browser(), main_url);

  // Initially, no guests are created.
  EXPECT_EQ(0U, test_guest_view_manager()->num_guests_created());

  content::WebContents* active_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Change the position of the <iframe> inside the page.
  EXPECT_TRUE(ExecuteScript(active_web_contents,
                            "document.querySelector('iframe').style ="
                            " 'margin-left: 100px; margin-top: 100px;';"));

  // Navigate subframe to a cross-site page with an embedded PDF.
  GURL frame_url =
      embedded_test_server()->GetURL("b.com", "/page_with_embedded_pdf.html");

  // Ensure the page finishes loading without crashing.
  EXPECT_TRUE(NavigateIframeToURL(active_web_contents, "test", frame_url));

  // Wait until the guest contents for PDF is created.
  content::WebContents* guest_contents =
      test_guest_view_manager()->WaitForSingleGuestCreated();

  // Observe navigations in guest to find out when navigation to the (PDF)
  // extension commits. It will be used as an indicator that BrowserPlugin
  // has attached.
  NavigationToExtensionSchemeObserver navigation_observer(guest_contents);

  // Before sending the mouse clicks, we need to make sure the BrowserPlugin has
  // attached, which happens before navigating the guest to the PDF extension.
  // When attached, the window rects are updated and the context menu position
  // can be properly calculated.
  navigation_observer.Wait();

  content::RenderWidgetHostView* child_view =
      ChildFrameAt(active_web_contents->GetMainFrame(), 0)->GetView();

  ContextMenuWaiter menu_waiter(content::NotificationService::AllSources());

  // Declaring a lambda to send a right-button mouse event to the embedder
  // frame.
  auto send_right_mouse_event = [](content::RenderWidgetHost* host, int x,
                                   int y, blink::WebInputEvent::Type type) {
    blink::WebMouseEvent event;
    event.x = x;
    event.y = y;
    event.button = blink::WebMouseEvent::Button::Right;
    event.setType(type);
    host->ForwardMouseEvent(event);
  };

  send_right_mouse_event(child_view->GetRenderWidgetHost(), 10, 20,
                         blink::WebInputEvent::MouseDown);
  send_right_mouse_event(child_view->GetRenderWidgetHost(), 10, 20,
                         blink::WebInputEvent::MouseUp);
  menu_waiter.WaitForMenuOpenAndClose();

  gfx::Point point_in_root_window =
      child_view->TransformPointToRootCoordSpace(gfx::Point(10, 20));

  EXPECT_EQ(point_in_root_window.x(), menu_waiter.params().x);
  EXPECT_EQ(point_in_root_window.y(), menu_waiter.params().y);
}
