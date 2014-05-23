// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/singleton.h"
#include "base/run_loop.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "content/browser/browser_plugin/browser_plugin_embedder.h"
#include "content/browser/browser_plugin/browser_plugin_guest.h"
#include "content/browser/browser_plugin/browser_plugin_host_factory.h"
#include "content/browser/browser_plugin/test_browser_plugin_guest.h"
#include "content/browser/browser_plugin/test_guest_manager.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/browser_plugin/browser_plugin_messages.h"
#include "content/common/view_messages.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/drop_data.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/shell/browser/shell_browser_context.h"
#include "net/base/net_util.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/test/spawned_test_server/spawned_test_server.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"

using base::ASCIIToUTF16;
using blink::WebInputEvent;
using blink::WebMouseEvent;
using content::BrowserPluginEmbedder;
using content::BrowserPluginGuest;
using content::BrowserPluginHostFactory;
using content::WebContentsImpl;

const char kHTMLForGuest[] =
    "data:text/html,<html><body>hello world</body></html>";

const char kHTMLForGuestAcceptDrag[] =
    "data:text/html,<html><body>"
    "<script>"
    "function dropped() {"
    "  document.title = \"DROPPED\";"
    "}"
    "</script>"
    "<textarea id=\"text\" style=\"width:100%; height: 100%\""
    "    ondrop=\"dropped();\">"
    "</textarea>"
    "</body></html>";

namespace content {

class TestBrowserPluginEmbedder : public BrowserPluginEmbedder {
 public:
  TestBrowserPluginEmbedder(WebContentsImpl* web_contents)
      : BrowserPluginEmbedder(web_contents) {
  }

  virtual ~TestBrowserPluginEmbedder() {}

  WebContentsImpl* web_contents() const {
    return static_cast<WebContentsImpl*>(BrowserPluginEmbedder::web_contents());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestBrowserPluginEmbedder);
};

// Test factory for creating test instances of BrowserPluginEmbedder and
// BrowserPluginGuest.
class TestBrowserPluginHostFactory : public BrowserPluginHostFactory {
 public:
  virtual BrowserPluginGuest* CreateBrowserPluginGuest(
      int instance_id,
      WebContentsImpl* web_contents) OVERRIDE {
    return new TestBrowserPluginGuest(instance_id, web_contents);
  }

  // Also keeps track of number of instances created.
  virtual BrowserPluginEmbedder* CreateBrowserPluginEmbedder(
      WebContentsImpl* web_contents) OVERRIDE {

    return new TestBrowserPluginEmbedder(web_contents);
  }

  // Singleton getter.
  static TestBrowserPluginHostFactory* GetInstance() {
    return Singleton<TestBrowserPluginHostFactory>::get();
  }

 protected:
  TestBrowserPluginHostFactory() {}
  virtual ~TestBrowserPluginHostFactory() {}

 private:
  // For Singleton.
  friend struct DefaultSingletonTraits<TestBrowserPluginHostFactory>;

  scoped_refptr<MessageLoopRunner> message_loop_runner_;

  DISALLOW_COPY_AND_ASSIGN(TestBrowserPluginHostFactory);
};

class BrowserPluginHostTest : public ContentBrowserTest {
 public:
  BrowserPluginHostTest()
      : test_embedder_(NULL),
        test_guest_(NULL),
        test_guest_manager_(NULL) {}

  virtual void SetUp() OVERRIDE {
    // Override factory to create tests instances of BrowserPlugin*.
    BrowserPluginEmbedder::set_factory_for_testing(
        TestBrowserPluginHostFactory::GetInstance());
    BrowserPluginGuest::set_factory_for_testing(
        TestBrowserPluginHostFactory::GetInstance());
    ContentBrowserTest::SetUp();
  }
  virtual void TearDown() OVERRIDE {
    BrowserPluginEmbedder::set_factory_for_testing(NULL);
    BrowserPluginGuest::set_factory_for_testing(NULL);

    ContentBrowserTest::TearDown();
  }

