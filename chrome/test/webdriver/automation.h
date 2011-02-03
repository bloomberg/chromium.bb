// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_WEBDRIVER_AUTOMATION_H_
#define CHROME_TEST_WEBDRIVER_AUTOMATION_H_

#include <string>

#include "base/task.h"
#include "base/ref_counted.h"
#include "base/scoped_temp_dir.h"
#include "chrome/test/automation/browser_proxy.h"
#include "chrome/test/automation/tab_proxy.h"
#include "chrome/test/ui/ui_test.h"

namespace webdriver {

// Creates and controls the Chrome instance.
// This class should be created and accessed on a single thread.
// TODO(phajdan.jr):  Abstract UITestBase classes, see:
// http://code.google.com/p/chromium/issues/detail?id=56865
class Automation : private UITestBase {
 public:
  Automation() {}

  // Creates a browser.
  void Init(bool* success);

  // Terminates this session and disconnects its automation proxy. After
  // invoking this method, the Automation can safely be deleted.
  void Terminate();

  // Executes the given |script| in the specified frame of the current
  // tab. |result| will be set to the JSON result. Returns true on success.
  void ExecuteScript(const std::string& frame_xpath,
                     const std::string& script,
                     std::string* result,
                     bool* success);

  void NavigateToURL(const std::string& url, bool* success);
  void GoForward(bool* success);
  void GoBack(bool* success);
  void Reload(bool* success);
  void GetURL(std::string* url, bool* success);
  void GetTabTitle(std::string* tab_title, bool* success);

 private:
  scoped_refptr<BrowserProxy> browser_;
  scoped_refptr<TabProxy> tab_;

  ScopedTempDir profile_dir_;

  DISALLOW_COPY_AND_ASSIGN(Automation);
};

}  // namespace webdriver

DISABLE_RUNNABLE_METHOD_REFCOUNT(webdriver::Automation);

#endif  // CHROME_TEST_WEBDRIVER_AUTOMATION_H_
