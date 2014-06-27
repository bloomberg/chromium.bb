// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_BROWSER_TEST_UTILS_H_
#define CONTENT_PUBLIC_TEST_BROWSER_TEST_UTILS_H_

#include <queue>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/compiler_specific.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/ref_counted.h"
#include "base/process/process.h"
#include "base/strings/string16.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/render_process_host_observer.h"
#include "content/public/browser/web_contents_observer.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "url/gurl.h"

#if defined(OS_WIN)
#include "base/win/scoped_handle.h"
#endif

namespace base {
class RunLoop;
}

namespace gfx {
class Point;
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
class WebContents;

// Generate a URL for a file path including a query string.
GURL GetFileUrlWithQuery(const base::FilePath& path,
                         const std::string& query_string);

// Waits for a load stop for the specified |web_contents|'s controller, if the
// tab is currently web_contents.  Otherwise returns immediately.
void WaitForLoadStop(WebContents* web_contents);

#if defined(USE_AURA)
// If WebContent's view is currently being resized, this will wait for the ack
// from the renderer that the resize is complete and for the
// WindowEventDispatcher to release the pointer moves. If there's no resize in
// progress, the method will return right away.
void WaitForResizeComplete(WebContents* web_contents);
#endif  // USE_AURA

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

// Taps the screen at |point|.
void SimulateTapAt(WebContents* web_contents, const gfx::Point& point);

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
                              const char* code,
                              bool control,
                              bool shift,
                              bool alt,
                              bool command);

namespace internal {
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
}  // namespace internal

// Executes the passed |script| in the specified frame. The |script| should not
// invoke domAutomationController.send(); otherwise, your test will hang or be
// flaky. If you want to extract a result, use one of the below functions.
// Returns true on success.
bool ExecuteScript(const internal::ToRenderFrameHost& adapter,
                   const std::string& script) WARN_UNUSED_RESULT;

// The following methods executes the passed |script| in the specified frame and
// sets |result| to the value passed to "window.domAutomationController.send" by
// the executed script. They return true on success, false if the script
// execution failed or did not evaluate to the expected type.
bool ExecuteScriptAndExtractInt(const internal::ToRenderFrameHost& adapter,
                                const std::string& script,
                                int* result) WARN_UNUSED_RESULT;
bool ExecuteScriptAndExtractBool(const internal::ToRenderFrameHost& adapter,
                                 const std::string& script,
                                 bool* result) WARN_UNUSED_RESULT;
bool ExecuteScriptAndExtractString(const internal::ToRenderFrameHost& adapter,
                                   const std::string& script,
                                   std::string* result) WARN_UNUSED_RESULT;

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

// Watches title changes on a WebContents, blocking until an expected title is
// set.
class TitleWatcher : public WebContentsObserver {
 public:
  // |web_contents| must be non-NULL and needs to stay alive for the
  // entire lifetime of |this|. |expected_title| is the title that |this|
  // will wait for.
  TitleWatcher(WebContents* web_contents,
               const base::string16& expected_title);
  virtual ~TitleWatcher();

  // Adds another title to watch for.
  void AlsoWaitForTitle(const base::string16& expected_title);

  // Waits until the title matches either expected_title or one of the titles
  // added with AlsoWaitForTitle. Returns the value of the most recently
  // observed matching title.
  const base::string16& WaitAndGetTitle() WARN_UNUSED_RESULT;

 private:
  // Overridden WebContentsObserver methods.
  virtual void DidStopLoading(RenderViewHost* render_view_host) OVERRIDE;
  virtual void TitleWasSet(NavigationEntry* entry, bool explicit_set) OVERRIDE;

  void TestTitle();

  std::vector<base::string16> expected_titles_;
  scoped_refptr<MessageLoopRunner> message_loop_runner_;

  // The most recently observed expected title, if any.
  base::string16 observed_title_;

  DISALLOW_COPY_AND_ASSIGN(TitleWatcher);
};

// Watches a WebContents and blocks until it is destroyed.
class WebContentsDestroyedWatcher : public WebContentsObserver {
 public:
  explicit WebContentsDestroyedWatcher(WebContents* web_contents);
  virtual ~WebContentsDestroyedWatcher();

  // Waits until the WebContents is destroyed.
  void Wait();

 private:
  // Overridden WebContentsObserver methods.
  virtual void WebContentsDestroyed() OVERRIDE;

  scoped_refptr<MessageLoopRunner> message_loop_runner_;

  DISALLOW_COPY_AND_ASSIGN(WebContentsDestroyedWatcher);
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
  virtual ~RenderProcessHostWatcher();

  // Waits until the renderer process exits.
  void Wait();

 private:
  // Overridden RenderProcessHost::LifecycleObserver methods.
  virtual void RenderProcessExited(RenderProcessHost* host,
                                   base::ProcessHandle handle,
                                   base::TerminationStatus status,
                                   int exit_code) OVERRIDE;
  virtual void RenderProcessHostDestroyed(RenderProcessHost* host) OVERRIDE;

  RenderProcessHost* render_process_host_;
  WatchType type_;

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
  virtual ~DOMMessageQueue();

  // Removes all messages in the message queue.
  void ClearQueue();

  // Wait for the next message to arrive. |message| will be set to the next
  // message. Returns true on success.
  bool WaitForMessage(std::string* message) WARN_UNUSED_RESULT;

  // Overridden NotificationObserver methods.
  virtual void Observe(int type,
                       const NotificationSource& source,
                       const NotificationDetails& details) OVERRIDE;

 private:
  NotificationRegistrar registrar_;
  std::queue<std::string> message_queue_;
  scoped_refptr<MessageLoopRunner> message_loop_runner_;

  DISALLOW_COPY_AND_ASSIGN(DOMMessageQueue);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_BROWSER_TEST_UTILS_H_
