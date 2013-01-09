// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/memory/singleton.h"
#include "base/run_loop.h"
#include "base/test/test_timeouts.h"
#include "base/utf_string_conversions.h"
#include "content/browser/browser_plugin/browser_plugin_guest.h"
#include "content/browser/browser_plugin/browser_plugin_host_factory.h"
#include "content/browser/browser_plugin/test_browser_plugin_embedder.h"
#include "content/browser/browser_plugin/test_browser_plugin_guest.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/view_messages.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_view_host_observer.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "content/shell/shell.h"
#include "content/test/content_browser_test_utils.h"
#include "content/test/content_browser_test.h"
#include "net/base/net_util.h"
#include "net/test/test_server.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebInputEvent.h"
#include "webkit/glue/webdropdata.h"

using WebKit::WebInputEvent;
using WebKit::WebMouseEvent;
using content::BrowserPluginEmbedder;
using content::BrowserPluginGuest;
using content::BrowserPluginHostFactory;
using content::WebContentsImpl;

namespace {

const char kHTMLForGuest[] =
    "data:text/html,<html><body>hello world</body></html>";
const char kHTMLForGuestBusyLoop[] =
    "data:text/html,<html><head><script type=\"text/javascript\">"
    "function PauseMs(timems) {"
    "  document.title = \"start\";"
    "  var date = new Date();"
    "  var currDate = null;"
    "  do {"
    "    currDate = new Date();"
    "  } while (currDate - date < timems)"
    "}"
    "function StartPauseMs(timems) {"
    "  setTimeout(function() { PauseMs(timems); }, 0);"
    "}"
    "</script></head><body></body></html>";
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
const char kHTMLForGuestWithTitle[] =
    "data:text/html,"
    "<html><head><title>%s</title></head>"
    "<body>hello world</body>"
    "</html>";
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
const char kHTMLForGuestWithSize[] =
    "data:text/html,"
    "<html>"
    "<body style=\"margin: 0px;\">"
    "<img style=\"width: 100%; height: 400px;\"/>"
    "</body>"
    "</html>";

std::string GetHTMLForGuestWithTitle(const std::string& title) {
  return StringPrintf(kHTMLForGuestWithTitle, title.c_str());
}

}  // namespace

namespace content {

// Test factory for creating test instances of BrowserPluginEmbedder and
// BrowserPluginGuest.
class TestBrowserPluginHostFactory : public BrowserPluginHostFactory {
 public:
  virtual BrowserPluginGuest* CreateBrowserPluginGuest(
      int instance_id,
      WebContentsImpl* web_contents,
      const BrowserPluginHostMsg_CreateGuest_Params& params) OVERRIDE {
    return new TestBrowserPluginGuest(instance_id,
                                      web_contents,
                                      params);
  }

  // Also keeps track of number of instances created.
  virtual BrowserPluginEmbedder* CreateBrowserPluginEmbedder(
      WebContentsImpl* web_contents,
      RenderViewHost* render_view_host) OVERRIDE {
    embedder_instance_count_++;
    if (message_loop_runner_)
      message_loop_runner_->Quit();

    return new TestBrowserPluginEmbedder(web_contents, render_view_host);
  }

  // Singleton getter.
  static TestBrowserPluginHostFactory* GetInstance() {
    return Singleton<TestBrowserPluginHostFactory>::get();
  }

  // Waits for at least one embedder to be created in the test. Returns true if
  // we have a guest, false if waiting times out.
  void WaitForEmbedderCreation() {
    // Check if already have created instance.
    if (embedder_instance_count_ > 0)
      return;
    // Wait otherwise.
    message_loop_runner_ = new MessageLoopRunner();
    message_loop_runner_->Run();
  }

 protected:
  TestBrowserPluginHostFactory() : embedder_instance_count_(0) {}
  virtual ~TestBrowserPluginHostFactory() {}

 private:
  // For Singleton.
  friend struct DefaultSingletonTraits<TestBrowserPluginHostFactory>;

  scoped_refptr<MessageLoopRunner> message_loop_runner_;
  int embedder_instance_count_;

  DISALLOW_COPY_AND_ASSIGN(TestBrowserPluginHostFactory);
};

// Test factory class for browser plugin that creates guests with short hang
// timeout.
class TestShortHangTimeoutGuestFactory : public TestBrowserPluginHostFactory {
 public:
  virtual BrowserPluginGuest* CreateBrowserPluginGuest(
      int instance_id,
      WebContentsImpl* web_contents,
      const BrowserPluginHostMsg_CreateGuest_Params& params) OVERRIDE {
    BrowserPluginGuest* guest =
        new TestBrowserPluginGuest(instance_id,
                                   web_contents,
                                   params);
    guest->set_guest_hang_timeout_for_testing(TestTimeouts::tiny_timeout());
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
class RenderViewHostMessageObserver : public RenderViewHostObserver {
 public:
  RenderViewHostMessageObserver(RenderViewHost* host,
                                uint32 message_id)
      : RenderViewHostObserver(host),
        message_id_(message_id),
        message_received_(false) {
  }

  virtual ~RenderViewHostMessageObserver() {}

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
      if (message_loop_runner_)
        message_loop_runner_->Quit();
    }
    return false;
  }

 private:
  scoped_refptr<MessageLoopRunner> message_loop_runner_;
  uint32 message_id_;
  bool message_received_;

  DISALLOW_COPY_AND_ASSIGN(RenderViewHostMessageObserver);
};

class BrowserPluginHostTest : public ContentBrowserTest {
 public:
  BrowserPluginHostTest()
      : test_embedder_(NULL),
        test_guest_(NULL) {}

