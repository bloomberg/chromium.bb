// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_WEBDRIVER_SESSION_H_
#define CHROME_TEST_WEBDRIVER_SESSION_H_

#include <string>

#include "base/scoped_ptr.h"
#include "chrome/test/webdriver/automation.h"
#include "chrome/test/webdriver/error_codes.h"

namespace base {
class WaitableEvent;
}

namespace webdriver {

// Every connection made by WebDriver maps to a session object.
// This object creates the chrome instance and keeps track of the
// state necessary to control the chrome browser created.
// TODO(phajdan.jr):  Abstract UITestBase classes, see:
// http://code.google.com/p/chromium/issues/detail?id=56865
class Session {
 public:
  explicit Session(const std::string& id);

  // Creates a browser.
  bool Init();

  // Terminates this session and disconnects its automation proxy. After
  // invoking this method, the Session can safely be deleted.
  void Terminate();

  // Executes the given |script| in the context of the frame that is currently
  // the focus of this session. The |script| should be in the form of a
  // function body (e.g. "return arguments[0]"), where \args| is the list of
  // arguments to pass to the function. The caller is responsible for the
  // script result |value|.
  ErrorCode ExecuteScript(const std::string& script,
                          const ListValue* const args,
                          Value** value);


  bool NavigateToURL(const std::string& url);
  bool GoForward();
  bool GoBack();
  bool Reload();
  bool GetURL(std::string* url);
  bool GetTabTitle(std::string* tab_title);

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

 private:
  void RunSessionTask(Task* task);
  void RunSessionTaskOnSessionThread(
      Task* task,
      base::WaitableEvent* done_event);
  void InitOnSessionThread(bool* success);
  void TerminateOnSessionThread();

  scoped_ptr<Automation> automation_;
  base::Thread thread_;

  const std::string id_;

  int window_num_;

  int implicit_wait_;
  Speed speed_;

  // The XPath to the frame within this session's active tab which all
  // commands should be directed to. XPath strings can represent a frame deep
  // down the tree (across multiple frame DOMs).
  // Example, /html/body/table/tbody/tr/td/iframe\n/frameset/frame[1]
  // should break into 2 xpaths
  // /html/body/table/tbody/tr/td/iframe & /frameset/frame[1].
  std::string current_frame_xpath_;

  DISALLOW_COPY_AND_ASSIGN(Session);
};

}  // namespace webdriver

DISABLE_RUNNABLE_METHOD_REFCOUNT(webdriver::Session);

#endif  // CHROME_TEST_WEBDRIVER_SESSION_H_
