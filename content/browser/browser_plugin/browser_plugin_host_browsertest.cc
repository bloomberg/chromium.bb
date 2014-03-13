// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/memory/singleton.h"
#include "base/run_loop.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/test_timeouts.h"
#include "content/browser/browser_plugin/browser_plugin_embedder.h"
#include "content/browser/browser_plugin/browser_plugin_guest.h"
#include "content/browser/browser_plugin/browser_plugin_host_factory.h"
#include "content/browser/browser_plugin/test_browser_plugin_guest.h"
#include "content/browser/browser_plugin/test_browser_plugin_guest_delegate.h"
#include "content/browser/browser_plugin/test_browser_plugin_guest_manager.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/frame_host/render_widget_host_view_guest.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/browser_plugin/browser_plugin_messages.h"
#include "content/common/view_messages.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/drop_data.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/test/content_browser_test.h"
#include "content/test/content_browser_test_utils.h"
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

const char kHTMLForGuestTouchHandler[] =
    "data:text/html,<html><body><div id=\"touch\">With touch</div></body>"
    "<script type=\"text/javascript\">"
    "function handler() {}"
    "function InstallTouchHandler() { "
    "  document.getElementById(\"touch\").addEventListener(\"touchstart\", "
    "     handler);"
    "}"
    "function UninstallTouchHandler() { "
    "  document.getElementById(\"touch\").removeEventListener(\"touchstart\", "
    "     handler);"
    "}"
    "</script></html>";

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
  virtual BrowserPluginGuestManager*
      CreateBrowserPluginGuestManager() OVERRIDE {
    guest_manager_instance_count_++;
    if (message_loop_runner_.get())
      message_loop_runner_->Quit();
    return new TestBrowserPluginGuestManager();
  }

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

  // Waits for at least one embedder to be created in the test. Returns true if
  // we have a guest, false if waiting times out.
  void WaitForGuestManagerCreation() {
    // Check if already have created an instance.
    if (guest_manager_instance_count_ > 0)
      return;
    // Wait otherwise.
    message_loop_runner_ = new MessageLoopRunner();
    message_loop_runner_->Run();
  }

 protected:
  TestBrowserPluginHostFactory() : guest_manager_instance_count_(0) {}
  virtual ~TestBrowserPluginHostFactory() {}

 private:
  // For Singleton.
  friend struct DefaultSingletonTraits<TestBrowserPluginHostFactory>;

  scoped_refptr<MessageLoopRunner> message_loop_runner_;
  int guest_manager_instance_count_;

  DISALLOW_COPY_AND_ASSIGN(TestBrowserPluginHostFactory);
};

// Test factory class for browser plugin that creates guests with short hang
// timeout.
class TestShortHangTimeoutGuestFactory : public TestBrowserPluginHostFactory {
 public:
  virtual BrowserPluginGuest* CreateBrowserPluginGuest(
      int instance_id, WebContentsImpl* web_contents) OVERRIDE {
    TestBrowserPluginGuest* guest =
        new TestBrowserPluginGuest(instance_id, web_contents);
    guest->set_guest_hang_timeout(TestTimeouts::tiny_timeout());
    return guest;
  }

  // Singleton getter.
  static TestShortHangTimeoutGuestFactory* GetInstance() {
    return Singleton<TestShortHangTimeoutGuestFactory>::get();
  }

 protected:
  TestShortHangTimeoutGuestFactory() {}
  virtual ~TestShortHangTimeoutGuestFactory() {}

 private:
  // For Singleton.
  friend struct DefaultSingletonTraits<TestShortHangTimeoutGuestFactory>;

  DISALLOW_COPY_AND_ASSIGN(TestShortHangTimeoutGuestFactory);
};

// A transparent observer that can be used to verify that a RenderViewHost
// received a specific message.
class MessageObserver : public WebContentsObserver {
 public:
  MessageObserver(WebContents* web_contents, uint32 message_id)
      : WebContentsObserver(web_contents),
        message_id_(message_id),
        message_received_(false) {
  }

