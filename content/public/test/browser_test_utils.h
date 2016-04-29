// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_BROWSER_TEST_UTILS_H_
#define CONTENT_PUBLIC_TEST_BROWSER_TEST_UTILS_H_

#include <queue>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/process/process.h"
#include "base/strings/string16.h"
#include "build/build_config.h"
#include "cc/output/compositor_frame.h"
#include "content/public/browser/browser_message_filter.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/render_process_host_observer.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/page_type.h"
#include "ipc/message_filter.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "url/gurl.h"

#if defined(OS_WIN)
#include "base/win/scoped_handle.h"
#endif

namespace gfx {
class Point;
}

namespace net {
namespace test_server {
class EmbeddedTestServer;
}
// TODO(svaldez): Remove typedef once EmbeddedTestServer has been migrated
// out of net::test_server.
using test_server::EmbeddedTestServer;
}

// A collections of functions designed for use with content_browsertests and
// browser_tests.
// TO BE CLEAR: any function here must work against both binaries. If it only
// works with browser_tests, it should be in chrome\test\base\ui_test_utils.h.
// If it only works with content_browsertests, it should be in
// content\test\content_browser_test_utils.h.

namespace content {

class BrowserContext;
class MessageLoopRunner;
class RenderViewHost;
class RenderWidgetHost;
class WebContents;

// Navigate a frame with ID |iframe_id| to |url|, blocking until the navigation
// finishes.  Uses a renderer-initiated navigation from script code in the
// main frame.
bool NavigateIframeToURL(WebContents* web_contents,
                         std::string iframe_id,
                         const GURL& url);

// Generate a URL for a file path including a query string.
GURL GetFileUrlWithQuery(const base::FilePath& path,
                         const std::string& query_string);

// Checks whether the page type of the last committed navigation entry matches
// |page_type|.
bool IsLastCommittedEntryOfPageType(WebContents* web_contents,
                                    content::PageType page_type);

// Waits for |web_contents| to stop loading.  If |web_contents| is not loading
// returns immediately.  Tests should use WaitForLoadStop instead and check that
// last navigation succeeds, and this function should only be used if the
// navigation leads to web_contents being destroyed.
void WaitForLoadStopWithoutSuccessCheck(WebContents* web_contents);

// Waits for |web_contents| to stop loading.  If |web_contents| is not loading
// returns immediately.  Returns true if the last navigation succeeded (resulted
// in a committed navigation entry of type PAGE_TYPE_NORMAL).
// TODO(alexmos): tests that use this function to wait for successful
// navigations should be refactored to do EXPECT_TRUE(WaitForLoadStop()).
bool WaitForLoadStop(WebContents* web_contents);

#if defined(USE_AURA) || defined(OS_ANDROID)
// If WebContent's view is currently being resized, this will wait for the ack
// from the renderer that the resize is complete and for the
// WindowEventDispatcher to release the pointer moves. If there's no resize in
// progress, the method will return right away.
void WaitForResizeComplete(WebContents* web_contents);
#endif  // defined(USE_AURA) || defined(OS_ANDROID)

// Causes the specified web_contents to crash. Blocks until it is crashed.
void CrashTab(WebContents* web_contents);

// Simulates clicking at the center of the given tab asynchronously; modifiers
// may contain bits from WebInputEvent::Modifiers.
void SimulateMouseClick(WebContents* web_contents,
                        int modifiers,
                        blink::WebMouseEvent::Button button);

// Simulates clicking at the point |point| of the given tab asynchronously;
// modifiers may contain bits from WebInputEvent::Modifiers.
void SimulateMouseClickAt(WebContents* web_contents,
                          int modifiers,
                          blink::WebMouseEvent::Button button,
                          const gfx::Point& point);

// Simulates asynchronously a mouse enter/move/leave event.
void SimulateMouseEvent(WebContents* web_contents,
                        blink::WebInputEvent::Type type,
                        const gfx::Point& point);

// Simulate a mouse wheel event.
void SimulateMouseWheelEvent(WebContents* web_contents,
                             const gfx::Point& point,
                             const gfx::Vector2d& delta);

// Sends a simple, three-event (Begin/Update/End) gesture scroll.
void SimulateGestureScrollSequence(WebContents* web_contents,
                                   const gfx::Point& point,
                                   const gfx::Vector2dF& delta);

// Taps the screen at |point|.
void SimulateTapAt(WebContents* web_contents, const gfx::Point& point);

#if defined(USE_AURA)
// Generates a TouchStart at |point|.
void SimulateTouchPressAt(WebContents* web_contents, const gfx::Point& point);
#endif

// Taps the screen with modifires at |point|.
void SimulateTapWithModifiersAt(WebContents* web_contents,
                                unsigned Modifiers,
                                const gfx::Point& point);

// Sends a key press asynchronously.
// The native code of the key event will be set to InvalidNativeKeycode().
// |key_code| alone is good enough for scenarios that only need the char
// value represented by a key event and not the physical key on the keyboard
// or the keyboard layout.
// For scenarios such as chromoting that need the native code,
// SimulateKeyPressWithCode should be used.
void SimulateKeyPress(WebContents* web_contents,
                      ui::KeyboardCode key_code,
                      bool control,
                      bool shift,
                      bool alt,
                      bool command);

// Sends a key press asynchronously.
// |code| specifies the UIEvents (aka: DOM4Events) value of the key:
// https://dvcs.w3.org/hg/d4e/raw-file/tip/source_respec.htm
// The native code of the key event will be set based on |code|.
// See ui/base/keycodes/vi usb_keycode_map.h for mappings between |code|
// and the native code.
// Examples of the various codes:
//   key_code: VKEY_A
//   code: "KeyA"
//   native key code: 0x001e (for Windows).
//   native key code: 0x0026 (for Linux).
void SimulateKeyPressWithCode(WebContents* web_contents,
                              ui::KeyboardCode key_code,
                              const std::string& code,
                              bool control,
                              bool shift,
                              bool alt,
                              bool command);

// Allow ExecuteScript* methods to target either a WebContents or a
// RenderFrameHost.  Targetting a WebContents means executing the script in the
// RenderFrameHost returned by WebContents::GetMainFrame(), which is the
// main frame.  Pass a specific RenderFrameHost to target it.
class ToRenderFrameHost {
 public:
  ToRenderFrameHost(WebContents* web_contents);
  ToRenderFrameHost(RenderViewHost* render_view_host);
  ToRenderFrameHost(RenderFrameHost* render_frame_host);