  static void SimulateSpaceKeyPress(WebContents* web_contents) {
    SimulateKeyPress(web_contents,
                     ui::VKEY_SPACE,
                     false,   // control.
                     false,   // shift.
                     false,   // alt.
                     false);  // command.
  }

  static void SimulateTabKeyPress(WebContents* web_contents) {
    SimulateKeyPress(web_contents,
                     ui::VKEY_TAB,
                     false,   // control.
                     false,   // shift.
                     false,   // alt.
                     false);  // command.
  }

  // Executes the JavaScript synchronously and makes sure the returned value is
  // freed properly.
  void ExecuteSyncJSFunction(RenderFrameHost* rfh, const std::string& jscript) {
    scoped_ptr<base::Value> value =
        content::ExecuteScriptAndGetValue(rfh, jscript);
  }

  // This helper method does the following:
  // 1. Start the test server and navigate the shell to |embedder_url|.
  // 2. Execute custom pre-navigation |embedder_code| if provided.
  // 3. Navigate the guest to the |guest_url|.
  // 4. Verify that the guest has been created and has completed loading.
  void StartBrowserPluginTest(const std::string& embedder_url,
                              const std::string& guest_url,
                              bool is_guest_data_url,
                              const std::string& embedder_code) {
    ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());
    GURL test_url(embedded_test_server()->GetURL(embedder_url));
    NavigateToURL(shell(), test_url);

    WebContentsImpl* embedder_web_contents = static_cast<WebContentsImpl*>(
        shell()->web_contents());
    static_cast<ShellBrowserContext*>(
        embedder_web_contents->GetBrowserContext())->
            set_guest_manager_for_testing(
                TestGuestManager::GetInstance());
    RenderViewHostImpl* rvh = static_cast<RenderViewHostImpl*>(
        embedder_web_contents->GetRenderViewHost());
    RenderFrameHost* rfh = embedder_web_contents->GetMainFrame();
    // Focus the embedder.
    rvh->Focus();
    // Activative IME.
    rvh->SetInputMethodActive(true);

    // Allow the test to do some operations on the embedder before we perform
    // the first navigation of the guest.
    if (!embedder_code.empty())
      ExecuteSyncJSFunction(rfh, embedder_code);

    if (!is_guest_data_url) {
      test_url = embedded_test_server()->GetURL(guest_url);
      ExecuteSyncJSFunction(
          rfh, base::StringPrintf("SetSrc('%s');", test_url.spec().c_str()));
    } else {
      ExecuteSyncJSFunction(
          rfh, base::StringPrintf("SetSrc('%s');", guest_url.c_str()));
    }

    test_embedder_ = static_cast<TestBrowserPluginEmbedder*>(
        embedder_web_contents->GetBrowserPluginEmbedder());
    ASSERT_TRUE(test_embedder_);

    test_guest_manager_ =
        static_cast<TestGuestManager*>(
            embedder_web_contents->GetBrowserContext()->GetGuestManager());

    WebContentsImpl* test_guest_web_contents =
        test_guest_manager_->WaitForGuestAdded();

    test_guest_ = static_cast<TestBrowserPluginGuest*>(
        test_guest_web_contents->GetBrowserPluginGuest());
    test_guest_->WaitForLoadStop();
  }

  TestBrowserPluginEmbedder* test_embedder() const { return test_embedder_; }
  TestBrowserPluginGuest* test_guest() const { return test_guest_; }
  TestGuestManager* test_guest_manager() const {
    return test_guest_manager_;
  }

 private:
  TestBrowserPluginEmbedder* test_embedder_;
  TestBrowserPluginGuest* test_guest_;
  TestGuestManager* test_guest_manager_;
  DISALLOW_COPY_AND_ASSIGN(BrowserPluginHostTest);
};