  virtual ~MessageObserver() {}

  void WaitUntilMessageReceived() {
    if (message_received_)
      return;
    message_loop_runner_ = new MessageLoopRunner();
    message_loop_runner_->Run();
  }

  void ResetState() {
    message_received_ = false;
  }

  // IPC::Listener implementation.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE {
    if (message.type() == message_id_) {
      message_received_ = true;
      if (message_loop_runner_.get())
        message_loop_runner_->Quit();
    }
    return false;
  }

 private:
  scoped_refptr<MessageLoopRunner> message_loop_runner_;
  uint32 message_id_;
  bool message_received_;

  DISALLOW_COPY_AND_ASSIGN(MessageObserver);
};

class BrowserPluginHostTest : public ContentBrowserTest {
 public:
  BrowserPluginHostTest()
      : test_embedder_(NULL),
        test_guest_(NULL),
        test_guest_manager_(NULL) {}

  virtual void SetUp() OVERRIDE {
    // Override factory to create tests instances of BrowserPlugin*.
    content::BrowserPluginEmbedder::set_factory_for_testing(
        TestBrowserPluginHostFactory::GetInstance());
    content::BrowserPluginGuest::set_factory_for_testing(
        TestBrowserPluginHostFactory::GetInstance());
    content::BrowserPluginGuestManager::set_factory_for_testing(
        TestBrowserPluginHostFactory::GetInstance());
    ContentBrowserTest::SetUp();
  }
  virtual void TearDown() OVERRIDE {
    content::BrowserPluginEmbedder::set_factory_for_testing(NULL);
    content::BrowserPluginGuest::set_factory_for_testing(NULL);

    ContentBrowserTest::TearDown();
  }

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    // Enable browser plugin in content_shell for running test.
    command_line->AppendSwitch(switches::kEnableBrowserPluginForAllViewTypes);
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

  // Executes the javascript synchronously and makes sure the returned value is
  // freed properly.
  void ExecuteSyncJSFunction(RenderViewHost* rvh, const std::string& jscript) {
    scoped_ptr<base::Value> value =
        content::ExecuteScriptAndGetValue(rvh, jscript);
  }

  bool IsAttributeNull(RenderViewHost* rvh, const std::string& attribute) {
    scoped_ptr<base::Value> value = content::ExecuteScriptAndGetValue(rvh,
        "document.getElementById('plugin').getAttribute('" + attribute + "');");
    return value->GetType() == base::Value::TYPE_NULL;
  }

  // Removes all attributes in the comma-delimited string |attributes|.
  void RemoveAttributes(RenderViewHost* rvh, const std::string& attributes) {
    std::vector<std::string> attributes_list;
    base::SplitString(attributes, ',', &attributes_list);
    std::vector<std::string>::const_iterator itr;
    for (itr = attributes_list.begin(); itr != attributes_list.end(); ++itr) {
      ExecuteSyncJSFunction(rvh, "document.getElementById('plugin')"
                                 "." + *itr + " = null;");
    }
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
    RenderViewHostImpl* rvh = static_cast<RenderViewHostImpl*>(
        embedder_web_contents->GetRenderViewHost());
    // Focus the embedder.
    rvh->Focus();
    // Activative IME.
    rvh->SetInputMethodActive(true);

    // Allow the test to do some operations on the embedder before we perform
    // the first navigation of the guest.
    if (!embedder_code.empty())
      ExecuteSyncJSFunction(rvh, embedder_code);

    if (!is_guest_data_url) {
      test_url = embedded_test_server()->GetURL(guest_url);
      ExecuteSyncJSFunction(
          rvh, base::StringPrintf("SetSrc('%s');", test_url.spec().c_str()));
    } else {
      ExecuteSyncJSFunction(
          rvh, base::StringPrintf("SetSrc('%s');", guest_url.c_str()));
    }

    // Wait to make sure embedder is created/attached to WebContents.
    TestBrowserPluginHostFactory::GetInstance()->WaitForGuestManagerCreation();

    test_embedder_ = static_cast<TestBrowserPluginEmbedder*>(
        embedder_web_contents->GetBrowserPluginEmbedder());
    ASSERT_TRUE(test_embedder_);

    test_guest_manager_ = static_cast<TestBrowserPluginGuestManager*>(
        embedder_web_contents->GetBrowserPluginGuestManager());
    ASSERT_TRUE(test_guest_manager_);

    test_guest_manager_->WaitForGuestAdded();

    // Verify that we have exactly one guest.
    const TestBrowserPluginGuestManager::GuestInstanceMap& instance_map =
        test_guest_manager_->guest_web_contents_for_testing();
    EXPECT_EQ(1u, instance_map.size());

    WebContentsImpl* test_guest_web_contents = static_cast<WebContentsImpl*>(
        instance_map.begin()->second);
    test_guest_ = static_cast<TestBrowserPluginGuest*>(
        test_guest_web_contents->GetBrowserPluginGuest());
    test_guest_->WaitForLoadStop();
  }