  virtual void SetUp() OVERRIDE {
    // Override factory to create tests instances of BrowserPlugin*.
    content::BrowserPluginEmbedder::set_factory_for_testing(
        TestBrowserPluginHostFactory::GetInstance());
    content::BrowserPluginGuest::set_factory_for_testing(
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
    scoped_ptr<base::Value> value(rvh->ExecuteJavascriptAndGetValue(
        string16(), UTF8ToUTF16(jscript)));
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
    ASSERT_TRUE(test_server()->Start());
    GURL test_url(test_server()->GetURL(embedder_url));
    NavigateToURL(shell(), test_url);

    WebContentsImpl* embedder_web_contents = static_cast<WebContentsImpl*>(
        shell()->web_contents());
    RenderViewHostImpl* rvh = static_cast<RenderViewHostImpl*>(
        embedder_web_contents->GetRenderViewHost());
    // Focus the embedder.
    rvh->Focus();

    // Allow the test to do some operations on the embedder before we perform
    // the first navigation of the guest.
    if (!embedder_code.empty())
      ExecuteSyncJSFunction(rvh, embedder_code);

    if (!is_guest_data_url) {
      test_url = test_server()->GetURL(guest_url);
      ExecuteSyncJSFunction(
          rvh, StringPrintf("SetSrc('%s');", test_url.spec().c_str()));
    } else {
      ExecuteSyncJSFunction(
          rvh, StringPrintf("SetSrc('%s');", guest_url.c_str()));
    }

    // Wait to make sure embedder is created/attached to WebContents.
    TestBrowserPluginHostFactory::GetInstance()->WaitForEmbedderCreation();

    test_embedder_ = static_cast<TestBrowserPluginEmbedder*>(
        embedder_web_contents->GetBrowserPluginEmbedder());
    ASSERT_TRUE(test_embedder_);
    test_embedder_->WaitForGuestAdded();

    // Verify that we have exactly one guest.
    const BrowserPluginEmbedder::ContainerInstanceMap& instance_map =
        test_embedder_->guest_web_contents_for_testing();
    EXPECT_EQ(1u, instance_map.size());

    WebContentsImpl* test_guest_web_contents = static_cast<WebContentsImpl*>(
        instance_map.begin()->second);
    test_guest_ = static_cast<TestBrowserPluginGuest*>(
        test_guest_web_contents->GetBrowserPluginGuest());
    test_guest_->WaitForLoadStop();
  }

  TestBrowserPluginEmbedder* test_embedder() const { return test_embedder_; }
  TestBrowserPluginGuest* test_guest() const { return test_guest_; }

 private:
  TestBrowserPluginEmbedder* test_embedder_;
  TestBrowserPluginGuest* test_guest_;
  DISALLOW_COPY_AND_ASSIGN(BrowserPluginHostTest);
};

// This test loads a guest that has a busy loop, and therefore it hangs the
// guest.
//
// Disabled on Windows and Linux since it is flaky. crbug.com/164812
#if defined(OS_WIN) || defined(OS_LINUX)
#define MAYBE_GuestUnresponsive DISABLED_GuestUnresponsive
#else
#define MAYBE_GuestUnresponsive GuestUnresponsive
#endif
IN_PROC_BROWSER_TEST_F(BrowserPluginHostTest,
                       MAYBE_GuestUnresponsive) {
  // Override the hang timeout for guest to be very small.
  content::BrowserPluginGuest::set_factory_for_testing(
      TestShortHangTimeoutGuestFactory::GetInstance());
  const char kEmbedderURL[] =
      "files/browser_plugin_embedder_guest_unresponsive.html";
  StartBrowserPluginTest(kEmbedderURL, kHTMLForGuestBusyLoop, true, "");
  // Wait until the busy loop starts.
  {
    const string16 expected_title = ASCIIToUTF16("start");
    content::TitleWatcher title_watcher(test_guest()->web_contents(),
                                        expected_title);
    // Hang the guest for a length of time.
    int spin_time = 10 * TestTimeouts::tiny_timeout().InMilliseconds();
    ExecuteSyncJSFunction(
        test_guest()->web_contents()->GetRenderViewHost(),
        StringPrintf("StartPauseMs(%d);", spin_time).c_str());

    string16 actual_title = title_watcher.WaitAndGetTitle();
    EXPECT_EQ(expected_title, actual_title);
  }
  {
    const string16 expected_title = ASCIIToUTF16("done");
    content::TitleWatcher title_watcher(test_embedder()->web_contents(),
                                        expected_title);

    // Send a mouse event to the guest.
    SimulateMouseClick(test_embedder()->web_contents(), 0,
        WebKit::WebMouseEvent::ButtonLeft);

    string16 actual_title = title_watcher.WaitAndGetTitle();
    EXPECT_EQ(expected_title, actual_title);
  }

  // Verify that the embedder has received the 'unresponsive' and 'responsive'
  // events.
  RenderViewHostImpl* rvh = static_cast<RenderViewHostImpl*>(
      test_embedder()->web_contents()->GetRenderViewHost());
  scoped_ptr<base::Value> value(rvh->ExecuteJavascriptAndGetValue(string16(),
      ASCIIToUTF16("unresponsiveCalled")));
  bool result = false;
  ASSERT_TRUE(value->GetAsBoolean(&result));
  EXPECT_TRUE(result);

  value.reset(rvh->ExecuteJavascriptAndGetValue(string16(),
      ASCIIToUTF16("responsiveCalled")));
  result = false;
  ASSERT_TRUE(value->GetAsBoolean(&result));
  EXPECT_TRUE(result);
}

// This test ensures that if guest isn't there and we resize the guest (from
// js), it remembers the size correctly.
//
// Initially we load an embedder with a guest without a src attribute (which has
// dimension 640x480), resize it to 100x200, and then we set the source to a
// sample guest. In the end we verify that the correct size has been set.
IN_PROC_BROWSER_TEST_F(BrowserPluginHostTest, NavigateAfterResize) {
  const gfx::Size nxt_size = gfx::Size(100, 200);
  const std::string embedder_code =
      StringPrintf("SetSize(%d, %d);", nxt_size.width(), nxt_size.height());
  const char kEmbedderURL[] = "files/browser_plugin_embedder.html";
  StartBrowserPluginTest(kEmbedderURL, kHTMLForGuest, true, embedder_code);

  // Wait for the guest to receive a damage buffer of size 100x200.
  // This means the guest will be painted properly at that size.
  test_guest()->WaitForDamageBufferWithSize(nxt_size);
}

IN_PROC_BROWSER_TEST_F(BrowserPluginHostTest, AdvanceFocus) {
  const char kEmbedderURL[] = "files/browser_plugin_focus.html";
  const char* kGuestURL = "files/browser_plugin_focus_child.html";
  StartBrowserPluginTest(kEmbedderURL, kGuestURL, false, "");

  SimulateMouseClick(test_embedder()->web_contents(), 0,
      WebKit::WebMouseEvent::ButtonLeft);
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
  net::TestServer https_server(
      net::TestServer::TYPE_HTTPS,
      net::TestServer::kLocalhost,
      FilePath(FILE_PATH_LITERAL("content/test/data")));
  ASSERT_TRUE(https_server.Start());

  // 1. Load an embedder page with one guest in it.
  const char kEmbedderURL[] = "files/browser_plugin_embedder.html";
  StartBrowserPluginTest(kEmbedderURL, kHTMLForGuest, true, "");

  // 2. Navigate to a URL in https, so we trigger a RenderViewHost swap.
  GURL test_https_url(https_server.GetURL(
      "files/browser_plugin_title_change.html"));
  content::WindowedNotificationObserver swap_observer(
      content::NOTIFICATION_WEB_CONTENTS_SWAPPED,
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
IN_PROC_BROWSER_TEST_F(BrowserPluginHostTest, EmbedderSameAfterNav) {
  const char kEmbedderURL[] = "files/browser_plugin_embedder.html";
  StartBrowserPluginTest(kEmbedderURL, kHTMLForGuest, true, "");
  WebContentsImpl* embedder_web_contents = test_embedder()->web_contents();

  // Navigate to another page in same host and port, so RenderViewHost swap
  // does not happen and existing embedder doesn't change in web_contents.
  GURL test_url_new(test_server()->GetURL(
      "files/browser_plugin_title_change.html"));
  const string16 expected_title = ASCIIToUTF16("done");
  content::TitleWatcher title_watcher(shell()->web_contents(), expected_title);
  NavigateToURL(shell(), test_url_new);
  LOG(INFO) << "Start waiting for title";
  string16 actual_title = title_watcher.WaitAndGetTitle();
  EXPECT_EQ(expected_title, actual_title);
  LOG(INFO) << "Done navigating to second page";

  TestBrowserPluginEmbedder* test_embedder_after_nav =
      static_cast<TestBrowserPluginEmbedder*>(
          embedder_web_contents->GetBrowserPluginEmbedder());
  // Embedder must not change in web_contents.
  ASSERT_EQ(test_embedder_after_nav, test_embedder());
}

// This test verifies that hiding the embedder also hides the guest.
IN_PROC_BROWSER_TEST_F(BrowserPluginHostTest, BrowserPluginVisibilityChanged) {
  const char kEmbedderURL[] = "files/browser_plugin_embedder.html";
  StartBrowserPluginTest(kEmbedderURL, kHTMLForGuest, true, "");

  // Hide the Browser Plugin.
  RenderViewHostImpl* rvh = static_cast<RenderViewHostImpl*>(
      test_embedder()->web_contents()->GetRenderViewHost());
  ExecuteSyncJSFunction(
      rvh, "document.getElementById('plugin').style.visibility = 'hidden'");

  // Make sure that the guest is hidden.
  test_guest()->WaitUntilHidden();
}

IN_PROC_BROWSER_TEST_F(BrowserPluginHostTest, EmbedderVisibilityChanged) {
  const char kEmbedderURL[] = "files/browser_plugin_embedder.html";
  StartBrowserPluginTest(kEmbedderURL, kHTMLForGuest, true, "");

  // Hide the embedder.
  test_embedder()->web_contents()->WasHidden();

  // Make sure that hiding the embedder also hides the guest.
  test_guest()->WaitUntilHidden();
}

// This test verifies that calling the reload method reloads the guest.
IN_PROC_BROWSER_TEST_F(BrowserPluginHostTest, ReloadGuest) {
  const char kEmbedderURL[] = "files/browser_plugin_embedder.html";
  StartBrowserPluginTest(kEmbedderURL, kHTMLForGuest, true, "");

  test_guest()->ResetUpdateRectCount();

  RenderViewHostImpl* rvh = static_cast<RenderViewHostImpl*>(
      test_embedder()->web_contents()->GetRenderViewHost());
  ExecuteSyncJSFunction(rvh, "document.getElementById('plugin').reload()");
  test_guest()->WaitForReload();
}

// This test verifies that calling the stop method forwards the stop request
// to the guest's WebContents.
IN_PROC_BROWSER_TEST_F(BrowserPluginHostTest, StopGuest) {
  const char kEmbedderURL[] = "files/browser_plugin_embedder.html";
  StartBrowserPluginTest(kEmbedderURL, kHTMLForGuest, true, "");

  RenderViewHostImpl* rvh = static_cast<RenderViewHostImpl*>(
      test_embedder()->web_contents()->GetRenderViewHost());
  ExecuteSyncJSFunction(rvh, "document.getElementById('plugin').stop()");
  test_guest()->WaitForStop();
}

// Verifies that installing/uninstalling touch-event handlers in the guest
// plugin correctly updates the touch-event handling state in the embedder.
IN_PROC_BROWSER_TEST_F(BrowserPluginHostTest, AcceptTouchEvents) {
  const char kEmbedderURL[] = "files/browser_plugin_embedder.html";
  StartBrowserPluginTest(kEmbedderURL, kHTMLForGuestTouchHandler, true, "");

  RenderViewHostImpl* rvh = static_cast<RenderViewHostImpl*>(
      test_embedder()->web_contents()->GetRenderViewHost());
  // The embedder should not have any touch event handlers at this point.
  EXPECT_FALSE(rvh->has_touch_handler());

  // Install the touch handler in the guest. This should cause the embedder to
  // start listening for touch events too.
  RenderViewHostMessageObserver observer(rvh,
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

IN_PROC_BROWSER_TEST_F(BrowserPluginHostTest, Renavigate) {
  const char kEmbedderURL[] = "files/browser_plugin_embedder.html";
  StartBrowserPluginTest(
      kEmbedderURL, GetHTMLForGuestWithTitle("P1"), true, "");
  RenderViewHostImpl* rvh = static_cast<RenderViewHostImpl*>(
      test_embedder()->web_contents()->GetRenderViewHost());

  // Navigate to P2 and verify that the navigation occurred.
  {
    const string16 expected_title = ASCIIToUTF16("P2");
    content::TitleWatcher title_watcher(test_guest()->web_contents(),
                                        expected_title);

    ExecuteSyncJSFunction(
        rvh,
        StringPrintf("SetSrc('%s');", GetHTMLForGuestWithTitle("P2").c_str()));

    string16 actual_title = title_watcher.WaitAndGetTitle();
    EXPECT_EQ(expected_title, actual_title);
  }

  // Navigate to P3 and verify that the navigation occurred.
  {
    const string16 expected_title = ASCIIToUTF16("P3");
    content::TitleWatcher title_watcher(test_guest()->web_contents(),
                                        expected_title);

    ExecuteSyncJSFunction(
        rvh,
        StringPrintf("SetSrc('%s');", GetHTMLForGuestWithTitle("P3").c_str()));

    string16 actual_title = title_watcher.WaitAndGetTitle();
    EXPECT_EQ(expected_title, actual_title);
  }

  // Go back and verify that we're back at P2.
  {
    const string16 expected_title = ASCIIToUTF16("P2");
    content::TitleWatcher title_watcher(test_guest()->web_contents(),
                                        expected_title);

    ExecuteSyncJSFunction(rvh, "Back();");
    string16 actual_title = title_watcher.WaitAndGetTitle();
    EXPECT_EQ(expected_title, actual_title);

    scoped_ptr<base::Value> value(rvh->ExecuteJavascriptAndGetValue(string16(),
        ASCIIToUTF16("CanGoBack()")));
    bool result = false;
    ASSERT_TRUE(value->GetAsBoolean(&result));
    EXPECT_TRUE(result);

    value.reset(rvh->ExecuteJavascriptAndGetValue(string16(),
        ASCIIToUTF16("CanGoForward()")));
    result = false;
    ASSERT_TRUE(value->GetAsBoolean(&result));
    EXPECT_TRUE(result);
  }

  // Go forward and verify that we're back at P3.
  {
    const string16 expected_title = ASCIIToUTF16("P3");
    content::TitleWatcher title_watcher(test_guest()->web_contents(),
                                        expected_title);

    ExecuteSyncJSFunction(rvh, "Forward();");
    string16 actual_title = title_watcher.WaitAndGetTitle();
    EXPECT_EQ(expected_title, actual_title);

    scoped_ptr<base::Value> value(rvh->ExecuteJavascriptAndGetValue(string16(),
        ASCIIToUTF16("CanGoForward()")));
    bool result = true;
    ASSERT_TRUE(value->GetAsBoolean(&result));
    EXPECT_FALSE(result);
  }

  // Go back two entries and verify that we're back at P1.
  {
    const string16 expected_title = ASCIIToUTF16("P1");
    content::TitleWatcher title_watcher(test_guest()->web_contents(),
                                        expected_title);

    ExecuteSyncJSFunction(rvh, "Go(-2);");
    string16 actual_title = title_watcher.WaitAndGetTitle();
    EXPECT_EQ(expected_title, actual_title);

    scoped_ptr<base::Value> value(rvh->ExecuteJavascriptAndGetValue(string16(),
        ASCIIToUTF16("CanGoBack()")));
    bool result = true;
    ASSERT_TRUE(value->GetAsBoolean(&result));
    EXPECT_FALSE(result);
  }
}

// This tests verifies that reloading the embedder does not crash the browser
// and that the guest is reset.
IN_PROC_BROWSER_TEST_F(BrowserPluginHostTest, ReloadEmbedder) {
  const char kEmbedderURL[] = "files/browser_plugin_embedder.html";
  StartBrowserPluginTest(kEmbedderURL, kHTMLForGuest, true, "");
  RenderViewHostImpl* rvh = static_cast<RenderViewHostImpl*>(
      test_embedder()->web_contents()->GetRenderViewHost());

  // Change the title of the page to 'modified' so that we know that
  // the page has successfully reloaded when it goes back to 'embedder'
  // in the next step.
  {
    const string16 expected_title = ASCIIToUTF16("modified");
    content::TitleWatcher title_watcher(test_embedder()->web_contents(),
                                        expected_title);

    ExecuteSyncJSFunction(rvh, StringPrintf("SetTitle('%s');", "modified"));

    string16 actual_title = title_watcher.WaitAndGetTitle();
    EXPECT_EQ(expected_title, actual_title);
  }

  // Reload the embedder page, and verify that the reload was successful.
  // Then navigate the guest to verify that the browser process does not crash.
  {
    const string16 expected_title = ASCIIToUTF16("embedder");
    content::TitleWatcher title_watcher(test_embedder()->web_contents(),
                                        expected_title);

    test_embedder()->web_contents()->GetController().Reload(false);
    string16 actual_title = title_watcher.WaitAndGetTitle();
    EXPECT_EQ(expected_title, actual_title);

    ExecuteSyncJSFunction(
        test_embedder()->web_contents()->GetRenderViewHost(),
        StringPrintf("SetSrc('%s');", kHTMLForGuest));

    const BrowserPluginEmbedder::ContainerInstanceMap& instance_map =
        test_embedder()->guest_web_contents_for_testing();
    WebContentsImpl* test_guest_web_contents = static_cast<WebContentsImpl*>(
        instance_map.begin()->second);
    TestBrowserPluginGuest* new_test_guest =
        static_cast<TestBrowserPluginGuest*>(
          test_guest_web_contents->GetBrowserPluginGuest());

    // Wait for the guest to send an UpdateRectMsg, meaning it is ready.
    new_test_guest->WaitForUpdateRectMsg();
  }
}

IN_PROC_BROWSER_TEST_F(BrowserPluginHostTest, TerminateGuest) {
  const char kEmbedderURL[] = "files/browser_plugin_embedder.html";
  StartBrowserPluginTest(kEmbedderURL, kHTMLForGuest, true, "");

  RenderViewHostImpl* rvh = static_cast<RenderViewHostImpl*>(
      test_embedder()->web_contents()->GetRenderViewHost());
  ExecuteSyncJSFunction(rvh, "document.getElementById('plugin').terminate()");

  // Expect the guest to crash.
  test_guest()->WaitForExit();
}

// This test verifies that the guest is responsive after crashing and going back
// to a previous navigation entry.
IN_PROC_BROWSER_TEST_F(BrowserPluginHostTest, BackAfterTerminateGuest) {
  const char* kEmbedderURL = "files/browser_plugin_embedder.html";
  StartBrowserPluginTest(
      kEmbedderURL, GetHTMLForGuestWithTitle("P1"), true, "");
  RenderViewHostImpl* rvh = static_cast<RenderViewHostImpl*>(
      test_embedder()->web_contents()->GetRenderViewHost());

  // Navigate to P2 and verify that the navigation occurred.
  {
    const string16 expected_title = ASCIIToUTF16("P2");
    content::TitleWatcher title_watcher(test_guest()->web_contents(),
                                        expected_title);

    ExecuteSyncJSFunction(
        rvh,
        StringPrintf("SetSrc('%s');", GetHTMLForGuestWithTitle("P2").c_str()));

    string16 actual_title = title_watcher.WaitAndGetTitle();
    EXPECT_EQ(expected_title, actual_title);
  }
  // Kill the guest.
  ExecuteSyncJSFunction(rvh, "document.getElementById('plugin').terminate()");

  // Expect the guest to report that it crashed.
  test_guest()->WaitForExit();
  // Go back and verify that we're back at P1.
  {
    const string16 expected_title = ASCIIToUTF16("P1");
    content::TitleWatcher title_watcher(test_guest()->web_contents(),
                                        expected_title);

    ExecuteSyncJSFunction(rvh, "Back();");

    string16 actual_title = title_watcher.WaitAndGetTitle();
    EXPECT_EQ(expected_title, actual_title);
  }
  // Send an input event and verify that the guest receives the input.
  SimulateMouseClick(test_embedder()->web_contents(), 0,
      WebKit::WebMouseEvent::ButtonLeft);
  test_guest()->WaitForInput();
}

IN_PROC_BROWSER_TEST_F(BrowserPluginHostTest, LoadStart) {
  const char kEmbedderURL[] = "files/browser_plugin_embedder.html";
  StartBrowserPluginTest(kEmbedderURL, "about:blank", true, "");

  const string16 expected_title = ASCIIToUTF16(kHTMLForGuest);
  content::TitleWatcher title_watcher(test_embedder()->web_contents(),
                                      expected_title);
  // Renavigate the guest to |kHTMLForGuest|.
  RenderViewHostImpl* rvh = static_cast<RenderViewHostImpl*>(
      test_embedder()->web_contents()->GetRenderViewHost());
  ExecuteSyncJSFunction(rvh, StringPrintf("SetSrc('%s');", kHTMLForGuest));

  string16 actual_title = title_watcher.WaitAndGetTitle();
  EXPECT_EQ(expected_title, actual_title);
}

IN_PROC_BROWSER_TEST_F(BrowserPluginHostTest, LoadAbort) {
  const char kEmbedderURL[] = "files/browser_plugin_embedder.html";
  StartBrowserPluginTest(kEmbedderURL, "about:blank", true, "");

  {
    // Navigate the guest to "close-socket".
    const string16 expected_title = ASCIIToUTF16("ERR_EMPTY_RESPONSE");
    content::TitleWatcher title_watcher(test_embedder()->web_contents(),
                                        expected_title);
    RenderViewHostImpl* rvh = static_cast<RenderViewHostImpl*>(
        test_embedder()->web_contents()->GetRenderViewHost());
    GURL test_url = test_server()->GetURL("close-socket");
    ExecuteSyncJSFunction(
        rvh, StringPrintf("SetSrc('%s');", test_url.spec().c_str()));
    string16 actual_title = title_watcher.WaitAndGetTitle();
    EXPECT_EQ(expected_title, actual_title);
  }

  {
    // Navigate the guest to an illegal chrome:// URL.
    const string16 expected_title = ASCIIToUTF16("ERR_FAILED");
    content::TitleWatcher title_watcher(test_embedder()->web_contents(),
                                        expected_title);
    RenderViewHostImpl* rvh = static_cast<RenderViewHostImpl*>(
        test_embedder()->web_contents()->GetRenderViewHost());
    GURL test_url("chrome://newtab");
    ExecuteSyncJSFunction(
        rvh, StringPrintf("SetSrc('%s');", test_url.spec().c_str()));
    string16 actual_title = title_watcher.WaitAndGetTitle();
    EXPECT_EQ(expected_title, actual_title);
  }

  {
    // Navigate the guest to an illegal file:// URL.
    const string16 expected_title = ASCIIToUTF16("ERR_ABORTED");
    content::TitleWatcher title_watcher(test_embedder()->web_contents(),
                                        expected_title);
    RenderViewHostImpl* rvh = static_cast<RenderViewHostImpl*>(
        test_embedder()->web_contents()->GetRenderViewHost());
    GURL test_url("file://foo");
    ExecuteSyncJSFunction(
        rvh, StringPrintf("SetSrc('%s');", test_url.spec().c_str()));
    string16 actual_title = title_watcher.WaitAndGetTitle();
    EXPECT_EQ(expected_title, actual_title);
  }
}

IN_PROC_BROWSER_TEST_F(BrowserPluginHostTest, LoadRedirect) {
  const char kEmbedderURL[] = "files/browser_plugin_embedder.html";
  StartBrowserPluginTest(kEmbedderURL, "about:blank", true, "");

  const string16 expected_title = ASCIIToUTF16("redirected");
  content::TitleWatcher title_watcher(test_embedder()->web_contents(),
                                      expected_title);

  // Navigate with a redirect and wait until the title changes.
  GURL redirect_url(test_server()->GetURL(
      "server-redirect?files/title1.html"));
  RenderViewHostImpl* rvh = static_cast<RenderViewHostImpl*>(
      test_embedder()->web_contents()->GetRenderViewHost());
  ExecuteSyncJSFunction(
      rvh, StringPrintf("SetSrc('%s');", redirect_url.spec().c_str()));

  string16 actual_title = title_watcher.WaitAndGetTitle();
  EXPECT_EQ(expected_title, actual_title);

  // Verify that we heard a loadRedirect during the navigation.
  scoped_ptr<base::Value> value(rvh->ExecuteJavascriptAndGetValue(
      string16(), ASCIIToUTF16("redirectOldUrl")));
  std::string result;
  EXPECT_TRUE(value->GetAsString(&result));
  EXPECT_EQ(redirect_url.spec().c_str(), result);

  value.reset(rvh->ExecuteJavascriptAndGetValue(
      string16(), ASCIIToUTF16("redirectNewUrl")));
  EXPECT_TRUE(value->GetAsString(&result));
  EXPECT_EQ(test_server()->GetURL("files/title1.html").spec().c_str(), result);
}

// Tests that a drag-n-drop over the browser plugin in the embedder happens
// correctly.
IN_PROC_BROWSER_TEST_F(BrowserPluginHostTest, AcceptDragEvents) {
  const char kEmbedderURL[] = "files/browser_plugin_dragging.html";
  StartBrowserPluginTest(kEmbedderURL, kHTMLForGuestAcceptDrag, true, "");

  RenderViewHostImpl* rvh = static_cast<RenderViewHostImpl*>(
      test_embedder()->web_contents()->GetRenderViewHost());

  // Get a location in the embedder outside of the plugin.
  base::ListValue *start, *end;
  scoped_ptr<base::Value> value(rvh->ExecuteJavascriptAndGetValue(string16(),
      ASCIIToUTF16("dragLocation()")));
  ASSERT_TRUE(value->GetAsList(&start) && start->GetSize() == 2);
  double start_x, start_y;
  ASSERT_TRUE(start->GetDouble(0, &start_x) && start->GetDouble(1, &start_y));

  // Get a location in the embedder that falls inside the plugin.
  value.reset(rvh->ExecuteJavascriptAndGetValue(string16(),
      ASCIIToUTF16("dropLocation()")));
  ASSERT_TRUE(value->GetAsList(&end) && end->GetSize() == 2);
  double end_x, end_y;
  ASSERT_TRUE(end->GetDouble(0, &end_x) && end->GetDouble(1, &end_y));

  WebDropData drop_data;
  GURL url = GURL("https://www.domain.com/index.html");
  drop_data.url = url;

  // Pretend that the URL is being dragged over the embedder. Start the drag
  // from outside the plugin, then move the drag inside the plugin and drop.
  // This should trigger appropriate messages from the embedder to the guest,
  // and end with a drop on the guest. The guest changes title when a drop
  // happens.
  const string16 expected_title = ASCIIToUTF16("DROPPED");
  content::TitleWatcher title_watcher(test_guest()->web_contents(),
      expected_title);

  rvh->DragTargetDragEnter(drop_data, gfx::Point(start_x, start_y),
      gfx::Point(start_x, start_y), WebKit::WebDragOperationEvery, 0);
  rvh->DragTargetDragOver(gfx::Point(end_x, end_y), gfx::Point(end_x, end_y),
      WebKit::WebDragOperationEvery, 0);
  rvh->DragTargetDrop(gfx::Point(end_x, end_y), gfx::Point(end_x, end_y), 0);

  string16 actual_title = title_watcher.WaitAndGetTitle();
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
  const char* kEmbedderURL = "files/browser_plugin_embedder.html";
  const char* kGuestURL = "files/browser_plugin_post_message_guest.html";
  StartBrowserPluginTest(kEmbedderURL, kGuestURL, false, "");
  RenderViewHostImpl* rvh = static_cast<RenderViewHostImpl*>(
      test_embedder()->web_contents()->GetRenderViewHost());
  {
    const string16 expected_title = ASCIIToUTF16("main guest");
    content::TitleWatcher title_watcher(test_embedder()->web_contents(),
                                        expected_title);

    // By the time we get here 'contentWindow' should be ready because the
    // guest has completed loading.
    ExecuteSyncJSFunction(
        rvh, StringPrintf("PostMessage('%s, false');", kTesting));

    // The title will be updated to "main guest" at the last stage of the
    // process described above.
    string16 actual_title = title_watcher.WaitAndGetTitle();
    EXPECT_EQ(expected_title, actual_title);
  }
}

// This is the same as BrowserPluginHostTest.PostMessage but also
// posts a message to an iframe.
// TODO(fsamuel): This test should replace the previous test once postMessage
// iframe targeting is fixed (see http://crbug.com/153701).
IN_PROC_BROWSER_TEST_F(BrowserPluginHostTest, DISABLED_PostMessageToIFrame) {
  const char* kTesting = "testing123";
  const char* kEmbedderURL = "files/browser_plugin_embedder.html";
  const char* kGuestURL = "files/browser_plugin_post_message_guest.html";
  StartBrowserPluginTest(kEmbedderURL, kGuestURL, false, "");
  RenderViewHostImpl* rvh = static_cast<RenderViewHostImpl*>(
      test_embedder()->web_contents()->GetRenderViewHost());
  {
    const string16 expected_title = ASCIIToUTF16("main guest");
    content::TitleWatcher title_watcher(test_embedder()->web_contents(),
                                        expected_title);

    ExecuteSyncJSFunction(
        rvh, StringPrintf("PostMessage('%s, false');", kTesting));

    // The title will be updated to "main guest" at the last stage of the
    // process described above.
    string16 actual_title = title_watcher.WaitAndGetTitle();
    EXPECT_EQ(expected_title, actual_title);
  }
  {
    content::TitleWatcher ready_watcher(test_embedder()->web_contents(),
                                        ASCIIToUTF16("ready"));

    RenderViewHostImpl* guest_rvh = static_cast<RenderViewHostImpl*>(
        test_guest()->web_contents()->GetRenderViewHost());
    GURL test_url = test_server()->GetURL(
        "files/browser_plugin_post_message_guest.html");
    ExecuteSyncJSFunction(
        guest_rvh,
        StringPrintf("CreateChildFrame('%s');", test_url.spec().c_str()));

    string16 actual_title = ready_watcher.WaitAndGetTitle();
    EXPECT_EQ(ASCIIToUTF16("ready"), actual_title);

    content::TitleWatcher iframe_watcher(test_embedder()->web_contents(),
                                        ASCIIToUTF16("iframe"));
    ExecuteSyncJSFunction(
        rvh, StringPrintf("PostMessage('%s', true);", kTesting));

    // The title will be updated to "iframe" at the last stage of the
    // process described above.
    actual_title = iframe_watcher.WaitAndGetTitle();
    EXPECT_EQ(ASCIIToUTF16("iframe"), actual_title);
  }
}

IN_PROC_BROWSER_TEST_F(BrowserPluginHostTest, LoadStop) {
  const char* kEmbedderURL = "files/browser_plugin_embedder.html";
  StartBrowserPluginTest(kEmbedderURL, "about:blank", true, "");

  const string16 expected_title = ASCIIToUTF16("loadStop");
  content::TitleWatcher title_watcher(
     test_embedder()->web_contents(), expected_title);
  // Renavigate the guest to |kHTMLForGuest|.
  RenderViewHostImpl* rvh = static_cast<RenderViewHostImpl*>(
      test_embedder()->web_contents()->GetRenderViewHost());
  ExecuteSyncJSFunction(rvh, StringPrintf("SetSrc('%s');", kHTMLForGuest));

  string16 actual_title = title_watcher.WaitAndGetTitle();
  EXPECT_EQ(expected_title, actual_title);
}

IN_PROC_BROWSER_TEST_F(BrowserPluginHostTest, LoadCommit) {
  const char* kEmbedderURL = "files/browser_plugin_embedder.html";
  StartBrowserPluginTest(kEmbedderURL, "about:blank", true, "");

  const string16 expected_title = ASCIIToUTF16(
      StringPrintf("loadCommit:%s", kHTMLForGuest));
  content::TitleWatcher title_watcher(
      test_embedder()->web_contents(), expected_title);
  // Renavigate the guest to |kHTMLForGuest|.
  RenderViewHostImpl* rvh = static_cast<RenderViewHostImpl*>(
      test_embedder()->web_contents()->GetRenderViewHost());
  ExecuteSyncJSFunction(rvh, StringPrintf("SetSrc('%s');", kHTMLForGuest));

  string16 actual_title = title_watcher.WaitAndGetTitle();
  EXPECT_EQ(expected_title, actual_title);
  scoped_ptr<base::Value> is_top_level(rvh->ExecuteJavascriptAndGetValue(
      string16(), ASCIIToUTF16("commitIsTopLevel")));
  bool top_level_bool = false;
  EXPECT_TRUE(is_top_level->GetAsBoolean(&top_level_bool));
  EXPECT_EQ(true, top_level_bool);
}

// This test verifies that if a browser plugin is hidden before navigation,
// the guest starts off hidden.
IN_PROC_BROWSER_TEST_F(BrowserPluginHostTest, HiddenBeforeNavigation) {
  const char* kEmbedderURL = "files/browser_plugin_embedder.html";
  const std::string embedder_code =
      "document.getElementById('plugin').style.visibility = 'hidden'";
  StartBrowserPluginTest(
      kEmbedderURL, kHTMLForGuest, true, embedder_code);
  EXPECT_FALSE(test_guest()->visible());
}

// This test verifies that if we lose the guest, and get a new one,
// the new guest will inherit the visibility state of the old guest.
//
// Very flaky on Linux, Linux CrOS, somewhat flaky on XP, slightly on
// Mac; http://crbug.com/162809.
IN_PROC_BROWSER_TEST_F(BrowserPluginHostTest, DISABLED_VisibilityPreservation) {
  const char* kEmbedderURL = "files/browser_plugin_embedder.html";
  StartBrowserPluginTest(kEmbedderURL, kHTMLForGuest, true, "");
  RenderViewHostImpl* rvh = static_cast<RenderViewHostImpl*>(
      test_embedder()->web_contents()->GetRenderViewHost());
  // Hide the BrowserPlugin.
  ExecuteSyncJSFunction(
      rvh, "document.getElementById('plugin').style.visibility = 'hidden';");
  test_guest()->WaitUntilHidden();
  // Kill the current guest.
  ExecuteSyncJSFunction(rvh, "document.getElementById('plugin').terminate();");
  test_guest()->WaitForExit();
  // Get a new guest.
  ExecuteSyncJSFunction(rvh, "document.getElementById('plugin').reload();");
  test_guest()->WaitForLoadStop();
  // Verify that the guest is told to hide.
  test_guest()->WaitUntilHidden();
}

// This test verifies that if a browser plugin is focused before navigation then
// the guest starts off focused.
IN_PROC_BROWSER_TEST_F(BrowserPluginHostTest, FocusBeforeNavigation) {
  const char* kEmbedderURL = "files/browser_plugin_embedder.html";
  const std::string embedder_code =
      "document.getElementById('plugin').focus();";
  StartBrowserPluginTest(
      kEmbedderURL, kHTMLForGuest, true, embedder_code);
  RenderViewHostImpl* guest_rvh = static_cast<RenderViewHostImpl*>(
      test_guest()->web_contents()->GetRenderViewHost());
  // Verify that the guest is focused.
  scoped_ptr<base::Value> value(
      guest_rvh->ExecuteJavascriptAndGetValue(string16(),
          ASCIIToUTF16("document.hasFocus()")));
  bool result = false;
  ASSERT_TRUE(value->GetAsBoolean(&result));
  EXPECT_TRUE(result);
}

// This test verifies that if we lose the guest, and get a new one,
// the new guest will inherit the focus state of the old guest.
IN_PROC_BROWSER_TEST_F(BrowserPluginHostTest, FocusPreservation) {
  const char* kEmbedderURL = "files/browser_plugin_embedder.html";
  StartBrowserPluginTest(kEmbedderURL, kHTMLForGuest, true, "");
  RenderViewHostImpl* rvh = static_cast<RenderViewHostImpl*>(
      test_embedder()->web_contents()->GetRenderViewHost());
  RenderViewHostImpl* guest_rvh = static_cast<RenderViewHostImpl*>(
      test_guest()->web_contents()->GetRenderViewHost());
  {
    // Focus the BrowserPlugin. This will have the effect of also focusing the
    // current guest.
    ExecuteSyncJSFunction(rvh, "document.getElementById('plugin').focus();");
    // Verify that key presses go to the guest.
    SimulateSpaceKeyPress(test_embedder()->web_contents());
    test_guest()->WaitForInput();
    // Verify that the guest is focused.
    scoped_ptr<base::Value> value(
        guest_rvh->ExecuteJavascriptAndGetValue(string16(),
            ASCIIToUTF16("document.hasFocus()")));
    bool result = false;
    ASSERT_TRUE(value->GetAsBoolean(&result));
    EXPECT_TRUE(result);
  }

  // Kill the current guest.
  ExecuteSyncJSFunction(rvh, "document.getElementById('plugin').terminate();");
  test_guest()->WaitForExit();

  {
    // Get a new guest.
    ExecuteSyncJSFunction(rvh, "document.getElementById('plugin').reload();");
    test_guest()->WaitForLoadStop();
    // Verify that the guest is focused.
    scoped_ptr<base::Value> value(
        guest_rvh->ExecuteJavascriptAndGetValue(string16(),
            ASCIIToUTF16("document.hasFocus()")));
    bool result = false;
    ASSERT_TRUE(value->GetAsBoolean(&result));
    EXPECT_TRUE(result);
  }
}

IN_PROC_BROWSER_TEST_F(BrowserPluginHostTest, FocusTracksEmbedder) {
  const char* kEmbedderURL = "files/browser_plugin_embedder.html";
  StartBrowserPluginTest(kEmbedderURL, kHTMLForGuest, true, "");
  RenderViewHostImpl* rvh = static_cast<RenderViewHostImpl*>(
      test_embedder()->web_contents()->GetRenderViewHost());
  RenderViewHostImpl* guest_rvh = static_cast<RenderViewHostImpl*>(
      test_guest()->web_contents()->GetRenderViewHost());
  {
    // Focus the BrowserPlugin. This will have the effect of also focusing the
    // current guest.
    ExecuteSyncJSFunction(rvh, "document.getElementById('plugin').focus();");
    // Verify that key presses go to the guest.
    SimulateSpaceKeyPress(test_embedder()->web_contents());
    test_guest()->WaitForInput();
    // Verify that the guest is focused.
    scoped_ptr<base::Value> value(
        guest_rvh->ExecuteJavascriptAndGetValue(string16(),
            ASCIIToUTF16("document.hasFocus()")));
    bool result = false;
    ASSERT_TRUE(value->GetAsBoolean(&result));
    EXPECT_TRUE(result);
  }
  // Blur the embedder.
  test_embedder()->web_contents()->GetRenderViewHost()->Blur();
  test_guest()->WaitForBlur();
}

// This test verifies that if a browser plugin is in autosize mode before
// navigation then the guest starts auto-sized.
IN_PROC_BROWSER_TEST_F(BrowserPluginHostTest, AutoSizeBeforeNavigation) {
  const char* kEmbedderURL = "files/browser_plugin_embedder.html";
  const std::string embedder_code =
      "document.getElementById('plugin').minWidth = 300;"
      "document.getElementById('plugin').minHeight = 200;"
      "document.getElementById('plugin').maxWidth = 600;"
      "document.getElementById('plugin').maxHeight = 400;"
      "document.getElementById('plugin').autoSize = true;";
  StartBrowserPluginTest(
      kEmbedderURL, kHTMLForGuestWithSize, true, embedder_code);
  // Verify that the guest has been auto-sized.
  test_guest()->WaitForViewSize(gfx::Size(300, 400));
}

// This test verifies that enabling autosize resizes the guest and triggers
// a 'sizechanged' event.
IN_PROC_BROWSER_TEST_F(BrowserPluginHostTest, AutoSizeAfterNavigation) {
  const char* kEmbedderURL = "files/browser_plugin_embedder.html";
  StartBrowserPluginTest(
      kEmbedderURL, kHTMLForGuestWithSize, true, "");
  RenderViewHostImpl* rvh = static_cast<RenderViewHostImpl*>(
      test_embedder()->web_contents()->GetRenderViewHost());

  {
    const string16 expected_title = ASCIIToUTF16("AutoSize(300, 400)");
    content::TitleWatcher title_watcher(test_embedder()->web_contents(),
                                        expected_title);
    ExecuteSyncJSFunction(
        rvh,
        "document.getElementById('plugin').minWidth = 300;"
        "document.getElementById('plugin').minHeight = 200;"
        "document.getElementById('plugin').maxWidth = 600;"
        "document.getElementById('plugin').maxHeight = 400;"
        "document.getElementById('plugin').autoSize = true;");
    string16 actual_title = title_watcher.WaitAndGetTitle();
    EXPECT_EQ(expected_title, actual_title);
  }
  {
    // Change the minWidth and verify that it causes relayout.
    const string16 expected_title = ASCIIToUTF16("AutoSize(350, 400)");
    content::TitleWatcher title_watcher(test_embedder()->web_contents(),
                                        expected_title);
    ExecuteSyncJSFunction(
        rvh, "document.getElementById('plugin').minWidth = 350;");
    string16 actual_title = title_watcher.WaitAndGetTitle();
    EXPECT_EQ(expected_title, actual_title);
  }
  {
    // Turn off autoSize and verify that the guest resizes to fit the container.
    ExecuteSyncJSFunction(
        rvh, "document.getElementById('plugin').autoSize = false;");
    test_guest()->WaitForViewSize(gfx::Size(640, 480));
  }
}

// Test for regression http://crbug.com/162961.
IN_PROC_BROWSER_TEST_F(BrowserPluginHostTest, GetRenderViewHostAtPositionTest) {
  const char kEmbedderURL[] = "files/browser_plugin_embedder.html";
  const std::string embedder_code = StringPrintf("SetSize(%d, %d);", 100, 100);
  StartBrowserPluginTest(kEmbedderURL, kHTMLForGuestWithSize, true,
                         embedder_code);
  // Check for render view host at position (150, 150) that is outside the
  // bounds of our guest, so this would respond with the render view host of the
  // embedder.
  test_embedder()->WaitForRenderViewHostAtPosition(150, 150);
  ASSERT_EQ(test_embedder()->web_contents()->GetRenderViewHost(),
            test_embedder()->last_rvh_at_position_response());
}

IN_PROC_BROWSER_TEST_F(BrowserPluginHostTest, ChangeWindowName) {
  const char kEmbedderURL[] = "files/browser_plugin_naming_embedder.html";
  const char* kGuestURL = "files/browser_plugin_naming_guest.html";
  StartBrowserPluginTest(kEmbedderURL, kGuestURL, false, "");

  RenderViewHostImpl* rvh = static_cast<RenderViewHostImpl*>(
      test_embedder()->web_contents()->GetRenderViewHost());
  // Verify that the plugin's name is properly initialized.
  {
    scoped_ptr<base::Value> value(rvh->ExecuteJavascriptAndGetValue(string16(),
        ASCIIToUTF16("document.getElementById('plugin').name")));
    std::string result;
    EXPECT_TRUE(value->GetAsString(&result));
    EXPECT_EQ("start", result);
  }
  {
    // Open a channel with the guest, wait until it replies,
    // then verify that the plugin's name has been updated.
    const string16 expected_title = ASCIIToUTF16("guest");
    content::TitleWatcher title_watcher(test_embedder()->web_contents(),
                                        expected_title);
    ExecuteSyncJSFunction(rvh, "OpenCommChannel();");
    string16 actual_title = title_watcher.WaitAndGetTitle();
    EXPECT_EQ(expected_title, actual_title);

    scoped_ptr<base::Value> value(rvh->ExecuteJavascriptAndGetValue(string16(),
        ASCIIToUTF16("document.getElementById('plugin').name")));
    std::string result;
    EXPECT_TRUE(value->GetAsString(&result));
    EXPECT_EQ("guest", result);
  }
  {
    // Set the plugin's name and verify that the window.name of the guest
    // has been updated.
    const string16 expected_title = ASCIIToUTF16("foobar");
    content::TitleWatcher title_watcher(test_embedder()->web_contents(),
                                        expected_title);
    ExecuteSyncJSFunction(rvh,
        "document.getElementById('plugin').name = 'foobar';");
    string16 actual_title = title_watcher.WaitAndGetTitle();
    EXPECT_EQ(expected_title, actual_title);

  }
}

}  // namespace content
