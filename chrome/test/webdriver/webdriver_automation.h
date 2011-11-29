// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_WEBDRIVER_WEBDRIVER_AUTOMATION_H_
#define CHROME_TEST_WEBDRIVER_WEBDRIVER_AUTOMATION_H_

#include <map>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/task.h"
#include "chrome/common/automation_constants.h"
#include "ui/base/keycodes/keyboard_codes.h"

class AutomationProxy;
class ProxyLauncher;
struct WebKeyEvent;

namespace base {
class DictionaryValue;
class ListValue;
}

namespace webdriver {

class Error;
class FramePath;
class Point;

// Creates and controls the Chrome instance.
// This class should be created and accessed on a single thread.
// Note: All member functions are void because they are invoked
// by posting a task from NewRunnableMethod.
class Automation {
 public:
  struct BrowserOptions {
    BrowserOptions();
    ~BrowserOptions();

    // The command line to use for launching the browser. If no program is
    // specified, the default browser executable will be used.
    CommandLine command;

    // The user data directory to be copied and used. If empty, a temporary
    // directory will be used.
    FilePath user_data_dir;

    // The channel ID of an already running browser to connect to. If empty,
    // the browser will be launched with an anonymous channel.
    std::string channel_id;

    // True if the Chrome process should only be terminated if quit is called.
    // If false, Chrome will also be terminated if this process is killed or
    // shutdown.
    bool detach_process;
  };

  Automation();
  virtual ~Automation();

  // Start the system's default Chrome binary.
  void Init(const BrowserOptions& options, Error** error);

  // Terminates this session and disconnects its automation proxy. After
  // invoking this method, the Automation can safely be deleted.
  void Terminate();

  // Executes the given |script| in the specified frame of the current
  // tab. |result| will be set to the JSON result. Returns true on success.
  void ExecuteScript(int tab_id,
                     const FramePath& frame_path,
                     const std::string& script,
                     std::string* result,
                     Error** error);

  // Sends a webkit key event to the current browser. Waits until the key has
  // been processed by the web page.
  void SendWebKeyEvent(int tab_id,
                       const WebKeyEvent& key_event,
                       Error** error);

  // Sends an OS level key event to the current browser. Waits until the key
  // has been processed by the browser.
  void SendNativeKeyEvent(int tab_id,
                          ui::KeyboardCode key_code,
                          int modifiers,
                          Error** error);

  // Drag and drop the file paths to the given location.
  void DragAndDropFilePaths(int tab_id,
                            const Point& location,
                            const std::vector<FilePath::StringType>& paths,
                            Error** error);

  // Captures a snapshot of the tab to the specified path.  The  PNG will
  // contain the entire page, including what is not in the current view
  // on the  screen.
  void CaptureEntirePageAsPNG(int tab_id, const FilePath& path, Error** error);

  void NavigateToURL(int tab_id, const std::string& url, Error** error);
  void NavigateToURLAsync(int tab_id, const std::string& url, Error** error);
  void GoForward(int tab_id, Error** error);
  void GoBack(int tab_id, Error** error);
  void Reload(int tab_id, Error** error);

  void GetCookies(const std::string& url,
                  base::ListValue** cookies,
                  Error** error);
  void DeleteCookie(const std::string& url,
                    const std::string& cookie_name,
                    Error** error);
  void SetCookie(const std::string& url,
                 base::DictionaryValue* cookie_dict,
                 Error** error);

  void MouseMove(int tab_id, const Point& p, Error** error);
  void MouseClick(int tab_id,
                  const Point& p,
                  automation::MouseButton button,
                  Error** error);
  void MouseDrag(int tab_id,
                 const Point& start,
                 const Point& end,
                 Error** error);
  void MouseButtonDown(int tab_id, const Point& p, Error** error);
  void MouseButtonUp(int tab_id, const Point& p, Error** error);
  void MouseDoubleClick(int tab_id, const Point& p, Error** error);

  // Get persistent IDs for all the tabs currently open. These IDs can be used
  // to identify the tab as long as the tab exists.
  void GetTabIds(std::vector<int>* tab_ids, Error** error);

  // Check if the given tab exists currently.
  void DoesTabExist(int tab_id, bool* does_exist, Error** error);

  void CloseTab(int tab_id, Error** error);

  // Gets the active JavaScript modal dialog's message.
  void GetAppModalDialogMessage(std::string* message, Error** error);

  // Accepts or dismisses the active JavaScript modal dialog.
  void AcceptOrDismissAppModalDialog(bool accept, Error** error);

  // Accepts an active prompt JavaScript modal dialog, using the given
  // prompt text as the result of the prompt.
  void AcceptPromptAppModalDialog(const std::string& prompt_text,
                                  Error** error);

  // Gets the version of the runing browser.
  void GetBrowserVersion(std::string* version);

  // Gets the ChromeDriver automation version supported by the automation
  // server.
  void GetChromeDriverAutomationVersion(int* version, Error** error);

  // Waits for all tabs to stop loading.
  void WaitForAllTabsToStopLoading(Error** error);

  // Install packed extension.
  void InstallExtensionDeprecated(const FilePath& path, Error** error);

  // Gets all installed extension IDs.
  void GetInstalledExtensions(std::vector<std::string>* extension_ids,
                              Error** error);

  // Install a packed or unpacked extension. If the path ends with '.crx',
  // the extension is assumed to be packed.
  void InstallExtension(const FilePath& path, std::string* extension_id,
                        Error** error);

 private:
  AutomationProxy* automation() const;
  Error* GetIndicesForTab(int tab_id, int* browser_index, int* tab_index);
  Error* CompareVersion(int client_build_no,
                        int client_patch_no,
                        bool* is_newer_or_equal);
  Error* CheckVersion(int client_build_no,
                      int client_patch_no,
                      const std::string& error_msg);
  Error* CheckAlertsSupported();
  Error* CheckAdvancedInteractionsSupported();
  Error* CheckNewExtensionInterfaceSupported();

  scoped_ptr<ProxyLauncher> launcher_;

  DISALLOW_COPY_AND_ASSIGN(Automation);
};

}  // namespace webdriver

DISABLE_RUNNABLE_METHOD_REFCOUNT(webdriver::Automation);

#endif  // CHROME_TEST_WEBDRIVER_WEBDRIVER_AUTOMATION_H_