  RenderFrameHost* render_frame_host() const { return render_frame_host_; }

 private:
  RenderFrameHost* render_frame_host_;
};

// Executes the passed |script| in the specified frame. The |script| should not
// invoke domAutomationController.send(); otherwise, your test will hang or be
// flaky. If you want to extract a result, use one of the below functions.
// Returns true on success.
bool ExecuteScript(const ToRenderFrameHost& adapter,
                   const std::string& script) WARN_UNUSED_RESULT;

// The following methods executes the passed |script| in the specified frame and
// sets |result| to the value passed to "window.domAutomationController.send" by
// the executed script. They return true on success, false if the script
// execution failed or did not evaluate to the expected type.
bool ExecuteScriptAndExtractDouble(const ToRenderFrameHost& adapter,
                                   const std::string& script,
                                   double* result) WARN_UNUSED_RESULT;
bool ExecuteScriptAndExtractInt(const ToRenderFrameHost& adapter,
                                const std::string& script,
                                int* result) WARN_UNUSED_RESULT;
bool ExecuteScriptAndExtractBool(const ToRenderFrameHost& adapter,
                                 const std::string& script,
                                 bool* result) WARN_UNUSED_RESULT;
bool ExecuteScriptAndExtractString(const ToRenderFrameHost& adapter,
                                   const std::string& script,
                                   std::string* result) WARN_UNUSED_RESULT;

// This function behaves similarly to ExecuteScriptAndExtractBool but runs the
// the script in the specified isolated world.
bool ExecuteScriptInIsolatedWorldAndExtractBool(
    const ToRenderFrameHost& adapter,
    const int world_id,
    const std::string& script,
    bool* result) WARN_UNUSED_RESULT;

// Walks the frame tree of the specified WebContents and returns the sole frame
// that matches the specified predicate function. This function will DCHECK if
// no frames match the specified predicate, or if more than one frame matches.
RenderFrameHost* FrameMatchingPredicate(
    WebContents* web_contents,
    const base::Callback<bool(RenderFrameHost*)>& predicate);

// Predicates for use with FrameMatchingPredicate.
bool FrameMatchesName(const std::string& name, RenderFrameHost* frame);
bool FrameIsChildOfMainFrame(RenderFrameHost* frame);
bool FrameHasSourceUrl(const GURL& url, RenderFrameHost* frame);

// Finds the child frame at the specified |index| for |frame| and returns its
// RenderFrameHost.  Returns nullptr if such child frame does not exist.
RenderFrameHost* ChildFrameAt(RenderFrameHost* frame, size_t index);

// Executes the WebUI resource test runner injecting each resource ID in
// |js_resource_ids| prior to executing the tests.
//
// Returns true if tests ran successfully, false otherwise.
bool ExecuteWebUIResourceTest(WebContents* web_contents,
                              const std::vector<int>& js_resource_ids);

// Returns the cookies for the given url.
std::string GetCookies(BrowserContext* browser_context, const GURL& url);

// Sets a cookie for the given url. Returns true on success.
bool SetCookie(BrowserContext* browser_context,
               const GURL& url,
               const std::string& value);

// Fetch the histograms data from other processes. This should be called after
// the test code has been executed but before performing assertions.
void FetchHistogramsFromChildProcesses();

// Registers a request handler which redirects to a different host, based
// on the request path. The format of the path should be
// "/cross-site/hostname/rest/of/path" to redirect the request to
// "<scheme>://hostname:<port>/rest/of/path", where <scheme> and <port>
// are the values for the instance of EmbeddedTestServer.
void SetupCrossSiteRedirector(net::EmbeddedTestServer* embedded_test_server);

// Waits for an interstitial page to attach to given web contents.
void WaitForInterstitialAttach(content::WebContents* web_contents);

// Waits for an interstitial page to detach from given web contents.
void WaitForInterstitialDetach(content::WebContents* web_contents);

// Runs task and waits for an interstitial page to detach from given web
// contents. Prefer this over WaitForInterstitialDetach if web_contents may be
// destroyed by the time WaitForInterstitialDetach is called (e.g. when waiting
// for an interstitial detach after closing a tab).
void RunTaskAndWaitForInterstitialDetach(content::WebContents* web_contents,
                                         const base::Closure& task);

// Waits until all resources have loaded in the given RenderFrameHost.
// When the load completes, this function sends a "pageLoadComplete" message
// via domAutomationController. The caller should make sure this extra
// message is handled properly.
bool WaitForRenderFrameReady(RenderFrameHost* rfh) WARN_UNUSED_RESULT;

// Enable accessibility support for all of the frames in this WebContents
void EnableAccessibilityForWebContents(WebContents* web_contents);

// Wait until the focused accessible node changes in any WebContents.
void WaitForAccessibilityFocusChange();

// Retrieve information about the node that's focused in the accessibility tree.
ui::AXNodeData GetFocusedAccessibilityNodeInfo(WebContents* web_contents);

// Watches title changes on a WebContents, blocking until an expected title is
// set.
class TitleWatcher : public WebContentsObserver {
 public:
  // |web_contents| must be non-NULL and needs to stay alive for the
  // entire lifetime of |this|. |expected_title| is the title that |this|
  // will wait for.
  TitleWatcher(WebContents* web_contents,
               const base::string16& expected_title);
  ~TitleWatcher() override;

