// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_WEBDRIVER_WEBDRIVER_AUTOMATION_H_
#define CHROME_TEST_WEBDRIVER_WEBDRIVER_AUTOMATION_H_

#include <map>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/common/automation_constants.h"
#include "chrome/test/webdriver/webdriver_logging.h"
#include "ui/base/keycodes/keyboard_codes.h"

class AutomationId;
class AutomationProxy;
class ProxyLauncher;
struct WebKeyEvent;
struct WebMouseEvent;
class WebViewId;
struct WebViewInfo;
class WebViewLocator;

namespace base {
class DictionaryValue;
class ListValue;
class Value;
}

namespace webdriver {

class Error;
class FramePath;
class Point;
class Rect;

// Creates and controls the Chrome instance.
// This class should be created and accessed on a single thread.
// Note: All member functions are void because they are invoked
// by posting a task.
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
    base::FilePath user_data_dir;

    // The channel ID of an already running browser to connect to. If empty,
    // the browser will be launched with an anonymous channel.
    std::string channel_id;

    // True if the Chrome process should only be terminated if quit is called.
    // If false, Chrome will also be terminated if this process is killed or
    // shutdown.
    bool detach_process;

    // True if the browser should ignore certificate related errors.
    bool ignore_certificate_errors;

    // Set of switches to be excluded from default list when starting Chrome.
    std::set<std::string> exclude_switches;
  };

  explicit Automation(const Logger& logger);
  virtual ~Automation();

  // Start the system's default Chrome binary.
  void Init(const BrowserOptions& options, int* build_no, Error** error);

  // Terminates this session and disconnects its automation proxy. After
  // invoking this method, the Automation can safely be deleted.
  void Terminate();

  // Executes the given |script| in the specified frame of the current
  // tab. |result| will be set to the JSON result. Returns true on success.
  void ExecuteScript(const WebViewId& view_id,
                     const FramePath& frame_path,
                     const std::string& script,
                     std::string* result,
                     Error** error);

  // Sends a webkit key event to the current browser. Waits until the key has
  // been processed by the web page.
  void SendWebKeyEvent(const WebViewId& view_id,
                       const WebKeyEvent& key_event,
                       Error** error);

  // Sends a web mouse event to the given view. Waits until the event has
  // been processed by the view.
  void SendWebMouseEvent(const WebViewId& view_id,
                         const WebMouseEvent& event,
                         Error** error);

  // Drag and drop the file paths to the given location.
  void DragAndDropFilePaths(
      const WebViewId& view_id,
      const Point& location,
      const std::vector<base::FilePath::StringType>& paths,
      Error** error);

  // Captures a snapshot of the tab to the specified path.  The  PNG will
  // contain the entire page, including what is not in the current view
  // on the  screen.
  void CaptureEntirePageAsPNG(
      const WebViewId& view_id, const base::FilePath& path, Error** error);

#if !defined(NO_TCMALLOC) && (defined(OS_LINUX) || defined(OS_CHROMEOS))
  // Dumps a heap profile of the process of the tab.
  void HeapProfilerDump(
      const WebViewId& view_id, const std::string& reason, Error** error);
