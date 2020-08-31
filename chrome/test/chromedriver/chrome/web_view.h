// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_CHROMEDRIVER_CHROME_WEB_VIEW_H_
#define CHROME_TEST_CHROMEDRIVER_CHROME_WEB_VIEW_H_

#include <memory>
#include <string>
#include <vector>


namespace base {
class DictionaryValue;
class FilePath;
class ListValue;
class TimeDelta;
class Value;
}

class FrameTracker;
struct Geoposition;
class JavaScriptDialogManager;
struct KeyEvent;
struct MouseEvent;
struct NetworkConditions;
class Status;
class Timeout;
struct TouchEvent;

class WebView {
 public:
  virtual ~WebView() {}

  // Return the id for this WebView.
  virtual std::string GetId() = 0;

  // Return true if the web view was crashed.
  virtual bool WasCrashed() = 0;

  // Make DevToolsCient connect to DevTools if it is disconnected.
  virtual Status ConnectIfNecessary() = 0;

  // Handles events that have been received but not yet handled.
  virtual Status HandleReceivedEvents() = 0;

  // Get the current URL of the main frame.
  virtual Status GetUrl(std::string* url) = 0;

  // Load a given URL in the main frame.
  virtual Status Load(const std::string& url, const Timeout* timeout) = 0;

  // Reload the current page.
  virtual Status Reload(const Timeout* timeout) = 0;

  // Freeze the current page.
  virtual Status Freeze(const Timeout* timeout) = 0;

  // Resume the current page.
  virtual Status Resume(const Timeout* timeout) = 0;

  // Send a command to the DevTools debugger
  virtual Status SendCommand(const std::string& cmd,
                             const base::DictionaryValue& params) = 0;

  // Send a command to the DevTools debugger. Received from WebSocket
  virtual Status SendCommandFromWebSocket(const std::string& cmd,
                                          const base::DictionaryValue& params,
                                          const int client_cmd_id) = 0;

  // Send a command to the DevTools debugger and wait for the result
  virtual Status SendCommandAndGetResult(
          const std::string& cmd,
          const base::DictionaryValue& params,
          std::unique_ptr<base::Value>* value) = 0;

  // Navigate |delta| steps forward in the browser history. A negative value
  // will navigate back in the history. If the delta exceeds the number of items
  // in the browser history, stay on the current page.
  virtual Status TraverseHistory(int delta, const Timeout* timeout) = 0;

  // Evaluates a JavaScript expression in a specified frame and returns
  // the result. |frame| is a frame ID or an empty string for the main frame.
  // If the expression evaluates to a element, it will be bound to a unique ID
  // (per frame) and the ID will be returned.
  // |awaitPromise| controls awaitPromise parameter for Command
  // send to devtools backend
  // |result| will never be NULL on success.
  virtual Status EvaluateScript(const std::string& frame,
                                const std::string& expression,
                                const bool awaitPromise,
                                std::unique_ptr<base::Value>* result) = 0;

  // Calls a JavaScript function in a specified frame with the given args and
  // returns the result. |frame| is a frame ID or an empty string for the main
  // frame. |args| may contain IDs that refer to previously returned elements.
  // These will be translated back to their referred objects before invoking the
  // function.
  // |result| will never be NULL on success.
  virtual Status CallFunction(const std::string& frame,
                              const std::string& function,
                              const base::ListValue& args,
                              std::unique_ptr<base::Value>* result) = 0;

  // Calls a JavaScript function in a specified frame with the given args and
  // two callbacks. The first may be invoked with a value to return to the user.
  // The second may be used to report an error. This function waits until
  // one of the callbacks is invoked or the timeout occurs.
  // |result| will never be NULL on success.
  virtual Status CallAsyncFunction(const std::string& frame,
                                   const std::string& function,
                                   const base::ListValue& args,
                                   const base::TimeDelta& timeout,
                                   std::unique_ptr<base::Value>* result) = 0;

  // Same as |CallAsyncFunction|, except no additional error callback is passed
  // to the function. Also, |kJavaScriptError| or |kScriptTimeout| is used
  // as the error code instead of |kUnknownError| in appropriate cases.
  // |result| will never be NULL on success.
  virtual Status CallUserAsyncFunction(
      const std::string& frame,
      const std::string& function,
      const base::ListValue& args,
      const base::TimeDelta& timeout,
      std::unique_ptr<base::Value>* result) = 0;

  // Same as |CallFunction|, except |kJavaScriptError| or |kScriptTimeout| is
  // used as the error code instead of |kUnknownError| in appropriate cases, and
  // respects timeout.
  // |result| will never be NULL on success.
  virtual Status CallUserSyncScript(const std::string& frame,
                                    const std::string& script,
                                    const base::ListValue& args,
                                    const base::TimeDelta& timeout,
                                    std::unique_ptr<base::Value>* result) = 0;