  // Adds another title to watch for.
  void AlsoWaitForTitle(const base::string16& expected_title);

  // Waits until the title matches either expected_title or one of the titles
  // added with AlsoWaitForTitle. Returns the value of the most recently
  // observed matching title.
  const base::string16& WaitAndGetTitle() WARN_UNUSED_RESULT;

 private:
  // Overridden WebContentsObserver methods.
  void DidStopLoading() override;
  void TitleWasSet(NavigationEntry* entry, bool explicit_set) override;

  void TestTitle();

  std::vector<base::string16> expected_titles_;
  scoped_refptr<MessageLoopRunner> message_loop_runner_;

  // The most recently observed expected title, if any.
  base::string16 observed_title_;

  DISALLOW_COPY_AND_ASSIGN(TitleWatcher);
};

// Watches a RenderProcessHost and waits for specified destruction events.
class RenderProcessHostWatcher : public RenderProcessHostObserver {
 public:
  enum WatchType {
    WATCH_FOR_PROCESS_EXIT,
    WATCH_FOR_HOST_DESTRUCTION
  };

  RenderProcessHostWatcher(RenderProcessHost* render_process_host,
                           WatchType type);
  // Waits for the render process that contains the specified web contents.
  RenderProcessHostWatcher(WebContents* web_contents, WatchType type);
  ~RenderProcessHostWatcher() override;

  // Waits until the renderer process exits.
  void Wait();

  // Returns true if a renderer process exited cleanly (without hitting
  // RenderProcessExited with an abnormal TerminationStatus). This should be
  // called after Wait().
  bool did_exit_normally() { return did_exit_normally_; }

 private:
  // Overridden RenderProcessHost::LifecycleObserver methods.
  void RenderProcessExited(RenderProcessHost* host,
                           base::TerminationStatus status,
                           int exit_code) override;
  void RenderProcessHostDestroyed(RenderProcessHost* host) override;

  RenderProcessHost* render_process_host_;
  WatchType type_;
  bool did_exit_normally_;

  scoped_refptr<MessageLoopRunner> message_loop_runner_;

