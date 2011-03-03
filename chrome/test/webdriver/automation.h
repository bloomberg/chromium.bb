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
#include "base/scoped_ptr.h"
#include "chrome/common/automation_constants.h"
#include "ui/base/keycodes/keyboard_codes.h"

class AutomationProxy;
class DictionaryValue;
class FilePath;
class GURL;
class ProxyLauncher;
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
              int modifiers);

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
class Automation {
 public:
  Automation();
  virtual ~Automation();

  // Creates a browser, using the exe found in |browser_dir|. If |browser_dir|
  // is empty, it will search in all the default locations.
  void Init(const FilePath& browser_dir, bool* success);

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

  // Waits for all tabs to stop loading.
  void WaitForAllTabsToStopLoading(bool* success);

 private:
  typedef std::map<int, scoped_refptr<TabProxy> > TabIdMap;

  TabProxy* GetTabById(int tab_id);
  AutomationProxy* automation() const;

  scoped_ptr<ProxyLauncher> launcher_;
  // Map from tab ID to |TabProxy|. The tab ID is simply the |AutomationHandle|
  // for the proxy.
  TabIdMap tab_id_map_;

  bool SendJSONRequest(
      int tab_id, const DictionaryValue& dict, std::string* reply);

  bool GetIndicesForTab(int tab_id, int* browser_index, int* tab_index);

  DISALLOW_COPY_AND_ASSIGN(Automation);
};

}  // namespace webdriver

DISABLE_RUNNABLE_METHOD_REFCOUNT(webdriver::Automation);

#endif  // CHROME_TEST_WEBDRIVER_AUTOMATION_H_