#endif  // !defined(NO_TCMALLOC) && (defined(OS_LINUX) || defined(OS_CHROMEOS))

  void NavigateToURL(
      const WebViewId& view_id, const std::string& url, Error** error);
  void NavigateToURLAsync(
      const WebViewId& view_id, const std::string& url, Error** error);
  void GoForward(const WebViewId& view_id, Error** error);
  void GoBack(const WebViewId& view_id, Error** error);
  void Reload(const WebViewId& view_id, Error** error);

  void GetCookies(const std::string& url,
                  base::ListValue** cookies,
                  Error** error);
  void DeleteCookie(const std::string& url,
                    const std::string& cookie_name,
                    Error** error);
  void SetCookie(const std::string& url,
                 base::DictionaryValue* cookie_dict,
                 Error** error);

  // TODO(kkania): All of these mouse commands are deprecated and should be
  // removed when chrome build 1002 is no longer supported.
  // Use SendWebMouseEvent instead.
  void MouseMoveDeprecated(const WebViewId& view_id,
                           const Point& p,
                           Error** error);
  void MouseClickDeprecated(const WebViewId& view_id,
                            const Point& p,
                            automation::MouseButton button,
                            Error** error);
  void MouseDragDeprecated(const WebViewId& view_id,
                           const Point& start,
                           const Point& end,
                           Error** error);
  void MouseButtonDownDeprecated(const WebViewId& view_id,
                                 const Point& p,
                                 Error** error);
  void MouseButtonUpDeprecated(const WebViewId& view_id,
                               const Point& p,
                               Error** error);
  void MouseDoubleClickDeprecated(const WebViewId& view_id,
                                  const Point& p,
                                  Error** error);

  // Get info for all views currently open.
  void GetViews(std::vector<WebViewInfo>* views, Error** error);

  // Check if the given view exists currently.
  void DoesViewExist(const WebViewId& view_id, bool* does_exist, Error** error);

  // Closes the given view.
  void CloseView(const WebViewId& view_id, Error** error);

  // Sets the bounds for the given view. The position should be in screen
  // coordinates, while the size should be the desired size of the view.
  void SetViewBounds(const WebViewId& view_id,
                     const Rect& bounds,
                     Error** error);

  // Maximizes the given view.
  void MaximizeView(const WebViewId& view_id, Error** error);

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

  // Waits for all views to stop loading.
  void WaitForAllViewsToStopLoading(Error** error);

  // Install a packed or unpacked extension. If the path ends with '.crx',
  // the extension is assumed to be packed.
  void InstallExtension(const base::FilePath& path, std::string* extension_id,
                        Error** error);

  // Gets a list of dictionary information about all installed extensions.
  void GetExtensionsInfo(base::ListValue* extensions_list, Error** error);

  // Gets a list of dictionary information about all installed extensions.
  void IsPageActionVisible(const WebViewId& tab_id,
                           const std::string& extension_id,
                           bool* is_visible,
                           Error** error);

  // Sets whether the extension is enabled or not.
  void SetExtensionState(const std::string& extension_id,
                         bool enable,
                         Error** error);

  // Clicks the extension action button. If |browser_action| is false, the
  // page action will be clicked.
  void ClickExtensionButton(const std::string& extension_id,
                            bool browser_action,
                            Error** error);

  // Uninstalls the given extension.
  void UninstallExtension(const std::string& extension_id, Error** error);

  // Set a local state preference, which is not associated with any profile.
  // Ownership of |value| is taken by this function.
  void SetLocalStatePreference(const std::string& pref,
                               base::Value* value,
                               Error** error);

  // Set a user preference, which is associated with the current profile.
  // Ownership of |value| is taken by this fucntion.
  void SetPreference(const std::string& pref,
                     base::Value* value,
                     Error** error);

  // Gets the current geolocation.
  void GetGeolocation(scoped_ptr<base::DictionaryValue>* geolocation,
                      Error** error);

  // Overrides the current geolocation.
  void OverrideGeolocation(const base::DictionaryValue* geolocation,
                           Error** error);

 private:
  AutomationProxy* automation() const;
  Error* ConvertViewIdToLocator(const WebViewId& view_id,
                                WebViewLocator* view_locator);
  Error* DetermineBuildNumber();
  Error* CheckVersion(int min_required_build_no,
                      const std::string& error_msg);
  Error* CheckAlertsSupported();
  Error* CheckAdvancedInteractionsSupported();
  Error* CheckNewExtensionInterfaceSupported();
  Error* CheckGeolocationSupported();
  Error* CheckMaximizeSupported();
  Error* IsNewMouseApiSupported(bool* supports_new_api);

  const Logger& logger_;
  scoped_ptr<ProxyLauncher> launcher_;
  int build_no_;
  scoped_ptr<base::DictionaryValue> geolocation_;

  DISALLOW_COPY_AND_ASSIGN(Automation);
};

}  // namespace webdriver

#endif  // CHROME_TEST_WEBDRIVER_WEBDRIVER_AUTOMATION_H_