  TestBrowserPluginEmbedder* test_embedder() const { return test_embedder_; }
  TestBrowserPluginGuest* test_guest() const { return test_guest_; }
  TestBrowserPluginGuestManager* test_guest_manager() const {
    return test_guest_manager_;
  }

 private:
  TestBrowserPluginEmbedder* test_embedder_;
  TestBrowserPluginGuest* test_guest_;
  TestBrowserPluginGuestManager* test_guest_manager_;
  DISALLOW_COPY_AND_ASSIGN(BrowserPluginHostTest);
};

// This test ensures that if guest isn't there and we resize the guest (from
// js), it remembers the size correctly.
//
// Initially we load an embedder with a guest without a src attribute (which has
// dimension 640x480), resize it to 100x200, and then we set the source to a
// sample guest. In the end we verify that the correct size has been set.
IN_PROC_BROWSER_TEST_F(BrowserPluginHostTest, NavigateAfterResize) {
  const gfx::Size nxt_size = gfx::Size(100, 200);
  const std::string embedder_code = base::StringPrintf(
      "SetSize(%d, %d);", nxt_size.width(), nxt_size.height());
  const char kEmbedderURL[] = "/browser_plugin_embedder.html";
  StartBrowserPluginTest(kEmbedderURL, kHTMLForGuest, true, embedder_code);

  // Wait for the guest to receive a damage buffer of size 100x200.
  // This means the guest will be painted properly at that size.
  test_guest()->WaitForDamageBufferWithSize(nxt_size);
}

IN_PROC_BROWSER_TEST_F(BrowserPluginHostTest, AdvanceFocus) {
  const char kEmbedderURL[] = "/browser_plugin_focus.html";
  const char* kGuestURL = "/browser_plugin_focus_child.html";
  StartBrowserPluginTest(kEmbedderURL, kGuestURL, false, std::string());

  SimulateMouseClick(test_embedder()->web_contents(), 0,
      blink::WebMouseEvent::ButtonLeft);
  BrowserPluginHostTest::SimulateTabKeyPress(test_embedder()->web_contents());
  // Wait until we focus into the guest.
  test_guest()->WaitForFocus();

  // TODO(fsamuel): A third Tab key press should not be necessary.
  // The browser plugin will take keyboard focus but it will not
  // focus an initial element. The initial element is dependent
  // upon tab direction which WebKit does not propagate to the plugin.
  // See http://crbug.com/147644.
  BrowserPluginHostTest::SimulateTabKeyPress(test_embedder()->web_contents());
  BrowserPluginHostTest::SimulateTabKeyPress(test_embedder()->web_contents());
  BrowserPluginHostTest::SimulateTabKeyPress(test_embedder()->web_contents());
  test_guest()->WaitForAdvanceFocus();
}

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

