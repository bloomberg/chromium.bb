// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_WEBDRIVER_AUTOMATION_H_
#define CHROME_TEST_WEBDRIVER_AUTOMATION_H_

#include <map>
#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/task.h"
#include "chrome/common/automation_constants.h"
#include "chrome/test/webdriver/error_codes.h"
#include "ui/base/keycodes/keyboard_codes.h"

class AutomationProxy;
class CommandLine;
class DictionaryValue;
class FilePath;
class GURL;
class ListValue;
class ProxyLauncher;
struct WebKeyEvent;

namespace gfx {
class Point;
}

namespace webdriver {

class FramePath;

// Creates and controls the Chrome instance.
// This class should be created and accessed on a single thread.
// Note: All member functions are void because they are invoked
// by posting a task from NewRunnableMethod.
class Automation {
 public:
  Automation();
  virtual ~Automation();

  // Creates a browser, using the specified |browser_exe|.
  void InitWithBrowserPath(const FilePath& browser_exe,
                           const CommandLine& options,
                           ErrorCode* code);

  // Start the system's default Chrome binary.
  void Init(const CommandLine& options,
            ErrorCode* code);

  // Terminates this session and disconnects its automation proxy. After
  // invoking this method, the Automation can safely be deleted.
  void Terminate();

  // Executes the given |script| in the specified frame of the current
  // tab. |result| will be set to the JSON result. Returns true on success.
  void ExecuteScript(int tab_id,
                     const FramePath& frame_path,
                     const std::string& script,
                     std::string* result,
                     bool* success);

  // Sends a webkit key event to the current browser. Waits until the key has
  // been processed by the web page.
  void SendWebKeyEvent(int tab_id,
                       const WebKeyEvent& key_event,
                       bool* success);

  // Sends an OS level key event to the current browser. Waits until the key
  // has been processed by the browser.
  void SendNativeKeyEvent(int tab_id,
                          ui::KeyboardCode key_code,
                          int modifiers,
                          bool* success);

  // Captures a snapshot of the tab to the specified path.  The  PNG will
  // contain the entire page, including what is not in the current view
  // on the  screen.
  void CaptureEntirePageAsPNG(int tab_id, const FilePath& path, bool* success);

  void NavigateToURL(int tab_id, const std::string& url, bool* success);
  void GoForward(int tab_id, bool* success);
  void GoBack(int tab_id, bool* success);
  void Reload(int tab_id, bool* success);

  void GetCookies(const std::string& url, ListValue** cookies, bool* success);
  void GetCookiesDeprecated(
      int tab_id, const GURL& gurl, std::string* cookies, bool* success);
  void DeleteCookie(const std::string& url,
                    const std::string& cookie_name,
                    bool* success);
  void DeleteCookieDeprecated(int tab_id,
                              const GURL& gurl,
                              const std::string& cookie_name,
                              bool* success);
  void SetCookie(
      const std::string& url, DictionaryValue* cookie_dict, bool* success);
  void SetCookieDeprecated(
      int tab_id, const GURL& gurl, const std::string& cookie, bool* success);

  void MouseMove(int tab_id, const gfx::Point& p, bool* success);
  void MouseClick(int tab_id,
                  const gfx::Point& p,
                  automation::MouseButton button,
                  bool* success);
  void MouseDrag(int tab_id,
                 const gfx::Point& start,
                 const gfx::Point& end,
                 bool* success);
  void MouseButtonDown(int tab_id, const gfx::Point& p, bool* success);
  void MouseButtonUp(int tab_id, const gfx::Point& p, bool* success);
  void MouseDoubleClick(int tab_id, const gfx::Point& p, bool* success);

  // Get persistent IDs for all the tabs currently open. These IDs can be used
  // to identify the tab as long as the tab exists.
  void GetTabIds(std::vector<int>* tab_ids, bool* success);

  // Check if the given tab exists currently.
  void DoesTabExist(int tab_id, bool* does_exist, bool* success);

  void CloseTab(int tab_id, bool* success);

  // Gets the active JavaScript modal dialog's message.
  void GetAppModalDialogMessage(std::string* message, bool* success);

  // Accepts or dismisses the active JavaScript modal dialog.
  void AcceptOrDismissAppModalDialog(bool accept, bool* success);

  // Accepts an active prompt JavaScript modal dialog, using the given
  // prompt text as the result of the prompt.
  void AcceptPromptAppModalDialog(const std::string& prompt_text,
                                  bool* success);

  // Gets the version of the runing browser.
  void GetBrowserVersion(std::string* version);

  // Gets the ChromeDriver automation version supported by the automation
  // server.
  void GetChromeDriverAutomationVersion(int* version, bool* success);

  // Waits for all tabs to stop loading.
  void WaitForAllTabsToStopLoading(bool* success);

 private:
  AutomationProxy* automation() const;
  bool GetIndicesForTab(int tab_id, int* browser_index, int* tab_index);

  scoped_ptr<ProxyLauncher> launcher_;

  DISALLOW_COPY_AND_ASSIGN(Automation);
};

}  // namespace webdriver

DISABLE_RUNNABLE_METHOD_REFCOUNT(webdriver::Automation);

#endif  // CHROME_TEST_WEBDRIVER_AUTOMATION_H_
