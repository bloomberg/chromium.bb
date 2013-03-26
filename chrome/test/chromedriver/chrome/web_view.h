// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_CHROMEDRIVER_CHROME_WEB_VIEW_H_
#define CHROME_TEST_CHROMEDRIVER_CHROME_WEB_VIEW_H_

#include <list>
#include <string>

#include "base/memory/scoped_ptr.h"

namespace base {
class ListValue;
class Value;
}

struct Geoposition;
class JavaScriptDialogManager;
struct KeyEvent;
struct MouseEvent;
class Status;

class WebView {
 public:
  virtual ~WebView() {}

  // Return the id for this WebView.
  virtual std::string GetId() = 0;

  // Make DevToolsCient connect to DevTools if it is disconnected.
  virtual Status ConnectIfNecessary() = 0;

  // Close the WebView itself.
  virtual Status Close() = 0;

  // Load a given URL in the main frame.
  virtual Status Load(const std::string& url) = 0;

  // Reload the current page.
  virtual Status Reload() = 0;

  // Evaluates a JavaScript expression in a specified frame and returns
  // the result. |frame| is a frame ID or an empty string for the main frame.
  // If the expression evaluates to a element, it will be bound to a unique ID
  // (per frame) and the ID will be returned.
  virtual Status EvaluateScript(const std::string& frame,
                                const std::string& expression,
                                scoped_ptr<base::Value>* result) = 0;

  // Calls a JavaScript function in a specified frame with the given args and
  // returns the result. |frame| is a frame ID or an empty string for the main
  // frame. |args| may contain IDs that refer to previously returned elements.
  // These will be translated back to their referred objects before invoking the
  // function.
  virtual Status CallFunction(const std::string& frame,
                              const std::string& function,
                              const base::ListValue& args,
                              scoped_ptr<base::Value>* result) = 0;

  // Gets the frame ID for a frame element returned by invoking the given
  // JavaScript function. |frame| is a frame ID or an empty string for the main
  // frame.
  virtual Status GetFrameByFunction(const std::string& frame,
                                    const std::string& function,
                                    const base::ListValue& args,
                                    std::string* out_frame) = 0;

  // Dispatch a sequence of mouse events.
  virtual Status DispatchMouseEvents(const std::list<MouseEvent>& events) = 0;

  // Dispatch a sequence of key events.
  virtual Status DispatchKeyEvents(const std::list<KeyEvent>& events) = 0;

  // Return all the cookies visible to the current page.
  virtual Status GetCookies(scoped_ptr<base::ListValue>* cookies) = 0;

  // Delete the cookie with the given name.
  virtual Status DeleteCookie(const std::string& name,
                              const std::string& url) = 0;

  // Waits until all pending navigations have completed in the given frame.
  // If |frame_id| is "", waits for navigations on the main frame.
  virtual Status WaitForPendingNavigations(const std::string& frame_id) = 0;

  // Returns whether the frame is pending navigation.
  virtual Status IsPendingNavigation(
      const std::string& frame_id, bool* is_pending) = 0;

  // Returns the frame id for the main frame.
  virtual Status GetMainFrame(std::string* out_frame) = 0;

  // Returns the JavaScriptDialogManager. Never null.
  virtual JavaScriptDialogManager* GetJavaScriptDialogManager() = 0;

  // Overrides normal geolocation with a given geoposition.
  virtual Status OverrideGeolocation(const Geoposition& geoposition) = 0;

  // Captures the visible portions of the web view as a base64-encoded PNG.
  virtual Status CaptureScreenshot(std::string* screenshot) = 0;
};

#endif  // CHROME_TEST_CHROMEDRIVER_CHROME_WEB_VIEW_H_
