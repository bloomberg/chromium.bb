// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_WEBDRIVER_SESSION_H_
#define CHROME_TEST_WEBDRIVER_SESSION_H_

#include <map>
#include <string>

#include "base/scoped_ptr.h"
#include "base/string16.h"
#include "chrome/test/webdriver/automation.h"
#include "chrome/test/webdriver/error_codes.h"

class GURL;
class ListValue;
class Value;

namespace base {
class WaitableEvent;
}

namespace webdriver {

// Every connection made by WebDriver maps to a session object.
// This object creates the chrome instance and keeps track of the
// state necessary to control the chrome browser created.
// A session manages its own lifetime.
// TODO(phajdan.jr):  Abstract UITestBase classes, see:
// http://code.google.com/p/chromium/issues/detail?id=56865
class Session {
 public:
  // Adds this |Session| to the |SessionManager|. The session manages its own
  // lifetime. Do not call delete.
  Session();
  // Removes this |Session| from the |SessionManager|.
  ~Session();

  // Starts the session thread and a new browser. Returns true on
  // success. On failure, the session will delete itself and return false.
  bool Init();

  // Terminates this session and deletes itself.
  void Terminate();

  // Executes the given |script| in the context of the given window and frame.
  // The |script| should be in the form of a function body
  // (e.g. "return arguments[0]"), where |args| is the list of arguments to
  // pass to the function. The caller is responsible for the script result
  // |value|.
  ErrorCode ExecuteScript(int window_id,
                          const std::string& frame_xpath,
                          const std::string& script,
                          const ListValue* const args,
                          Value** value);

  // Same as above, but uses the currently targeted window and frame.
  ErrorCode ExecuteScript(const std::string& script,
                          const ListValue* const args,
                          Value** value);

  // Send the given keys to the given element dictionary. This function takes
  // ownership of |element|.
  ErrorCode SendKeys(DictionaryValue* element, const string16& keys);

  bool NavigateToURL(const std::string& url);
  bool GoForward();
  bool GoBack();
  bool Reload();
  bool GetURL(GURL* url);
  bool GetURL(std::string* url);
  bool GetTabTitle(std::string* tab_title);
  bool GetCookies(const GURL& url, std::string* cookies);
  bool GetCookieByName(const GURL& url, const std::string& cookie_name,
             std::string* cookie);
  bool DeleteCookie(const GURL& url, const std::string& cookie_name);
  bool SetCookie(const GURL& url, const std::string& cookie);

  // Gets all the currently existing window IDs. Returns true on success.
  bool GetWindowIds(std::vector<int>* window_ids);

  // Switches the window used by default. |name| is either an ID returned by
  // |GetWindowIds| or the name attribute of a DOM window.
  ErrorCode SwitchToWindow(const std::string& name);

  // Switches the frame used by default. |name_or_id| is either the name or id
  // of a frame element.
  ErrorCode SwitchToFrameWithNameOrId(const std::string& name_or_id);

  // Switches the frame used by default. |index| is the zero-based frame index.
  ErrorCode SwitchToFrameWithIndex(int index);

  // Closes the current window. Returns true on success.
  // Note: The session will be deleted if this closes the last window in the
  // session.
  bool CloseWindow();

  inline const std::string& id() const { return id_; }

  inline int implicit_wait() { return implicit_wait_; }
  inline void set_implicit_wait(const int& timeout) {
    implicit_wait_ = timeout > 0 ? timeout : 0;
  }

  enum Speed { kSlow, kMedium, kFast, kUnknown };
  inline Speed speed() { return speed_; }
  inline void set_speed(Speed speed) {
    speed_ = speed;
  }

  inline const std::string& current_frame_xpath() const {
    return current_frame_xpath_;
  }

  inline void set_current_frame_xpath(const std::string& xpath) {
    current_frame_xpath_ = xpath;
  }

  inline int current_window_id() const { return current_window_id_; }

 private:
  void RunSessionTask(Task* task);
  void RunSessionTaskOnSessionThread(
      Task* task,
      base::WaitableEvent* done_event);
  void InitOnSessionThread(bool* success);
  void TerminateOnSessionThread();
  void SendKeysOnSessionThread(const string16& keys, bool* success);
  ErrorCode SwitchToFrameWithJavaScriptLocatedFrame(
      const std::string& script,
      ListValue* args);

  const std::string id_;

  scoped_ptr<Automation> automation_;
  base::Thread thread_;

  int implicit_wait_;
  Speed speed_;

  // The XPath to the frame within this session's active tab which all
  // commands should be directed to. XPath strings can represent a frame deep
  // down the tree (across multiple frame DOMs).
  // Example, /html/body/table/tbody/tr/td/iframe\n/frameset/frame[1]
  // should break into 2 xpaths
  // /html/body/table/tbody/tr/td/iframe & /frameset/frame[1].
  std::string current_frame_xpath_;
  int current_window_id_;

  DISALLOW_COPY_AND_ASSIGN(Session);
};

}  // namespace webdriver

DISABLE_RUNNABLE_METHOD_REFCOUNT(webdriver::Session);

#endif  // CHROME_TEST_WEBDRIVER_SESSION_H_