// This test verifies that hiding the embedder also hides the guest.
IN_PROC_BROWSER_TEST_F(BrowserPluginHostTest, BrowserPluginVisibilityChanged) {
  const char kEmbedderURL[] = "/browser_plugin_embedder.html";
  StartBrowserPluginTest(kEmbedderURL, kHTMLForGuest, true, std::string());

  // Hide the Browser Plugin.
  RenderViewHostImpl* rvh = static_cast<RenderViewHostImpl*>(
      test_embedder()->web_contents()->GetRenderViewHost());
  ExecuteSyncJSFunction(
      rvh, "document.getElementById('plugin').style.visibility = 'hidden'");

  // Make sure that the guest is hidden.
  test_guest()->WaitUntilHidden();
}

IN_PROC_BROWSER_TEST_F(BrowserPluginHostTest, EmbedderVisibilityChanged) {
  const char kEmbedderURL[] = "/browser_plugin_embedder.html";
  StartBrowserPluginTest(kEmbedderURL, kHTMLForGuest, true, std::string());

  // Hide the embedder.
  test_embedder()->web_contents()->WasHidden();

  // Make sure that hiding the embedder also hides the guest.
  test_guest()->WaitUntilHidden();
}

// Verifies that installing/uninstalling touch-event handlers in the guest
// plugin correctly updates the touch-event handling state in the embedder.
IN_PROC_BROWSER_TEST_F(BrowserPluginHostTest, AcceptTouchEvents) {
  const char kEmbedderURL[] = "/browser_plugin_embedder.html";
  StartBrowserPluginTest(
      kEmbedderURL, kHTMLForGuestTouchHandler, true, std::string());

  RenderViewHostImpl* rvh = static_cast<RenderViewHostImpl*>(
      test_embedder()->web_contents()->GetRenderViewHost());
  // The embedder should not have any touch event handlers at this point.
  EXPECT_FALSE(rvh->has_touch_handler());

  // Install the touch handler in the guest. This should cause the embedder to
  // start listening for touch events too.
  MessageObserver observer(test_embedder()->web_contents(),
                           ViewHostMsg_HasTouchEventHandlers::ID);
  ExecuteSyncJSFunction(test_guest()->web_contents()->GetRenderViewHost(),
                        "InstallTouchHandler();");
  observer.WaitUntilMessageReceived();
  EXPECT_TRUE(rvh->has_touch_handler());

  // Uninstalling the touch-handler in guest should cause the embedder to stop
  // listening for touch events.
  observer.ResetState();
  ExecuteSyncJSFunction(test_guest()->web_contents()->GetRenderViewHost(),
                        "UninstallTouchHandler();");
  observer.WaitUntilMessageReceived();
  EXPECT_FALSE(rvh->has_touch_handler());
}

