// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_BROWSER_TEST_UTILS_H_
#define CONTENT_PUBLIC_TEST_BROWSER_TEST_UTILS_H_

#include <vector>

#include "base/callback_forward.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/process.h"
#include "base/scoped_temp_dir.h"
#include "base/string16.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "googleurl/src/gurl.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebInputEvent.h"
#include "ui/base/keycodes/keyboard_codes.h"

#if defined(OS_WIN)
#include "base/win/scoped_handle.h"
#endif

class CommandLine;

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

class MessageLoopRunner;
class RenderViewHost;
class WebContents;

// Generate a URL for a file path including a query string.
GURL GetFileUrlWithQuery(const FilePath& path, const std::string& query_string);

// Waits for a load stop for the specified |web_contents|'s controller, if the
// tab is currently web_contents.  Otherwise returns immediately.
void WaitForLoadStop(WebContents* web_contents);

// Causes the specified web_contents to crash. Blocks until it is crashed.
void CrashTab(WebContents* web_contents);

// Simulates clicking at the center of the given tab asynchronously.
void SimulateMouseClick(WebContents* web_contents);

// Simulates asynchronously a mouse enter/move/leave event.
void SimulateMouseEvent(WebContents* web_contents,
                        WebKit::WebInputEvent::Type type,
                        const gfx::Point& point);

// Sends a key press asynchronously.
void SimulateKeyPress(WebContents* web_contents,
                      ui::KeyboardCode key,
                      bool control,
                      bool shift,
                      bool alt,
                      bool command);

// Executes the passed |script| in the frame pointed to by |frame_xpath| (use
// empty string for main frame).  The |script| should not invoke
// domAutomationController.send(); otherwise, your test will hang or be flaky.
// If you want to extract a result, use one of the below functions.
// Returns true on success.
bool ExecuteJavaScript(RenderViewHost* render_view_host,
                       const std::wstring& frame_xpath,
                       const std::wstring& script) WARN_UNUSED_RESULT;

// The following methods executes the passed |script| in the frame pointed to by
// |frame_xpath| (use empty string for main frame) and sets |result| to the
// value returned by the script evaluation.
// They return true on success, false if the script evaluation failed or did not
// evaluate to the expected type.
bool ExecuteJavaScriptAndExtractInt(RenderViewHost* render_view_host,
                                    const std::wstring& frame_xpath,
                                    const std::wstring& script,
                                    int* result) WARN_UNUSED_RESULT;
bool ExecuteJavaScriptAndExtractBool(RenderViewHost* render_view_host,
                                     const std::wstring& frame_xpath,
                                     const std::wstring& script,
                                     bool* result) WARN_UNUSED_RESULT;
bool ExecuteJavaScriptAndExtractString(
    RenderViewHost* render_view_host,
    const std::wstring& frame_xpath,
    const std::wstring& script,
    std::string* result) WARN_UNUSED_RESULT;

// Watches title changes on a tab, blocking until an expected title is set.
class TitleWatcher : public NotificationObserver {
 public:
  // |web_contents| must be non-NULL and needs to stay alive for the
  // entire lifetime of |this|. |expected_title| is the title that |this|
  // will wait for.
  TitleWatcher(WebContents* web_contents,
               const string16& expected_title);
  virtual ~TitleWatcher();

  // Adds another title to watch for.
  void AlsoWaitForTitle(const string16& expected_title);

  // Waits until the title matches either expected_title or one of the titles
  // added with  AlsoWaitForTitle.  Returns the value of the most recently
  // observed matching title.
  const string16& WaitAndGetTitle() WARN_UNUSED_RESULT;

 private:
  // NotificationObserver
  virtual void Observe(int type,
                       const NotificationSource& source,
                       const NotificationDetails& details) OVERRIDE;

  WebContents* web_contents_;
  std::vector<string16> expected_titles_;
  NotificationRegistrar notification_registrar_;
  scoped_refptr<MessageLoopRunner> message_loop_runner_;

  // The most recently observed expected title, if any.
  string16 observed_title_;

  bool expected_title_observed_;
  bool quit_loop_on_observation_;

  DISALLOW_COPY_AND_ASSIGN(TitleWatcher);
};

// This is a utility class for running a python websocket server
// during tests. The server is started during the construction of the
// object, and is stopped when the destructor is called. Note that
// because of the underlying script that is used:
//
//    third_paty/WebKit/Tools/Scripts/new-run-webkit-websocketserver
//
// Only *_wsh.py handlers found under "http/tests/websocket/tests" from the
// |root_directory| will be found and active while running the test
// server.
class TestWebSocketServer {
 public:
  TestWebSocketServer();

  // Stops the python websocket server if it was already started.
  ~TestWebSocketServer();

  // Use a random port, useful for tests that are sharded. Returns the port.
  int UseRandomPort();

  // Serves with TLS.
  void UseTLS();

  // Starts the python websocket server using |root_directory|. Returns whether
  // the server was successfully started.
  bool Start(const FilePath& root_directory);

 private:
  // Sets up PYTHONPATH to run websocket_server.py.
  void SetPythonPath();

  // Creates a CommandLine for invoking the python interpreter.
  CommandLine* CreatePythonCommandLine();

  // Creates a CommandLine for invoking the python websocker server.
  CommandLine* CreateWebSocketServerCommandLine();

  // Has the server been started?
  bool started_;

  // A Scoped temporary directory for holding the python pid file.
  ScopedTempDir temp_dir_;

  // Used to close the same python interpreter when server falls out
  // scope.
  FilePath websocket_pid_file_;

#if defined(OS_POSIX)
  // ProcessHandle used to terminate child process.
  base::ProcessHandle process_group_id_;
#elif defined(OS_WIN)
  // JobObject used to clean up orphaned child process.
  base::win::ScopedHandle job_handle_;
#endif

  // Holds port number which the python websocket server uses.
  int port_;

  // If the python websocket server serves with TLS.
  bool secure_;

  DISALLOW_COPY_AND_ASSIGN(TestWebSocketServer);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_BROWSER_TEST_UTILS_H_