// This test opens a page in http and then opens another page in https, forcing
// a RenderViewHost swap in the web_contents. We verify that the embedder in the
// web_contents gets cleared properly.
IN_PROC_BROWSER_TEST_F(BrowserPluginHostTest, EmbedderChangedAfterSwap) {
  net::SpawnedTestServer https_server(
      net::SpawnedTestServer::TYPE_HTTPS,
      net::SpawnedTestServer::kLocalhost,
      base::FilePath(FILE_PATH_LITERAL("content/test/data")));
  ASSERT_TRUE(https_server.Start());

  // 1. Load an embedder page with one guest in it.
  const char kEmbedderURL[] = "/browser_plugin_embedder.html";
  StartBrowserPluginTest(kEmbedderURL, kHTMLForGuest, true, std::string());

  // 2. Navigate to a URL in https, so we trigger a RenderViewHost swap.
  GURL test_https_url(https_server.GetURL(
      "files/browser_plugin_title_change.html"));
  content::WindowedNotificationObserver swap_observer(
      content::NOTIFICATION_RENDER_VIEW_HOST_CHANGED,
      content::Source<WebContents>(test_embedder()->web_contents()));
  NavigateToURL(shell(), test_https_url);
  swap_observer.Wait();

  TestBrowserPluginEmbedder* test_embedder_after_swap =
      static_cast<TestBrowserPluginEmbedder*>(
          static_cast<WebContentsImpl*>(shell()->web_contents())->
              GetBrowserPluginEmbedder());
  // Verify we have a no embedder in web_contents (since the new page doesn't
  // have any browser plugin).
  ASSERT_TRUE(!test_embedder_after_swap);
  ASSERT_NE(test_embedder(), test_embedder_after_swap);
}

// This test opens two pages in http and there is no RenderViewHost swap,
// therefore the embedder created on first page navigation stays the same in
// web_contents.
// Failing flakily on Windows: crbug.com/308405
#if defined(OS_WIN)
#define MAYBE_EmbedderSameAfterNav DISABLED_EmbedderSameAfterNav
#else
#define MAYBE_EmbedderSameAfterNav EmbedderSameAfterNav
#endif
IN_PROC_BROWSER_TEST_F(BrowserPluginHostTest, MAYBE_EmbedderSameAfterNav) {
  const char kEmbedderURL[] = "/browser_plugin_embedder.html";
  StartBrowserPluginTest(kEmbedderURL, kHTMLForGuest, true, std::string());
  WebContentsImpl* embedder_web_contents = test_embedder()->web_contents();

  // Navigate to another page in same host and port, so RenderViewHost swap
  // does not happen and existing embedder doesn't change in web_contents.
  GURL test_url_new(embedded_test_server()->GetURL(
      "/browser_plugin_title_change.html"));
  const base::string16 expected_title = ASCIIToUTF16("done");
  content::TitleWatcher title_watcher(shell()->web_contents(), expected_title);
  NavigateToURL(shell(), test_url_new);
  VLOG(0) << "Start waiting for title";
  base::string16 actual_title = title_watcher.WaitAndGetTitle();
  EXPECT_EQ(expected_title, actual_title);
  VLOG(0) << "Done navigating to second page";

  TestBrowserPluginEmbedder* test_embedder_after_nav =
      static_cast<TestBrowserPluginEmbedder*>(
          embedder_web_contents->GetBrowserPluginEmbedder());
  // Embedder must not change in web_contents.
  ASSERT_EQ(test_embedder_after_nav, test_embedder());
}