  DISALLOW_COPY_AND_ASSIGN(RenderProcessHostWatcher);
};

// Watches for responses from the DOMAutomationController and keeps them in a
// queue. Useful for waiting for a message to be received.
class DOMMessageQueue : public NotificationObserver {
 public:
  // Constructs a DOMMessageQueue and begins listening for messages from the
  // DOMAutomationController. Do not construct this until the browser has
  // started.
  DOMMessageQueue();
  ~DOMMessageQueue() override;

  // Removes all messages in the message queue.
  void ClearQueue();

  // Wait for the next message to arrive. |message| will be set to the next
  // message. Returns true on success.
  bool WaitForMessage(std::string* message) WARN_UNUSED_RESULT;

  // Overridden NotificationObserver methods.
  void Observe(int type,
               const NotificationSource& source,
               const NotificationDetails& details) override;

 private:
  NotificationRegistrar registrar_;
  std::queue<std::string> message_queue_;
  scoped_refptr<MessageLoopRunner> message_loop_runner_;

  DISALLOW_COPY_AND_ASSIGN(DOMMessageQueue);
};

// Used to wait for a new WebContents to be created. Instantiate this object
// before the operation that will create the window.
class WebContentsAddedObserver {
 public:
  WebContentsAddedObserver();
  ~WebContentsAddedObserver();

  // Will run a message loop to wait for the new window if it hasn't been
  // created since the constructor
  WebContents* GetWebContents();

  // Will tell whether RenderViewCreated Callback has invoked
  bool RenderViewCreatedCalled();

 private:
  class RenderViewCreatedObserver;

  void WebContentsCreated(WebContents* web_contents);

  // Callback to WebContentCreated(). Cached so that we can unregister it.
  base::Callback<void(WebContents*)> web_contents_created_callback_;

  WebContents* web_contents_;
  std::unique_ptr<RenderViewCreatedObserver> child_observer_;
  scoped_refptr<MessageLoopRunner> runner_;

  DISALLOW_COPY_AND_ASSIGN(WebContentsAddedObserver);
};

// Request a new frame be drawn, returns false if request fails.
bool RequestFrame(WebContents* web_contents);

// Watches compositor frame changes, blocking until a frame has been
// composited. This class is intended to be run on the main thread; to
// synchronize the main thread against the impl thread.
class FrameWatcher : public IPC::MessageFilter {
 public:
  FrameWatcher();

  // Listen for new frames from the |web_contents| renderer process.
  void AttachTo(WebContents* web_contents);

  // Wait for |frames_to_wait| swap mesages from the compositor.
  void WaitFrames(int frames_to_wait);

  // Return the meta data received in the last compositor
  // swap frame.
  const cc::CompositorFrameMetadata& LastMetadata();

 private:
  ~FrameWatcher() override;

  // Overridden BrowserMessageFilter methods.
  bool OnMessageReceived(const IPC::Message& message) override;

  void ReceivedFrameSwap(cc::CompositorFrameMetadata meta_data);

  int frames_to_wait_;
  base::Closure quit_;
  cc::CompositorFrameMetadata last_metadata_;

  DISALLOW_COPY_AND_ASSIGN(FrameWatcher);
};

// This class is intended to synchronize the renderer main thread, renderer impl
// thread and the browser main thread.
class MainThreadFrameObserver : public IPC::Listener {
 public:
  explicit MainThreadFrameObserver(RenderWidgetHost* render_widget_host);
  ~MainThreadFrameObserver() override;

  // Synchronizes the browser main thread with the renderer main thread and impl
  // thread.
  void Wait();

  // Overridden IPC::Listener methods.
  bool OnMessageReceived(const IPC::Message& msg) override;

 private:
  void Quit();

  RenderWidgetHost* render_widget_host_;
  std::unique_ptr<base::RunLoop> run_loop_;
  int routing_id_;

  DISALLOW_COPY_AND_ASSIGN(MainThreadFrameObserver);
};

// Watches for an input msg to be consumed.
class InputMsgWatcher : public BrowserMessageFilter {
 public:
  InputMsgWatcher(RenderWidgetHost* render_widget_host,
                  blink::WebInputEvent::Type type);

  // Wait until ack message occurs, returning the ack result from
  // the message.
  uint32_t WaitForAck();

 private:
  ~InputMsgWatcher() override;

  // Overridden BrowserMessageFilter methods.
  bool OnMessageReceived(const IPC::Message& message) override;

  void ReceivedAck(blink::WebInputEvent::Type ack_type, uint32_t ack_state);

  blink::WebInputEvent::Type wait_for_type_;
  uint32_t ack_result_;
  base::Closure quit_;

  DISALLOW_COPY_AND_ASSIGN(InputMsgWatcher);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_BROWSER_TEST_UTILS_H_