  // Gets the frame ID for a frame element returned by invoking the given
  // JavaScript function. |frame| is a frame ID or an empty string for the main
  // frame.
  virtual Status GetFrameByFunction(const std::string& frame,
                                    const std::string& function,
                                    const base::ListValue& args,
                                    std::string* out_frame) = 0;

  // Dispatch a sequence of mouse events.
  virtual Status DispatchMouseEvents(const std::vector<MouseEvent>& events,
                                     const std::string& frame,
                                     bool async_dispatch_events) = 0;

  // Dispatch a single touch event.
  virtual Status DispatchTouchEvent(const TouchEvent& event,
                                    bool async_dispatch_events) = 0;

  // Dispatch a sequence of touch events.
  virtual Status DispatchTouchEvents(const std::vector<TouchEvent>& events,
                                     bool async_dispatch_events) = 0;

  // Dispatch a single touch event with more than one touch point.
  virtual Status DispatchTouchEventWithMultiPoints(
      const std::vector<TouchEvent>& events,
      bool async_dispatch_events) = 0;
  // Dispatch a sequence of key events.
  virtual Status DispatchKeyEvents(const std::vector<KeyEvent>& events,
                                   bool async_dispatch_events) = 0;

  // Return all the cookies visible to the current page.
  virtual Status GetCookies(std::unique_ptr<base::ListValue>* cookies,
                            const std::string& current_page_url) = 0;

  // Delete the cookie with the given name.
  virtual Status DeleteCookie(const std::string& name,
                              const std::string& url,
                              const std::string& domain,
                              const std::string& path) = 0;

  virtual Status AddCookie(const std::string& name,
                           const std::string& url,
                           const std::string& value,
                           const std::string& domain,
                           const std::string& path,
                           const std::string& sameSite,
                           bool secure,
                           bool httpOnly,
                           double expiry) = 0;

  // Waits until all pending navigations have completed in the given frame.
  // If |frame_id| is "", waits for navigations on the main frame.
  // If a modal dialog appears while waiting, kUnexpectedAlertOpen will be
  // returned.
  // If timeout is exceeded, will return a timeout status.
  // If |stop_load_on_timeout| is true, will attempt to stop the page load on
  // timeout before returning the timeout status.
  virtual Status WaitForPendingNavigations(const std::string& frame_id,
                                           const Timeout& timeout,
                                           bool stop_load_on_timeout) = 0;

  // Returns whether the frame is pending navigation.
  virtual Status IsPendingNavigation(const std::string& frame_id,
                                     const Timeout* timeout,
                                     bool* is_pending) const = 0;

  // Returns the JavaScriptDialogManager. Never null.
  virtual JavaScriptDialogManager* GetJavaScriptDialogManager() = 0;

  // Overrides normal geolocation with a given geoposition.
  virtual Status OverrideGeolocation(const Geoposition& geoposition) = 0;

  // Overrides normal network conditions with given conditions.
  virtual Status OverrideNetworkConditions(
      const NetworkConditions& network_conditions) = 0;

  // Overrides normal download directory with given path.
  virtual Status OverrideDownloadDirectoryIfNeeded(
      const std::string& download_directory) = 0;

  // Captures the visible portions of the web view as a base64-encoded PNG.
  virtual Status CaptureScreenshot(
      std::string* screenshot,
      const base::DictionaryValue& params) = 0;

  // Set files in a file input element.
  // |element| is the WebElement JSON Object of the input element.
  virtual Status SetFileInputFiles(const std::string& frame,
                                   const base::DictionaryValue& element,
                                   const std::vector<base::FilePath>& files,
                                   const bool append) = 0;

  // Take a heap snapshot which can build up a graph of Javascript objects.
  // A raw heap snapshot is in JSON format:
  //  1. A meta data element "snapshot" about how to parse data elements.
  //  2. Data elements: "nodes", "edges", "strings".
  virtual Status TakeHeapSnapshot(std::unique_ptr<base::Value>* snapshot) = 0;

  // Start recording Javascript CPU Profile.
  virtual Status StartProfile() = 0;

  // Stop recording Javascript CPU Profile and returns a graph of
  // CPUProfile objects. The format for the captured profile is defined
  // (by DevTools) in protocol.json.
  virtual Status EndProfile(std::unique_ptr<base::Value>* profile_data) = 0;

  virtual Status SynthesizeTapGesture(int x,
                                      int y,
                                      int tap_count,
                                      bool is_long_press) = 0;

  virtual Status SynthesizeScrollGesture(int x,
                                         int y,
                                         int xoffset,
                                         int yoffset) = 0;

  virtual bool IsNonBlocking() const = 0;

  virtual bool IsOOPIF(const std::string& frame_id) = 0;

  virtual FrameTracker* GetFrameTracker() const = 0;

  virtual std::unique_ptr<base::Value> GetCastSinks() = 0;

  virtual std::unique_ptr<base::Value> GetCastIssueMessage() = 0;

  virtual void ClearNavigationState(const std::string& new_frame_id) = 0;
};

#endif  // CHROME_TEST_CHROMEDRIVER_CHROME_WEB_VIEW_H_