// This tests verifies that reloading the embedder does not crash the browser
// and that the guest is reset.
IN_PROC_BROWSER_TEST_F(BrowserPluginHostTest, DISABLED_ReloadEmbedder) {
  const char kEmbedderURL[] = "/browser_plugin_embedder.html";
  StartBrowserPluginTest(kEmbedderURL, kHTMLForGuest, true, std::string());
  RenderFrameHost* rfh = test_embedder()->web_contents()->GetMainFrame();

  // Change the title of the page to 'modified' so that we know that
  // the page has successfully reloaded when it goes back to 'embedder'
  // in the next step.
  {
    const base::string16 expected_title = ASCIIToUTF16("modified");
    content::TitleWatcher title_watcher(test_embedder()->web_contents(),
                                        expected_title);

    ExecuteSyncJSFunction(rfh,
                          base::StringPrintf("SetTitle('%s');", "modified"));

    base::string16 actual_title = title_watcher.WaitAndGetTitle();
    EXPECT_EQ(expected_title, actual_title);
  }

  // Reload the embedder page, and verify that the reload was successful.
  // Then navigate the guest to verify that the browser process does not crash.
  {
    const base::string16 expected_title = ASCIIToUTF16("embedder");
    content::TitleWatcher title_watcher(test_embedder()->web_contents(),
                                        expected_title);

    test_embedder()->web_contents()->GetController().Reload(false);
    base::string16 actual_title = title_watcher.WaitAndGetTitle();
    EXPECT_EQ(expected_title, actual_title);

    ExecuteSyncJSFunction(
        test_embedder()->web_contents()->GetMainFrame(),
        base::StringPrintf("SetSrc('%s');", kHTMLForGuest));

    WebContentsImpl* test_guest_web_contents = static_cast<WebContentsImpl*>(
        test_guest_manager()->WaitForGuestAdded());
    TestBrowserPluginGuest* new_test_guest =
        static_cast<TestBrowserPluginGuest*>(
          test_guest_web_contents->GetBrowserPluginGuest());
    ASSERT_TRUE(new_test_guest != NULL);

    // Wait for the guest to send an UpdateRectMsg, meaning it is ready.
    new_test_guest->WaitForUpdateRectMsg();
  }
}

// Always failing in the win7 try bot. See http://crbug.com/181107.
// Times out on the Mac. See http://crbug.com/297576.
#if defined(OS_WIN) || defined(OS_MACOSX)
#define MAYBE_AcceptDragEvents DISABLED_AcceptDragEvents
#else
#define MAYBE_AcceptDragEvents AcceptDragEvents
#endif

// Tests that a drag-n-drop over the browser plugin in the embedder happens
// correctly.
IN_PROC_BROWSER_TEST_F(BrowserPluginHostTest, MAYBE_AcceptDragEvents) {
  const char kEmbedderURL[] = "/browser_plugin_dragging.html";
  StartBrowserPluginTest(
      kEmbedderURL, kHTMLForGuestAcceptDrag, true, std::string());

  RenderViewHostImpl* rvh = static_cast<RenderViewHostImpl*>(
      test_embedder()->web_contents()->GetRenderViewHost());
  RenderFrameHost* rfh = test_embedder()->web_contents()->GetMainFrame();

  // Get a location in the embedder outside of the plugin.
  base::ListValue *start, *end;
  scoped_ptr<base::Value> value =
      content::ExecuteScriptAndGetValue(rfh, "dragLocation()");
  ASSERT_TRUE(value->GetAsList(&start) && start->GetSize() == 2);
  double start_x, start_y;
  ASSERT_TRUE(start->GetDouble(0, &start_x) && start->GetDouble(1, &start_y));

  // Get a location in the embedder that falls inside the plugin.
  value = content::ExecuteScriptAndGetValue(rfh, "dropLocation()");
  ASSERT_TRUE(value->GetAsList(&end) && end->GetSize() == 2);
  double end_x, end_y;
  ASSERT_TRUE(end->GetDouble(0, &end_x) && end->GetDouble(1, &end_y));

  DropData drop_data;
  GURL url = GURL("https://www.domain.com/index.html");
  drop_data.url = url;

  // Pretend that the URL is being dragged over the embedder. Start the drag
  // from outside the plugin, then move the drag inside the plugin and drop.
  // This should trigger appropriate messages from the embedder to the guest,
  // and end with a drop on the guest. The guest changes title when a drop
  // happens.
  const base::string16 expected_title = ASCIIToUTF16("DROPPED");
  content::TitleWatcher title_watcher(test_guest()->web_contents(),
      expected_title);

  rvh->DragTargetDragEnter(drop_data, gfx::Point(start_x, start_y),
      gfx::Point(start_x, start_y), blink::WebDragOperationEvery, 0);
  rvh->DragTargetDragOver(gfx::Point(end_x, end_y), gfx::Point(end_x, end_y),
      blink::WebDragOperationEvery, 0);
  rvh->DragTargetDrop(gfx::Point(end_x, end_y), gfx::Point(end_x, end_y), 0);

  base::string16 actual_title = title_watcher.WaitAndGetTitle();
  EXPECT_EQ(expected_title, actual_title);
}

