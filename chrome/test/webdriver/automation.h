// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_WEBDRIVER_AUTOMATION_H_
#define CHROME_TEST_WEBDRIVER_AUTOMATION_H_

#include <map>
#include <string>
#include <vector>

#include "base/task.h"
#include "base/ref_counted.h"
#include "base/scoped_temp_dir.h"
#include "chrome/common/automation_constants.h"
#include "chrome/test/ui/ui_test.h"
#include "ui/base/keycodes/keyboard_codes.h"

class GURL;
class TabProxy;

namespace gfx {
class Point;
}

namespace webdriver {

struct WebKeyEvent {
  WebKeyEvent(automation::KeyEventTypes type,
              ui::KeyboardCode key_code,
              const std::string& unmodified_text,
              const std::string& modified_text,
              int modifiers)
      : type(type),
        key_code(key_code),
        unmodified_text(unmodified_text),
        modified_text(modified_text),
        modifiers(modifiers) {}

  automation::KeyEventTypes type;
  ui::KeyboardCode key_code;
  std::string unmodified_text;
  std::string modified_text;
  int modifiers;
};

// Creates and controls the Chrome instance.
// This class should be created and accessed on a single thread.
// Note: All member functions are void because they are invoked
// by posting a task from NewRunnableMethod.
// TODO(phajdan.jr):  Abstract UITestBase classes, see:
// http://code.google.com/p/chromium/issues/detail?id=56865
class Automation : private UITestBase {
 public:
  Automation();
  virtual ~Automation();

  // Creates a browser.
  void Init(bool* success);

  // Terminates this session and disconnects its automation proxy. After
  // invoking this method, the Automation can safely be deleted.
  void Terminate();

  // Executes the given |script| in the specified frame of the current
  // tab. |result| will be set to the JSON result. Returns true on success.
  void ExecuteScript(int tab_id,
                     const std::string& frame_xpath,
                     const std::string& script,
                     std::string* result,
                     bool* success);

  // Sends a key event to the current browser. Waits until the key has
  // been processed by the web page.
  void SendWebKeyEvent(int tab_id, const WebKeyEvent& key_event, bool* success);

  void NavigateToURL(int tab_id, const std::string& url, bool* success);
  void GoForward(int tab_id, bool* success);
  void GoBack(int tab_id, bool* success);
  void Reload(int tab_id, bool* success);
  void GetURL(int tab_id, std::string* url, bool* success);
  void GetGURL(int tab_id, GURL* gurl, bool* success);
  void GetTabTitle(int tab_id, std::string* tab_title, bool* success);
  void GetCookies(
      int tab_id, const GURL& gurl, std::string* cookies, bool* success);
  void GetCookieByName(int tab_id,
                       const GURL& gurl,
                       const std::string& cookie_name,
                       std::string* cookie,
                       bool* success);
  void DeleteCookie(int tab_id,
                    const GURL& gurl,
                    const std::string& cookie_name,
                    bool* success);
  void SetCookie(
      int tab_id, const GURL& gurl, const std::string& cookie, bool* success);
  void MouseMove(int tab_id, const gfx::Point& p, bool* success);
  void MouseClick(int tab_id, const gfx::Point& p, int flag, bool* success);
  void MouseDrag(int tab_id,
                 const gfx::Point& start,
                 const gfx::Point& end,
                 bool* success);

  // Get persistent IDs for all the tabs currently open. These IDs can be used
  // to identify the tab as long as the tab exists.
  void GetTabIds(std::vector<int>* tab_ids, bool* success);

  // Check if the given tab exists currently.
  void DoesTabExist(int tab_id, bool* does_exist);

  void CloseTab(int tab_id, bool* success);

  // Gets the version of the runing browser.
  void GetVersion(std::string* version);

 private:
  typedef std::map<int, scoped_refptr<TabProxy> > TabIdMap;
  TabProxy* GetTabById(int tab_id);

  // Map from tab ID to |TabProxy|. The tab ID is simply the |AutomationHandle|
  // for the proxy.
  TabIdMap tab_id_map_;
  ScopedTempDir profile_dir_;

  bool SendJSONRequest(
      int tab_id, const DictionaryValue& dict, std::string* reply);

  DISALLOW_COPY_AND_ASSIGN(Automation);
};

}  // namespace webdriver

DISABLE_RUNNABLE_METHOD_REFCOUNT(webdriver::Automation);

#endif  // CHROME_TEST_WEBDRIVER_AUTOMATION_H_