// This tests verifies that reloading the embedder does not crash the browser
// and that the guest is reset.
IN_PROC_BROWSER_TEST_F(BrowserPluginHostTest, DISABLED_ReloadEmbedder) {
  const char kEmbedderURL[] = "/browser_plugin_embedder.html";
  StartBrowserPluginTest(kEmbedderURL, kHTMLForGuest, true, std::string());
  RenderViewHostImpl* rvh = static_cast<RenderViewHostImpl*>(
      test_embedder()->web_contents()->GetRenderViewHost());

  // Change the title of the page to 'modified' so that we know that
  // the page has successfully reloaded when it goes back to 'embedder'
  // in the next step.
  {
    const base::string16 expected_title = ASCIIToUTF16("modified");
    content::TitleWatcher title_watcher(test_embedder()->web_contents(),
                                        expected_title);

    ExecuteSyncJSFunction(rvh,
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
        test_embedder()->web_contents()->GetRenderViewHost(),
        base::StringPrintf("SetSrc('%s');", kHTMLForGuest));
    test_guest_manager()->WaitForGuestAdded();

    const TestBrowserPluginGuestManager::GuestInstanceMap& instance_map =
        test_guest_manager()->guest_web_contents_for_testing();
    WebContentsImpl* test_guest_web_contents = static_cast<WebContentsImpl*>(
        instance_map.begin()->second);
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

  // Get a location in the embedder outside of the plugin.
  base::ListValue *start, *end;
  scoped_ptr<base::Value> value =
      content::ExecuteScriptAndGetValue(rvh, "dragLocation()");
  ASSERT_TRUE(value->GetAsList(&start) && start->GetSize() == 2);
  double start_x, start_y;
  ASSERT_TRUE(start->GetDouble(0, &start_x) && start->GetDouble(1, &start_y));

  // Get a location in the embedder that falls inside the plugin.
  value = content::ExecuteScriptAndGetValue(rvh, "dropLocation()");
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

// This test verifies that round trip postMessage works as expected.
// 1. The embedder posts a message 'testing123' to the guest.
// 2. The guest receives and replies to the message using the event object's
// source object: event.source.postMessage('foobar', '*')
// 3. The embedder receives the message and uses the event's source
// object to do one final reply: 'stop'
// 4. The guest receives the final 'stop' message.
// 5. The guest acks the 'stop' message with a 'stop_ack' message.
// 6. The embedder changes its title to 'main guest' when it sees the 'stop_ack'
// message.
IN_PROC_BROWSER_TEST_F(BrowserPluginHostTest, PostMessage) {
  const char* kTesting = "testing123";
  const char* kEmbedderURL = "/browser_plugin_embedder.html";
  const char* kGuestURL = "/browser_plugin_post_message_guest.html";
  StartBrowserPluginTest(kEmbedderURL, kGuestURL, false, std::string());
  RenderViewHostImpl* rvh = static_cast<RenderViewHostImpl*>(
      test_embedder()->web_contents()->GetRenderViewHost());
  {
    const base::string16 expected_title = ASCIIToUTF16("main guest");
    content::TitleWatcher title_watcher(test_embedder()->web_contents(),
                                        expected_title);

    // By the time we get here 'contentWindow' should be ready because the
    // guest has completed loading.
    ExecuteSyncJSFunction(
        rvh, base::StringPrintf("PostMessage('%s, false');", kTesting));

    // The title will be updated to "main guest" at the last stage of the
    // process described above.
    base::string16 actual_title = title_watcher.WaitAndGetTitle();
    EXPECT_EQ(expected_title, actual_title);
  }
}

// This is the same as BrowserPluginHostTest.PostMessage but also
// posts a message to an iframe.
// TODO(fsamuel): This test should replace the previous test once postMessage
// iframe targeting is fixed (see http://crbug.com/153701).
IN_PROC_BROWSER_TEST_F(BrowserPluginHostTest, DISABLED_PostMessageToIFrame) {
  const char* kTesting = "testing123";
  const char* kEmbedderURL = "/browser_plugin_embedder.html";
  const char* kGuestURL = "/browser_plugin_post_message_guest.html";
  StartBrowserPluginTest(kEmbedderURL, kGuestURL, false, std::string());
  RenderViewHostImpl* rvh = static_cast<RenderViewHostImpl*>(
      test_embedder()->web_contents()->GetRenderViewHost());
  {
    const base::string16 expected_title = ASCIIToUTF16("main guest");
    content::TitleWatcher title_watcher(test_embedder()->web_contents(),
                                        expected_title);

    ExecuteSyncJSFunction(
        rvh, base::StringPrintf("PostMessage('%s, false');", kTesting));

    // The title will be updated to "main guest" at the last stage of the
    // process described above.
    base::string16 actual_title = title_watcher.WaitAndGetTitle();
    EXPECT_EQ(expected_title, actual_title);
  }
  {
    content::TitleWatcher ready_watcher(test_embedder()->web_contents(),
                                        ASCIIToUTF16("ready"));

    RenderViewHostImpl* guest_rvh = static_cast<RenderViewHostImpl*>(
        test_guest()->web_contents()->GetRenderViewHost());
    GURL test_url = embedded_test_server()->GetURL(
        "/browser_plugin_post_message_guest.html");
    ExecuteSyncJSFunction(
        guest_rvh,
        base::StringPrintf(
            "CreateChildFrame('%s');", test_url.spec().c_str()));

    base::string16 actual_title = ready_watcher.WaitAndGetTitle();
    EXPECT_EQ(ASCIIToUTF16("ready"), actual_title);

    content::TitleWatcher iframe_watcher(test_embedder()->web_contents(),
                                        ASCIIToUTF16("iframe"));
    ExecuteSyncJSFunction(
        rvh, base::StringPrintf("PostMessage('%s', true);", kTesting));

    // The title will be updated to "iframe" at the last stage of the
    // process described above.
    actual_title = iframe_watcher.WaitAndGetTitle();
    EXPECT_EQ(ASCIIToUTF16("iframe"), actual_title);
  }
}

// This test verifies that if a browser plugin is hidden before navigation,
// the guest starts off hidden.
IN_PROC_BROWSER_TEST_F(BrowserPluginHostTest, HiddenBeforeNavigation) {
  const char* kEmbedderURL = "/browser_plugin_embedder.html";
  const std::string embedder_code =
      "document.getElementById('plugin').style.visibility = 'hidden'";
  StartBrowserPluginTest(
      kEmbedderURL, kHTMLForGuest, true, embedder_code);
  EXPECT_FALSE(test_guest()->visible());
}

// This test verifies that if a browser plugin is focused before navigation then
// the guest starts off focused.
IN_PROC_BROWSER_TEST_F(BrowserPluginHostTest, FocusBeforeNavigation) {
  const char* kEmbedderURL = "/browser_plugin_embedder.html";
  const std::string embedder_code =
      "document.getElementById('plugin').focus();";
  StartBrowserPluginTest(
      kEmbedderURL, kHTMLForGuest, true, embedder_code);
  RenderViewHostImpl* guest_rvh = static_cast<RenderViewHostImpl*>(
      test_guest()->web_contents()->GetRenderViewHost());
  // Verify that the guest is focused.
  scoped_ptr<base::Value> value =
      content::ExecuteScriptAndGetValue(guest_rvh, "document.hasFocus()");
  bool result = false;
  ASSERT_TRUE(value->GetAsBoolean(&result));
  EXPECT_TRUE(result);
}

IN_PROC_BROWSER_TEST_F(BrowserPluginHostTest, FocusTracksEmbedder) {
  const char* kEmbedderURL = "/browser_plugin_embedder.html";
  StartBrowserPluginTest(kEmbedderURL, kHTMLForGuest, true, std::string());
  // Blur the embedder.
  test_embedder()->web_contents()->GetRenderViewHost()->Blur();
  // Ensure that the guest is also blurred.
  test_guest()->WaitForBlur();
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

// Verify that navigating to an invalid URL (e.g. 'http:') doesn't cause
// a crash.
IN_PROC_BROWSER_TEST_F(BrowserPluginHostTest, DoNotCrashOnInvalidNavigation) {
  const char kEmbedderURL[] = "/browser_plugin_embedder.html";
  StartBrowserPluginTest(kEmbedderURL, kHTMLForGuest, true, std::string());
  TestBrowserPluginGuestDelegate* delegate =
      new TestBrowserPluginGuestDelegate();
  test_guest()->SetDelegate(delegate);

  const char kValidSchemeWithEmptyURL[] = "http:";
  ExecuteSyncJSFunction(
      test_embedder()->web_contents()->GetRenderViewHost(),
      base::StringPrintf("SetSrc('%s');", kValidSchemeWithEmptyURL));
  EXPECT_TRUE(delegate->load_aborted());
  EXPECT_FALSE(delegate->load_aborted_url().is_valid());
  EXPECT_EQ(kValidSchemeWithEmptyURL,
            delegate->load_aborted_url().possibly_invalid_spec());

  delegate->ResetStates();

  // Attempt a navigation to chrome-guest://abc123, which is a valid URL. But it
  // should be blocked because the scheme isn't web-safe or a pseudo-scheme.
  ExecuteSyncJSFunction(
      test_embedder()->web_contents()->GetRenderViewHost(),
      base::StringPrintf("SetSrc('%s://abc123');", kGuestScheme));
  EXPECT_TRUE(delegate->load_aborted());
  EXPECT_TRUE(delegate->load_aborted_url().is_valid());
}

// Tests involving the threaded compositor.
class BrowserPluginThreadedCompositorTest : public BrowserPluginHostTest {
 public:
  BrowserPluginThreadedCompositorTest() {}
  virtual ~BrowserPluginThreadedCompositorTest() {}

 protected:
  virtual void SetUpCommandLine(CommandLine* cmd) OVERRIDE {
    BrowserPluginHostTest::SetUpCommandLine(cmd);
    cmd->AppendSwitch(switches::kEnableThreadedCompositing);
  }
};

class BrowserPluginThreadedCompositorPixelTest
    : public BrowserPluginThreadedCompositorTest {
 protected:
  virtual void SetUp() OVERRIDE {
    EnablePixelOutput();
    BrowserPluginThreadedCompositorTest::SetUp();
  }
};

static void CompareSkBitmaps(const SkBitmap& expected_bitmap,
                             const SkBitmap& bitmap) {
  EXPECT_EQ(expected_bitmap.width(), bitmap.width());
  if (expected_bitmap.width() != bitmap.width())
    return;
  EXPECT_EQ(expected_bitmap.height(), bitmap.height());
  if (expected_bitmap.height() != bitmap.height())
    return;
  EXPECT_EQ(expected_bitmap.config(), bitmap.config());
  if (expected_bitmap.config() != bitmap.config())
    return;

  SkAutoLockPixels expected_bitmap_lock(expected_bitmap);
  SkAutoLockPixels bitmap_lock(bitmap);
  int fails = 0;
  const int kAllowableError = 2;
  for (int i = 0; i < bitmap.width() && fails < 10; ++i) {
    for (int j = 0; j < bitmap.height() && fails < 10; ++j) {
      SkColor expected_color = expected_bitmap.getColor(i, j);
      SkColor color = bitmap.getColor(i, j);
      int expected_alpha = SkColorGetA(expected_color);
      int alpha = SkColorGetA(color);
      int expected_red = SkColorGetR(expected_color);
      int red = SkColorGetR(color);
      int expected_green = SkColorGetG(expected_color);
      int green = SkColorGetG(color);
      int expected_blue = SkColorGetB(expected_color);
      int blue = SkColorGetB(color);
      EXPECT_NEAR(expected_alpha, alpha, kAllowableError)
          << "expected_color: " << std::hex << expected_color
          << " color: " <<  color
          << " Failed at " << std::dec << i << ", " << j
          << " Failure " << ++fails;
      EXPECT_NEAR(expected_red, red, kAllowableError)
          << "expected_color: " << std::hex << expected_color
          << " color: " <<  color
          << " Failed at " << std::dec << i << ", " << j
          << " Failure " << ++fails;
      EXPECT_NEAR(expected_green, green, kAllowableError)
          << "expected_color: " << std::hex << expected_color
          << " color: " <<  color
          << " Failed at " << std::dec << i << ", " << j
          << " Failure " << ++fails;
      EXPECT_NEAR(expected_blue, blue, kAllowableError)
          << "expected_color: " << std::hex << expected_color
          << " color: " <<  color
          << " Failed at " << std::dec << i << ", " << j
          << " Failure " << ++fails;
    }
  }
  EXPECT_LT(fails, 10);
}

static void CompareSkBitmapAndRun(const base::Closure& callback,
                                  const SkBitmap& expected_bitmap,
                                  bool *result,
                                  bool succeed,
                                  const SkBitmap& bitmap) {
  *result = succeed;
  if (succeed)
    CompareSkBitmaps(expected_bitmap, bitmap);
  callback.Run();
}

// Mac: http://crbug.com/171744
// Aura/Ubercomp: http://crbug.com//327035
#if defined(OS_MACOSX) || defined(USE_AURA)
#define MAYBE_GetBackingStore DISABLED_GetBackingStore
#else
#define MAYBE_GetBackingStore GetBackingStore
#endif
IN_PROC_BROWSER_TEST_F(BrowserPluginThreadedCompositorPixelTest,
                       MAYBE_GetBackingStore) {
  const char kEmbedderURL[] = "/browser_plugin_embedder.html";
  const char kHTMLForGuest[] =
      "data:text/html,<html><style>body { background-color: red; }</style>"
      "<body></body></html>";
  StartBrowserPluginTest(kEmbedderURL, kHTMLForGuest, true,
                         std::string("SetSize(50, 60);"));

  WebContentsImpl* guest_contents = test_guest()->web_contents();
  RenderWidgetHostImpl* guest_widget_host =
      RenderWidgetHostImpl::From(guest_contents->GetRenderViewHost());

  SkBitmap expected_bitmap;
  expected_bitmap.setConfig(SkBitmap::kARGB_8888_Config, 50, 60);
  expected_bitmap.allocPixels();
  expected_bitmap.eraseARGB(255, 255, 0, 0);  // #f00
  bool result = false;
  while (!result) {
    base::RunLoop loop;
    guest_widget_host->CopyFromBackingStore(
        gfx::Rect(),
        guest_widget_host->GetView()->GetViewBounds().size(),
        base::Bind(&CompareSkBitmapAndRun,
                   loop.QuitClosure(),
                   expected_bitmap,
                   &result),
        SkBitmap::kARGB_8888_Config);
    loop.Run();
  }
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
  RenderViewHostImpl* guest_rvh = static_cast<RenderViewHostImpl*>(
      test_guest()->web_contents()->GetRenderViewHost());

  std::vector<blink::WebCompositionUnderline> underlines;

  // An input field in browser plugin guest gets focus and given some user
  // input via IME.
  {
    ExecuteSyncJSFunction(guest_rvh,
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
    ExecuteSyncJSFunction(guest_rvh,
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
    ExecuteSyncJSFunction(guest_rvh,
                          "document.getElementById('input2').focus();");
    test_guest()->WaitForImeCancel();
    scoped_ptr<base::Value> value =
        content::ExecuteScriptAndGetValue(
            guest_rvh, "document.getElementById('input1').value");
    std::string result;
    ASSERT_TRUE(value->GetAsString(&result));
    EXPECT_EQ(base::UTF16ToUTF8(composition), result);
  }
  // Tests ExtendSelectionAndDelete message works in browser_plugin.
  {
    // Set 'InputTestABC' in input1 and put caret at 6 (after 'T').
    ExecuteSyncJSFunction(guest_rvh,
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
    embedder_rvh->Send(
        new ViewMsg_ExtendSelectionAndDelete(
            test_embedder()->web_contents()->GetRoutingID(),
            1, 3));
    base::string16 actual_title = title_watcher.WaitAndGetTitle();
    EXPECT_EQ(expected_value, actual_title);
    scoped_ptr<base::Value> value =
        content::ExecuteScriptAndGetValue(
            guest_rvh, "document.getElementById('input1').value");
    std::string actual_value;
    ASSERT_TRUE(value->GetAsString(&actual_value));
    EXPECT_EQ(base::UTF16ToUTF8(expected_value), actual_value);
  }
}

}  // namespace content