// This test verifies that if IME is enabled in the embedder, it is also enabled
// in the guest.
IN_PROC_BROWSER_TEST_F(BrowserPluginHostTest, VerifyInputMethodActive) {
  const char* kEmbedderURL = "/browser_plugin_embedder.html";
  StartBrowserPluginTest(kEmbedderURL, kHTMLForGuest, true, std::string());
  RenderViewHostImpl* rvh = static_cast<RenderViewHostImpl*>(
      test_guest()->web_contents()->GetRenderViewHost());
  EXPECT_TRUE(rvh->input_method_active());
}

// This test exercises the following scenario:
// 1. An <input> in guest has focus.
// 2. User takes focus to embedder by clicking e.g. an <input> in embedder.
// 3. User brings back the focus directly to the <input> in #1.
//
// Now we need to make sure TextInputTypeChange fires properly for the guest's
// view (RenderWidgetHostViewGuest) upon step #3. This test ensures that,
// otherwise IME doesn't work after step #3.
IN_PROC_BROWSER_TEST_F(BrowserPluginHostTest, FocusRestored) {
  const char kEmbedderURL[] = "/browser_plugin_embedder.html";
  const char kGuestHTML[] = "data:text/html,"
      "<html><body><input id=\"input1\"></body>"
      "<script>"
      "var i = document.getElementById(\"input1\");"
      "document.body.addEventListener(\"click\", function(e) {"
      "  i.focus();"
      "});"
      "i.addEventListener(\"focus\", function(e) {"
      "  document.title = \"FOCUS\";"
      "});"
      "i.addEventListener(\"blur\", function(e) {"
      "  document.title = \"BLUR\";"
      "});"
      "</script>"
      "</html>";
  StartBrowserPluginTest(kEmbedderURL, kGuestHTML, true,
                         "document.getElementById(\"plugin\").focus();");

  ASSERT_TRUE(test_embedder());
  const char *kTitles[3] = {"FOCUS", "BLUR", "FOCUS"};
  gfx::Point kClickPoints[3] = {
    gfx::Point(20, 20), gfx::Point(700, 20), gfx::Point(20, 20)
  };

  for (int i = 0; i < 3; ++i) {
    base::string16 expected_title = base::UTF8ToUTF16(kTitles[i]);
    content::TitleWatcher title_watcher(test_guest()->web_contents(),
                                        expected_title);
    SimulateMouseClickAt(test_embedder()->web_contents(), 0,
        blink::WebMouseEvent::ButtonLeft,
        kClickPoints[i]);
    base::string16 actual_title = title_watcher.WaitAndGetTitle();
    EXPECT_EQ(expected_title, actual_title);
  }
  TestBrowserPluginGuest* guest = test_guest();
  ASSERT_TRUE(guest);
  ui::TextInputType text_input_type = guest->last_text_input_type();
  ASSERT_TRUE(text_input_type != ui::TEXT_INPUT_TYPE_NONE);
}

// Tests input method.
IN_PROC_BROWSER_TEST_F(BrowserPluginHostTest, InputMethod) {
  const char kEmbedderURL[] = "/browser_plugin_embedder.html";
  const char kGuestHTML[] = "data:text/html,"
      "<html><body><input id=\"input1\">"
      "<input id=\"input2\"></body>"
      "<script>"
      "var i = document.getElementById(\"input1\");"
      "i.oninput = function() {"
      "  document.title = i.value;"
      "}"
      "</script>"
      "</html>";
  StartBrowserPluginTest(kEmbedderURL, kGuestHTML, true,
                         "document.getElementById(\"plugin\").focus();");

  RenderViewHostImpl* embedder_rvh = static_cast<RenderViewHostImpl*>(
      test_embedder()->web_contents()->GetRenderViewHost());
  RenderFrameHost* guest_rfh = test_guest()->web_contents()->GetMainFrame();

  std::vector<blink::WebCompositionUnderline> underlines;

  // An input field in browser plugin guest gets focus and given some user
  // input via IME.
  {
    ExecuteSyncJSFunction(guest_rfh,
                          "document.getElementById('input1').focus();");
    base::string16 expected_title = base::UTF8ToUTF16("InputTest123");
    content::TitleWatcher title_watcher(test_guest()->web_contents(),
                                        expected_title);
    embedder_rvh->Send(
        new ViewMsg_ImeSetComposition(
            test_embedder()->web_contents()->GetRoutingID(),
            expected_title,
            underlines,
            12, 12));
    base::string16 actual_title = title_watcher.WaitAndGetTitle();
    EXPECT_EQ(expected_title, actual_title);
  }
  // A composition is committed via IME.
  {
    base::string16 expected_title = base::UTF8ToUTF16("InputTest456");
    content::TitleWatcher title_watcher(test_guest()->web_contents(),
                                        expected_title);
    embedder_rvh->Send(
        new ViewMsg_ImeConfirmComposition(
            test_embedder()->web_contents()->GetRoutingID(),
            expected_title,
            gfx::Range(),
            true));
    base::string16 actual_title = title_watcher.WaitAndGetTitle();
    EXPECT_EQ(expected_title, actual_title);
  }
  // IME composition starts, but focus moves out, then the composition will
  // be committed and get cancel msg.
  {
    ExecuteSyncJSFunction(guest_rfh,
                          "document.getElementById('input1').value = '';");
    base::string16 composition = base::UTF8ToUTF16("InputTest789");
    content::TitleWatcher title_watcher(test_guest()->web_contents(),
                                        composition);
    embedder_rvh->Send(
        new ViewMsg_ImeSetComposition(
            test_embedder()->web_contents()->GetRoutingID(),
            composition,
            underlines,
            12, 12));
    base::string16 actual_title = title_watcher.WaitAndGetTitle();
    EXPECT_EQ(composition, actual_title);
    // Moving focus causes IME cancel, and the composition will be committed
    // in input1, not in input2.
    ExecuteSyncJSFunction(guest_rfh,
                          "document.getElementById('input2').focus();");
    test_guest()->WaitForImeCancel();
    scoped_ptr<base::Value> value =
        content::ExecuteScriptAndGetValue(
            guest_rfh, "document.getElementById('input1').value");
    std::string result;
    ASSERT_TRUE(value->GetAsString(&result));
    EXPECT_EQ(base::UTF16ToUTF8(composition), result);
  }
  // Tests ExtendSelectionAndDelete message works in browser_plugin.
  {
    // Set 'InputTestABC' in input1 and put caret at 6 (after 'T').
    ExecuteSyncJSFunction(guest_rfh,
                          "var i = document.getElementById('input1');"
                          "i.focus();"
                          "i.value = 'InputTestABC';"
                          "i.selectionStart=6;"
                          "i.selectionEnd=6;");
    base::string16 expected_value = base::UTF8ToUTF16("InputABC");
    content::TitleWatcher title_watcher(test_guest()->web_contents(),
                                        expected_value);
    // Delete 'Test' in 'InputTestABC', as the caret is after 'T':
    // delete before 1 character ('T') and after 3 characters ('est').
    RenderFrameHostImpl* rfh = static_cast<RenderFrameHostImpl*>(
        test_embedder()->web_contents()->GetFocusedFrame());
    rfh->ExtendSelectionAndDelete(1, 3);
    base::string16 actual_title = title_watcher.WaitAndGetTitle();
    EXPECT_EQ(expected_value, actual_title);
    scoped_ptr<base::Value> value =
        content::ExecuteScriptAndGetValue(
            guest_rfh, "document.getElementById('input1').value");
    std::string actual_value;
    ASSERT_TRUE(value->GetAsString(&actual_value));
    EXPECT_EQ(base::UTF16ToUTF8(expected_value), actual_value);
  }
}

}  // namespace content
