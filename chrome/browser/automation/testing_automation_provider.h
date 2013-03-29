// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOMATION_TESTING_AUTOMATION_PROVIDER_H_
#define CHROME_BROWSER_AUTOMATION_TESTING_AUTOMATION_PROVIDER_H_

#include <map>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/automation/automation_event_observers.h"
#include "chrome/browser/automation/automation_event_queue.h"
#include "chrome/browser/automation/automation_provider.h"
#include "chrome/browser/automation/automation_provider_json.h"
#include "chrome/browser/history/history_service.h"
#include "chrome/browser/importer/importer_list_observer.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/common/page_type.h"
#include "content/public/common/security_style.h"
#include "net/cert/cert_status_flags.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebInputEvent.h"

#if defined(OS_CHROMEOS)
// TODO(sque): move to a ChromeOS-specific class. See crosbug.com/22081.
class PowerManagerClientObserverForTesting;
#endif  // defined(OS_CHROMEOS)

class CreditCard;
class ImporterList;

namespace base {
class DictionaryValue;
}

namespace content {
class RenderViewHost;
struct NativeWebKeyboardEvent;
}

namespace gfx {
class Rect;
}

namespace webkit {
struct WebPluginInfo;
}

// This is an automation provider containing testing calls.
class TestingAutomationProvider : public AutomationProvider,
                                  public chrome::BrowserListObserver,
                                  public importer::ImporterListObserver,
                                  public content::NotificationObserver {
 public:
  explicit TestingAutomationProvider(Profile* profile);

  virtual IPC::Channel::Mode GetChannelMode(bool use_named_interface);

  // IPC::Listener:
  virtual bool OnMessageReceived(const IPC::Message& msg) OVERRIDE;
  virtual void OnChannelError() OVERRIDE;

 private:
  // Storage for ImportSettings() to resume operations after a callback.
  struct ImportSettingsData {
    string16 browser_name;
    int import_items;
    bool first_run;
    Browser* browser;
    IPC::Message* reply_message;
  };

  virtual ~TestingAutomationProvider();

  // chrome::BrowserListObserver:
  virtual void OnBrowserAdded(Browser* browser) OVERRIDE;
  virtual void OnBrowserRemoved(Browser* browser) OVERRIDE;

  // importer::ImporterListObserver:
  virtual void OnSourceProfilesLoaded() OVERRIDE;

  // content::NotificationObserver:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // IPC Message callbacks.
  void CloseBrowser(int handle, IPC::Message* reply_message);
  void ActivateTab(int handle, int at_index, int* status);
  void AppendTab(int handle, const GURL& url, IPC::Message* reply_message);
  void GetMachPortCount(int* port_count);
  void GetActiveTabIndex(int handle, int* active_tab_index);
  void CloseTab(int tab_handle, bool wait_until_closed,
                IPC::Message* reply_message);
  void GetCookies(const GURL& url, int handle, int* value_size,
                  std::string* value);
  void NavigateToURLBlockUntilNavigationsComplete(int handle, const GURL& url,
                                                  int number_of_navigations,
                                                  IPC::Message* reply_message);
  void NavigationAsync(int handle, const GURL& url, bool* status);
  void Reload(int handle, IPC::Message* reply_message);
  void GetRedirectsFrom(int tab_handle,
                        const GURL& source_url,
                        IPC::Message* reply_message);
  void GetBrowserWindowCount(int* window_count);
  void GetNormalBrowserWindowCount(int* window_count);
  // Be aware that the browser window returned might be of non TYPE_TABBED
  // or in incognito mode.
  void GetBrowserWindow(int index, int* handle);
  void ExecuteBrowserCommandAsync(int handle, int command, bool* success);
  void ExecuteBrowserCommand(int handle, int command,
                             IPC::Message* reply_message);
  void TerminateSession(int handle, bool* success);
  void WindowGetViewBounds(int handle, int view_id, bool screen_coordinates,
                           bool* success, gfx::Rect* bounds);
  void SetWindowBounds(int handle, const gfx::Rect& bounds, bool* result);
  void SetWindowVisible(int handle, bool visible, bool* result);
  void GetTabCount(int handle, int* tab_count);
  void GetType(int handle, int* type_as_int);
  void GetTab(int win_handle, int tab_index, int* tab_handle);
  void GetTabTitle(int handle, int* title_string_size, std::wstring* title);
  void GetTabIndex(int handle, int* tabstrip_index);
  void GetTabURL(int handle, bool* success, GURL* url);
  void ExecuteJavascript(int handle,
                         const std::wstring& frame_xpath,
                         const std::wstring& script,
                         IPC::Message* reply_message);

  // If |show| is true, call Show() on the new window after creating it.
  void OpenNewBrowserWindowOfType(int type,
                                  bool show,
                                  IPC::Message* reply_message);

  // Retrieves a Browser from a Window and vice-versa.
  void GetWindowForBrowser(int window_handle, bool* success, int* handle);

  // Gets the duration in ms of the last event matching |event_name|.
  // |duration_ms| is -1 if the event hasn't occurred yet.
  void GetMetricEventDuration(const std::string& event_name, int* duration_ms);

  // Brings the browser window to the front and activates it.
  void BringBrowserToFront(int browser_handle, bool* success);

  // Responds to requests to open the FindInPage window.
  void HandleOpenFindInPageRequest(const IPC::Message& message,
                                   int handle);

  // Get the visibility state of the Find window.
  void GetFindWindowVisibility(int handle, bool* visible);

  // Wait for the bookmark model to load.
  void WaitForBookmarkModelToLoad(int handle, IPC::Message* reply_message);

  // Set |loaded| to true if the bookmark model has loaded, else false.
  void BookmarkModelHasLoaded(int handle, bool* loaded);

  // Get the visibility state of the Bookmark bar.
  // Returns a status dictionary over the JSON interface.
  void GetBookmarkBarStatus(base::DictionaryValue* args,
                            IPC::Message* reply_message);

  // Get the bookmarks as a JSON string.
  void GetBookmarksAsJSON(base::DictionaryValue* args,
                          IPC::Message* reply_message);

  // Editing, modification, and removal of bookmarks through the JSON interface.
  // Bookmarks are referenced by id.
  void WaitForBookmarkModelToLoadJSON(base::DictionaryValue* args,
                                      IPC::Message* reply_message);
  void AddBookmark(base::DictionaryValue* args,
                   IPC::Message* reply_message);
  void ReparentBookmark(base::DictionaryValue* args,
                        IPC::Message* reply_message);
  void SetBookmarkTitle(base::DictionaryValue* args,
                        IPC::Message* reply_message);
  void SetBookmarkURL(base::DictionaryValue* args,
                      IPC::Message* reply_message);
  void RemoveBookmark(base::DictionaryValue* args,
                      IPC::Message* reply_message);

  void WaitForBrowserWindowCountToBecome(int target_count,
                                         IPC::Message* reply_message);

  void GoBackBlockUntilNavigationsComplete(int handle,
                                           int number_of_navigations,
                                           IPC::Message* reply_message);

  void GoForwardBlockUntilNavigationsComplete(int handle,
                                              int number_of_navigations,
                                              IPC::Message* reply_message);

  // Generic pattern for pyautolib
  // Uses the JSON interface for input/output.
  void SendJSONRequestWithBrowserHandle(int handle,
                                        const std::string& json_request,
                                        IPC::Message* reply_message);
  void SendJSONRequestWithBrowserIndex(int index,
                                       const std::string& json_request,
                                       IPC::Message* reply_message);
  void SendJSONRequest(Browser* browser,
                       const std::string& json_request,
                       IPC::Message* reply_message);

  // Method ptr for json handlers.
  // Uses the JSON interface for input/output.
  typedef void (TestingAutomationProvider::*JsonHandler)(base::DictionaryValue*,
                                                         IPC::Message*);

  // Method ptr for json handlers that take a browser argument.
  // Uses the JSON interface for input/output.
  typedef void (TestingAutomationProvider::*BrowserJsonHandler)(
      Browser* browser,
      base::DictionaryValue*,
      IPC::Message*);

  // JSON interface helper functions.
  static scoped_ptr<DictionaryValue> ParseJSONRequestCommand(
      const std::string& json_request,
      std::string* command,
      std::string* error);
  void BuildJSONHandlerMaps();

  // Set window dimensions.
  // Uses the JSON interface for input/output.
  void SetWindowDimensions(Browser* browser,
                           base::DictionaryValue* args,
                           IPC::Message* reply_message);

  // Get info about infobars in the given WebContents object.
  // This includes info about the type of infobars, the message text,
  // buttons, etc.
  // Caller owns the returned object.
  ListValue* GetInfobarsInfo(content::WebContents* tc);

  // Perform actions on an infobar like dismiss, accept, cancel.
  // This method can handle dismiss for all infobars. It can also handle
  // accept / cancel (where it will assume the infobar is a confirm infobar) and
  // allow / deny (where it will assume the infobar is a media stream infobar).
  // For the media stream infobar, passing 'allow' will just select the first
  // video and audio device available to the bar, or report an error if there
  // are no devices available.
  //
  // Uses the JSON interface for input/output.
  void PerformActionOnInfobar(Browser* browser,
                              base::DictionaryValue* args,
                              IPC::Message* reply_message);

  // Create a new profile and open a new browser window with this profile. Uses
  // the JSON interface for input/output.
  void OpenNewBrowserWindowWithNewProfile(
      base::DictionaryValue* args,
      IPC::Message* reply_message);

  // Open a new browser window.
  // Uses the JSON interface for input/output.
  void OpenNewBrowserWindow(base::DictionaryValue* args,
                            IPC::Message* reply_message);
  // Close a browser window.
  // Uses the JSON interface for input/output.
  void CloseBrowserWindow(base::DictionaryValue* args,
                          IPC::Message* reply_message);

  // Get info about multi-profile users.
  // Uses the JSON interface for input/output.
  void GetMultiProfileInfo(
      base::DictionaryValue* args,
      IPC::Message* reply_message);
  // Open a new browser window for an existing profile.
  // Uses the JSON interface for input/output.
  void OpenProfileWindow(
      base::DictionaryValue* args, IPC::Message* reply_message);

  // Get info about the chromium/chrome in use.
  // This includes things like version, executable name, executable path.
  // Uses the JSON interface for input/output.
  void GetBrowserInfo(base::DictionaryValue* args,
                      IPC::Message* reply_message);

  // Get the browser window count. Uses the JSON interface.
  void GetBrowserWindowCountJSON(base::DictionaryValue* args,
                                 IPC::Message* reply_message);

  // Get info about browser-related processes that currently exist.
  void GetProcessInfo(base::DictionaryValue* args,
                      IPC::Message* reply_message);

  // Get info about the state of navigation in a given tab.
  // This includes ssl info.
  // Uses the JSON interface for input/output.
  void GetNavigationInfo(Browser* browser,
                         base::DictionaryValue* args,
                         IPC::Message* reply_message);

  // Get info about downloads. This includes only ones that have been
  // registered by the history system.
  // Uses the JSON interface for input/output.
  void GetDownloadsInfo(Browser* browser,
                        base::DictionaryValue* args,
                        IPC::Message* reply_message);

  // Wait for all downloads to complete.
  // Uses the JSON interface for input/output.
  void WaitForAllDownloadsToComplete(Browser* browser,
                                     base::DictionaryValue* args,
                                     IPC::Message* reply_message);

  // Performs the given action on the specified download.
  // Uses the JSON interface for input/output.
  void PerformActionOnDownload(Browser* browser,
                               base::DictionaryValue* args,
                               IPC::Message* reply_message);

  // Get info about history.
  // Uses the JSON interface for input/output.
  void GetHistoryInfo(Browser* browser,
                      base::DictionaryValue* args,
                      IPC::Message* reply_message);

  // Invoke loading of template url model.
  // Uses the JSON interface for input/output.
  void LoadSearchEngineInfo(Browser* browser,
                            base::DictionaryValue* args,
                            IPC::Message* reply_message);

  // Sets the visibility of the download shelf. Uses the JSON interface.
  // Example:
  //   input: { "is_visible": true,
  //            "windex": 1,
  //          }
  //   output: none
  void SetDownloadShelfVisibleJSON(base::DictionaryValue* args,
                                   IPC::Message* reply_message);

  // Gets the visibility of the download shelf. Uses the JSON interface.
  // Example:
  //   input: { "windex": 1 }
  //   output: { "is_visible": true }
  void IsDownloadShelfVisibleJSON(base::DictionaryValue* args,
                                  IPC::Message* reply_message);

  // Gets the download path of the given tab. Uses the JSON interface.
  // Example:
  //   input: { "tab_index": 1,
  //            "windex": 1,
  //          }
  //   output: { "path": "/home/foobar/Downloads" }
  void GetDownloadDirectoryJSON(base::DictionaryValue* args,
                                IPC::Message* reply_message);

  // Get search engines list.
  // Assumes that the profile's template url model is loaded.
  // Uses the JSON interface for input/output.
  void GetSearchEngineInfo(Browser* browser,
                           base::DictionaryValue* args,
                           IPC::Message* reply_message);

  // Add or edit search engine.
  // Assumes that the profile's template url model is loaded.
  // Uses the JSON interface for input/output.
  void AddOrEditSearchEngine(Browser* browser,
                             base::DictionaryValue* args,
                             IPC::Message* reply_message);

  // Perform a given action on an existing search engine.
  // Assumes that the profile's template url model is loaded.
  // Uses the JSON interface for input/output.
  void PerformActionOnSearchEngine(Browser* browser,
                                   base::DictionaryValue* args,
                                   IPC::Message* reply_message);

  // Get info about preferences stored in Local State.
  // Uses the JSON interface for input/output.
  void GetLocalStatePrefsInfo(base::DictionaryValue* args,
                              IPC::Message* reply_message);

  // Set local state prefs.
  // Uses the JSON interface for input/output.
  void SetLocalStatePrefs(base::DictionaryValue* args,
                          IPC::Message* reply_message);

  // Get info about preferences.
  // Uses the JSON interface for input/output.
  void GetPrefsInfo(base::DictionaryValue* args,
                    IPC::Message* reply_message);

  // Set prefs.
  // Uses the JSON interface for input/output.
  void SetPrefs(base::DictionaryValue* args,
                IPC::Message* reply_message);

  // Return load times of initial tabs.
  // Uses the JSON interface for input/output.
  // Only includes tabs from command line arguments or session restore.
  // See declaration of InitialLoadObserver in automation_provider_observers.h
  // for example response.
  void GetInitialLoadTimes(Browser* browser,
                           base::DictionaryValue* args,
                           IPC::Message* reply_message);

  // Get info about plugins.
  // Uses the JSON interface for input/output.
  void GetPluginsInfo(Browser* browser,
                      base::DictionaryValue* args,
                      IPC::Message* reply_message);
  void GetPluginsInfoCallback(Browser* browser,
      base::DictionaryValue* args,
      IPC::Message* reply_message,
      const std::vector<webkit::WebPluginInfo>& plugins);

  // Enable a plugin.
  // Uses the JSON interface for input/output.
  void EnablePlugin(Browser* browser,
                    base::DictionaryValue* args,
                    IPC::Message* reply_message);

  // Disable a plugin.
  // Uses the JSON interface for input/output.
  void DisablePlugin(Browser* browser,
                     base::DictionaryValue* args,
                     IPC::Message* reply_message);

  // Get info about omnibox.
  // Contains data about the matches (url, content, description)
  // in the omnibox popup, the text in the omnibox.
  // Uses the JSON interface for input/output.
  void GetOmniboxInfo(Browser* browser,
                      base::DictionaryValue* args,
                      IPC::Message* reply_message);

  // Set text in the omnibox. This sets focus to the omnibox.
  // Uses the JSON interface for input/output.
  void SetOmniboxText(Browser* browser,
                      base::DictionaryValue* args,
                      IPC::Message* reply_message);

  // Move omnibox popup selection up or down.
  // Uses the JSON interface for input/output.
  void OmniboxMovePopupSelection(Browser* browser,
                                 base::DictionaryValue* args,
                                 IPC::Message* reply_message);

  // Accept the current string of text in the omnibox.
  // This is equivalent to clicking or hiting enter on a popup selection.
  // Blocks until the page loads.
  // Uses the JSON interface for input/output.
  void OmniboxAcceptInput(Browser* browser,
                          base::DictionaryValue* args,
                          IPC::Message* reply_message);

  // Save the contents of a tab into a file.
  // Uses the JSON interface for input/output.
  void SaveTabContents(Browser* browser,
                       base::DictionaryValue* args,
                       IPC::Message* reply_message);

  // Import the given settings from the given browser.
  // Uses the JSON interface for input/output.
  void ImportSettings(Browser* browser,
                      base::DictionaryValue* args,
                      IPC::Message* reply_message);

  // Add a new entry to the password store based on the password information
  // provided. This method can also be used to add a blacklisted site (which
  // will never fill in the password).
  // Uses the JSON interface for input/output.
  void AddSavedPassword(Browser* browser,
                        base::DictionaryValue* args,
                        IPC::Message* reply_message);

  // Removes the password matching the information provided. This method can
  // also be used to remove a blacklisted site.
  // Uses the JSON interface for input/output.
  void RemoveSavedPassword(Browser* browser,
                           base::DictionaryValue* args,
                           IPC::Message* reply_message);

  // Return the saved username/password combinations.
  // Uses the JSON interface for input/output.
  void GetSavedPasswords(Browser* browser,
                         base::DictionaryValue* args,
                         IPC::Message* reply_message);

  // Install the given unpacked/packed extension.
  // Uses the JSON interface for input/output.
  void InstallExtension(base::DictionaryValue* args,
                        IPC::Message* reply_message);

  // Get info about all intalled extensions.
  // Uses the JSON interface for input/output.
  void GetExtensionsInfo(base::DictionaryValue* args,
                         IPC::Message* reply_message);

  // Uninstalls the extension with the given id.
  // Uses the JSON interface for input/output.
  void UninstallExtensionById(base::DictionaryValue* args,
                              IPC::Message* reply_message);

  // Set extension states:
  //   Enable/disable extension.
  //   Allow/disallow extension in incognito mode.
  // Uses the JSON interface for input/output.
  void SetExtensionStateById(base::DictionaryValue* args,
                             IPC::Message* reply_message);

  // Trigger page action asynchronously in the active tab.
  // Uses the JSON interface for input/output.
  void TriggerPageActionById(base::DictionaryValue* args,
                             IPC::Message* reply_message);

  // Trigger browser action asynchronously in the active tab.
  // Uses the JSON interface for input/output.
  void TriggerBrowserActionById(base::DictionaryValue* args,
                                IPC::Message* reply_message);

  // Auto-updates installed extensions.
  // Uses the JSON interface for input/output.
  void UpdateExtensionsNow(base::DictionaryValue* args,
                           IPC::Message* reply_message);

#if !defined(NO_TCMALLOC) && (defined(OS_LINUX) || defined(OS_CHROMEOS))
  // Dumps a heap profile.
  // It also checks whether the heap profiler is running, or not.
  // Uses the JSON interface for input/output.
  void HeapProfilerDump(base::DictionaryValue* args,
                        IPC::Message* reply_message);
#endif  // !defined(NO_TCMALLOC) && (defined(OS_LINUX) || defined(OS_CHROMEOS))

  // Overrides the current geoposition.
  // Uses the JSON interface for input/output.
  void OverrideGeoposition(base::DictionaryValue* args,
                           IPC::Message* reply_message);

  // Responds to the Find request and returns the match count.
  void FindInPage(Browser* browser,
                  base::DictionaryValue* args,
                  IPC::Message* reply_message);

  // Opens the find request dialogue in the given browser.
  // Example:
  //   input: { "windex": 1 }
  //   output: none
  void OpenFindInPage(base::DictionaryValue* args,
                      IPC::Message* reply_message);

  // Returns whether the find request dialogue is visible in the given browser.
  // Example:
  //   input: { "windex": 1 }
  //   output: { "is_visible": true }
  void IsFindInPageVisible(base::DictionaryValue* args,
                           IPC::Message* reply_message);

  // Get ordered list of all active and queued HTML5 notifications.
  // Uses the JSON interface for input/output.
  void GetAllNotifications(Browser* browser,
                           base::DictionaryValue* args,
                           IPC::Message* reply_message);

  // Close an active HTML5 notification.
  // Uses the JSON interface for input/output.
  void CloseNotification(Browser* browser,
                         base::DictionaryValue* args,
                         IPC::Message* reply_message);

  // Waits for the number of active HTML5 notifications to reach a given count.
  // Uses the JSON interface for input/output.
  void WaitForNotificationCount(Browser* browser,
                                base::DictionaryValue* args,
                                IPC::Message* reply_message);

  // Gets info about the elements in the NTP.
  // Uses the JSON interface for input/output.
  void GetNTPInfo(Browser* browser,
                  base::DictionaryValue* args,
                  IPC::Message* reply_message);

  // Removes a thumbnail from the NTP's Most Visited sites section.
  // Uses the JSON interface for input/output.
  void RemoveNTPMostVisitedThumbnail(Browser* browser,
                                     base::DictionaryValue* args,
                                     IPC::Message* reply_message);

  // Restores all thumbnails that have been removed (i.e., blacklisted) from the
  // NTP's Most Visited sites section.
  // Uses the JSON interface for input/output.
  void RestoreAllNTPMostVisitedThumbnails(Browser* browser,
                                          base::DictionaryValue* args,
                                          IPC::Message* reply_message);

  // Kills the given renderer process and returns after the associated
  // RenderProcessHost receives notification of its closing.
  void KillRendererProcess(Browser* browser,
                           base::DictionaryValue* args,
                           IPC::Message* reply_message);

  // Populates the fields of the event parameter with what is found in the
  // args parameter.  Upon failure, returns false and puts the error message in
  // the error parameter, otherwise returns true.
  bool BuildWebKeyEventFromArgs(base::DictionaryValue* args,
                                std::string* error,
                                content::NativeWebKeyboardEvent* event);

  // Populates the fields of the event parameter with default data, except for
  // the specified key type and key code.
  void BuildSimpleWebKeyEvent(WebKit::WebInputEvent::Type type,
                              int windows_key_code,
                              content::NativeWebKeyboardEvent* event);

  // Sends a key press event using the given key code to the specified tab.
  // A key press is a combination of a "key down" event and a "key up" event.
  // This function does not wait before returning.
  void SendWebKeyPressEventAsync(int key_code,
                                 content::WebContents* web_contents);

  // Launches the specified app from the currently-selected tab.
  void LaunchApp(Browser* browser,
                 base::DictionaryValue* args,
                 IPC::Message* reply_message);

  // Sets the launch type for the specified app.
  void SetAppLaunchType(Browser* browser,
                        base::DictionaryValue* args,
                        IPC::Message* reply_message);

  // Gets statistics about the v8 heap in a renderer process.
  void GetV8HeapStats(Browser* browser,
                      base::DictionaryValue* args,
                      IPC::Message* reply_message);

  // Gets the current FPS associated with a renderer process view.
  void GetFPS(Browser* browser,
              base::DictionaryValue* args,
              IPC::Message* reply_message);

  // Fullscreen and Mouse Lock hooks. They take no JSON parameters.
  void IsFullscreenForBrowser(Browser* browser,
            base::DictionaryValue* args,
            IPC::Message* reply_message);
  void IsFullscreenForTab(Browser* browser,
            base::DictionaryValue* args,
            IPC::Message* reply_message);
  void IsMouseLocked(Browser* browser,
            base::DictionaryValue* args,
            IPC::Message* reply_message);
  void IsMouseLockPermissionRequested(Browser* browser,
            base::DictionaryValue* args,
            IPC::Message* reply_message);
  void IsFullscreenPermissionRequested(Browser* browser,
            base::DictionaryValue* args,
            IPC::Message* reply_message);
  void IsFullscreenBubbleDisplayed(Browser* browser,
              base::DictionaryValue* args,
              IPC::Message* reply_message);
  void IsFullscreenBubbleDisplayingButtons(Browser* browser,
            base::DictionaryValue* args,
            IPC::Message* reply_message);
  void AcceptCurrentFullscreenOrMouseLockRequest(Browser* browser,
            base::DictionaryValue* args,
            IPC::Message* reply_message);
  void DenyCurrentFullscreenOrMouseLockRequest(Browser* browser,
            base::DictionaryValue* args,
            IPC::Message* reply_message);

  // Waits for all views to stop loading or a modal dialog to become active.
  void WaitForAllViewsToStopLoading(base::DictionaryValue* args,
                                    IPC::Message* reply_message);

  // Gets the browser and tab index of the given tab. Uses the JSON interface.
  // Either "tab_id" or "tab_handle" must be specified, but not both. "tab_id"
  // refers to the ID from the |NavigationController|, while "tab_handle" is
  // the handle number assigned by the automation system.
  // Example:
  //   input: { "tab_id": 1,     // optional
  //            "tab_handle": 3  // optional
  //          }
  //   output: { "windex": 1, "tab_index": 5 }
  void GetIndicesFromTab(base::DictionaryValue* args,
                         IPC::Message* reply_message);

  // Executes a browser command on the given browser window. Does not wait for
  // the command to complete.
  // Example:
  //   input: { "accelerator": 1,
  //            "windex": 1
  //          }
  void ExecuteBrowserCommandAsyncJSON(DictionaryValue* args,
                                      IPC::Message* reply_message);

  // Executes a browser command on the given browser window. Waits for the
  // command to complete before returning.
  // Example:
  //   input: { "accelerator": 1,
  //            "windex": 1
  //          }
  void ExecuteBrowserCommandJSON(DictionaryValue* args,
                                 IPC::Message* reply_message);

  // Checks if a browser command is enabled on the given browser window.
  // Example:
  //   input: { "accelerator": 1,
  //            "windex": 1
  //          }
  //   output: { "enabled": true }
  void IsMenuCommandEnabledJSON(DictionaryValue* args,
                                IPC::Message* reply_message);

  // Returns a dictionary of information about the given tab.
  // Example:
  //   input: { "tab_index": 1,
  //            "windex": 1
  //          }
  //   output: { "title": "Hello World",
  //             "url": "http://foo.bar" }
  void GetTabInfo(DictionaryValue* args,
                  IPC::Message* reply_message);

  // Returns the tab count for the given browser window.
  // Example:
  //   input: { "windex": 1 }
  //   output: { "tab_count": 5 }
  void GetTabCountJSON(DictionaryValue* args,
                       IPC::Message* reply_message);

  // Navigates to the given URL. Uses the JSON interface.
  // The pair |windex| and |tab_index| or the single |auto_id| must be given
  // to specify the tab.
  // Example:
  //   input: { "windex": 1,
  //            "tab_index": 3,
  //            "auto_id": { "type": 0, "id": "awoein" },
  //            "url": "http://www.google.com",
  //            "navigation_count": 1  // number of navigations to wait for
  //          }
  //   output: { "result": AUTOMATION_MSG_NAVIGATION_SUCCESS }
  void NavigateToURL(base::DictionaryValue* args, IPC::Message* reply_message);

  // Get the index of the currently active tab. Uses the JSON interface.
  // The integer |windex| must be given to specify the browser window.
  // Example:
  //   input: { "windex": 1 }
  //   output: { "tab_index": 3 }
  void GetActiveTabIndexJSON(DictionaryValue* args,
                             IPC::Message* reply_message);

  // Append a new tab. Uses the JSON interface.
  // The integer |windex| must be given to specify the browser window. The tab
  // is opened to |url| and blocks until the page loads.
  // Example:
  //   input: { "windex": 1,
  //            "url": "http://google.com"
  //          }
  //   output: { "result": AUTOMATION_MSG_NAVIGATION_SUCCESS }
  void AppendTabJSON(DictionaryValue* args, IPC::Message* reply_message);

  // Waits until any pending navigation completes in the specified tab.
  // The pair |windex| and |tab_index| or the single |auto_id| must be given
  // to specify the tab.
  // Example:
  //   input: { "windex": 1,
  //            "tab_index": 1,
  //            "auto_id": { "type": 0, "id": "awoein" },
  //           }
  //   output: { "result": AUTOMATION_MSG_NAVIGATION_SUCCESS }
  void WaitUntilNavigationCompletes(
      base::DictionaryValue* args, IPC::Message* reply_message);

  // Executes javascript in the specified frame. Uses the JSON interface.
  // Waits for a result from the |DOMAutomationController|. The javascript
  // must send a string.
  // The pair |windex| and |tab_index| or the single |auto_id| must be given
  // to specify the tab.
  // Example:
  //   input: { "windex": 1,
  //            "tab_index": 1,
  //            "auto_id": { "type": 0, "id": "awoein" },
  //            "frame_xpath": "//frames[1]",
  //            "javascript":
  //                "window.domAutomationController.send(window.name)",
  //           }
  //   output: { "result": "My Window Name" }
  // This and some following methods have a suffix of JSON to distingush them
  // from already existing methods which perform the same function, but use
  // custom IPC messages instead of the JSON IPC message. These functions will
  // eventually be replaced with the JSON ones and the JSON suffix will be
  // dropped.
  // TODO(kkania): Replace the non-JSON counterparts and drop the JSON suffix.
  void ExecuteJavascriptJSON(
      base::DictionaryValue* args, IPC::Message* reply_message);

  // Creates a DomEventObserver associated with the AutomationEventQueue.
  // Example:
  //   input: { "event_name": "login complete",
  //            "automation_id": 4444,
  //            "recurring": False
  //          }
  //   output: { "observer_id": 1 }
  void AddDomEventObserver(
      base::DictionaryValue* args, IPC::Message* reply_message);

  // Removes an event observer associated with the AutomationEventQueue.
  // Example:
  //   input: { "observer_id": 1 }
  //   output: none
  void RemoveEventObserver(
      base::DictionaryValue* args, IPC::Message* reply_message);

  // Retrieves an event from the AutomationEventQueue.
  // Blocks if 'blocking' is true, otherwise returns immediately.
  // Example:
  //   input: { "observer_id": 1,
  //            "blocking": true,
  //          }
  //   output: { "type": "raised",
  //             "name": "login complete"
  //             "id": 1,
  //           }
  void GetNextEvent(base::DictionaryValue* args, IPC::Message* reply_message);

  // Removes all events and observers attached to the AutomationEventQueue.
  // Example:
  //   input: none
  //   output: none
  void ClearEventQueue(
      base::DictionaryValue* args, IPC::Message* reply_message);

  // Executes javascript in the specified frame of a render view.
  // Uses the JSON interface. Waits for a result from the
  // |DOMAutomationController|. The javascript must send a string.
  // Example:
  //   input: { "view": {
  //              "render_process_id": 1,
  //              "render_view_id": 2,
  //            }
  //            "frame_xpath": "//frames[1]",
  //            "javascript":
  //                "window.domAutomationController.send(window.name)",
  //           }
  //   output: { "result": "My Window Name" }
  void ExecuteJavascriptInRenderView(
      base::DictionaryValue* args, IPC::Message* reply_message);

  // Goes forward in the specified tab. Uses the JSON interface.
  // The pair |windex| and |tab_index| or the single |auto_id| must be given
  // to specify the tab.
  // Example:
  //   input: { "windex": 1,
  //            "tab_index": 1,
  //            "auto_id": { "type": 0, "id": "awoein" }
  //          }
  //   output: { "did_go_forward": true,                      // optional
  //             "result": AUTOMATION_MSG_NAVIGATION_SUCCESS  // optional
  //           }
  void GoForward(base::DictionaryValue* args, IPC::Message* reply_message);

  // Goes back in the specified tab. Uses the JSON interface.
  // The pair |windex| and |tab_index| or the single |auto_id| must be given
  // to specify the tab.
  // Example:
  //   input: { "windex": 1,
  //            "tab_index": 1,
  //            "auto_id": { "type": 0, "id": "awoein" }
  //          }
  //   output: { "did_go_back": true,                         // optional
  //             "result": AUTOMATION_MSG_NAVIGATION_SUCCESS  // optional
  //           }
  void GoBack(base::DictionaryValue* args, IPC::Message* reply_message);

  // Reload the specified tab. Uses the JSON interface.
  // The pair |windex| and |tab_index| or the single |auto_id| must be given
  // to specify the tab.
  // Example:
  //   input: { "windex": 1,
  //            "tab_index": 1,
  //            "auto_id": { "type": 0, "id": "awoein" }
  //          }
  //   output: { "result": AUTOMATION_MSG_NAVIGATION_SUCCESS  // optional }
  void ReloadJSON(base::DictionaryValue* args, IPC::Message* reply_message);

  // Captures the entire page of the the specified tab, including the
  // non-visible portions of the page, and saves the PNG to a file.
  // The pair |windex| and |tab_index| or the single |auto_id| must be given
  // to specify the tab.
  // Example:
  //   input: { "windex": 1,
  //            "tab_index": 1,
  //            "auto_id": { "type": 0, "id": "awoein" },
  //            "path": "/tmp/foo.png"
  //          }
  //   output: none
  void CaptureEntirePageJSON(
      base::DictionaryValue* args, IPC::Message* reply_message);

  // Gets the cookies for the given URL. Uses the JSON interface.
  // "expiry" refers to the amount of seconds since the Unix epoch. If omitted,
  // the cookie is valid for the duration of the browser session.
  // Example:
  //   input: { "url": "http://www.google.com" }
  //   output: { "cookies": [
  //               {
  //                 "name": "PREF",
  //                 "value": "123101",
  //                 "path": "/",
  //                 "domain": "www.google.com",
  //                 "secure": false,
  //                 "expiry": 1401982012
  //               }
  //             ]
  //           }
  void GetCookiesJSON(base::DictionaryValue* args, IPC::Message* reply_message);

  // Deletes the cookie with the given name for the URL. Uses the JSON
  // interface.
  // Example:
  //   input: {
  //            "url": "http://www.google.com",
  //            "name": "my_cookie"
  //          }
  //   output: none
  void DeleteCookieJSON(base::DictionaryValue* args,
                        IPC::Message* reply_message);

  // Sets a cookie for the given URL. Uses the JSON interface.
  // "expiry" refers to the amount of seconds since the Unix epoch. If omitted,
  // the cookie will be valid for the duration of the browser session.
  // "domain" refers to the applicable domain for the cookie. Valid domain
  // choices for the site "http://www.google.com" and resulting cookie
  // applicability:
  //   [.]www.google.com - applicable on www.google.com and its subdomains
  //   [.]google.com - applicable on google.com and its subdomains
  //   <none> - applicable only on www.google.com
  //
  // Example:
  //   input: { "url": "http://www.google.com",
  //            "cookie": {
  //              "name": "PREF",
  //              "value": "123101",
  //              "path": "/",                  // optional
  //              "domain": ".www.google.com",  // optional
  //              "secure": false,              // optional
  //              "expiry": 1401982012          // optional
  //            }
  //          }
  //   output: none
  void SetCookieJSON(base::DictionaryValue* args, IPC::Message* reply_message);

  // Gets the cookies for the given URL in the context of a given browser
  // window. Uses the JSON interface.
  // Example:
  //   input: { "url": "http://www.google.com",
  //            "tab_index": 1,
  //            "windex": 1,
  //          }
  //   output: { "cookies": "foo=bar" }
  void GetCookiesInBrowserContext(base::DictionaryValue* args,
                                  IPC::Message* reply_message);

  // Deletes the cookie with the given name for the URL in the context of a
  // given browser window. Uses the JSON interface.
  // Example:
  //   input: { "url": "http://www.google.com",
  //            "cookie_name": "my_cookie"
  //            "tab_index": 1,
  //            "windex": 1,
  //          }
  //   output: none
  void DeleteCookieInBrowserContext(base::DictionaryValue* args,
                                    IPC::Message* reply_message);

  // Sets a cookie for the given URL in the context of a given browser window.
  // Uses the JSON interface.
  //
  // Example:
  //   input: { "url": "http://www.google.com",
  //            "value": "name=value; Expires=Wed, 09 Jun 2021 10:18:14 GMT",
  //            "tab_index": 1,
  //            "windex": 1,
  //          }
  //   output: none
  void SetCookieInBrowserContext(base::DictionaryValue* args,
                                 IPC::Message* reply_message);

  // Gets the ID for every open tab. This ID is unique per session.
  // Example:
  //   input: none
  //   output: { "ids": [213, 1] }
  void GetTabIds(base::DictionaryValue* args, IPC::Message* reply_message);

  // Gets info about all open views. Each view ID is unique per session.
  // Example:
  //   input: none
  //   output: { "views": [
  //               {
  //                 "auto_id": { "type": 0, "id": "awoein" },
  //                 "extension_id": "askjeoias3"  // optional
  //               }
  //             ]
  //           }
  void GetViews(base::DictionaryValue* args, IPC::Message* reply_message);

  // Checks if the given tab ID refers to an open tab.
  // Example:
  //   input: { "id": 41 }
  //   output: { "is_valid": false }
  void IsTabIdValid(base::DictionaryValue* args, IPC::Message* reply_message);

  // Checks if the given automation ID refers to an actual object.
  // Example:
  //   input: { "auto_id": { "type": 0, "id": "awoein" } }
  //   output: { "does_exist": false }
  void DoesAutomationObjectExist(
      base::DictionaryValue* args, IPC::Message* reply_message);

  // Closes the specified tab.
  // The pair |windex| and |tab_index| or the single |auto_id| must be given
  // to specify the tab.
  // Example:
  //   input: { "windex": 1,
  //            "tab_index": 1,
  //            "auto_id": { "type": 0, "id": "awoein" }
  //          }
  //   output: none
  void CloseTabJSON(base::DictionaryValue* args, IPC::Message* reply_message);

  // Sets the specified web view bounds.
  // The single |auto_id| must be given to specify the view.
  // This method currently is only supported for tabs.
  // Example:
  //   input: { "auto_id": { "type": 0, "id": "awoein" },
  //            "bounds": {
  //              "x": 100,
  //              "y": 200,
  //              "width": 500,
  //              "height": 800
  //            }
  //          }
  //   output: none
  void SetViewBounds(base::DictionaryValue* args, IPC::Message* reply_message);

  // Maximizes the web view.
  // The single |auto_id| must be given to specify the view.
  // This method currently is only supported for tabs.
  // Example:
  //   input: { "auto_id": { "type": 0, "id": "awoein" } }
  //   output: none
  void MaximizeView(base::DictionaryValue* args, IPC::Message* reply_message);

  // Sends the WebKit events for a mouse click at a given coordinate.
  // The pair |windex| and |tab_index| or the single |auto_id| must be given
  // to specify the render view.
  // Example:
  //   input: { "windex": 1,
  //            "tab_index": 1,
  //            "auto_id": { "type": 0, "id": "awoein" },
  //            "button": automation::kLeftButton,
  //            "x": 100,
  //            "y": 100
  //          }
  //   output: none
  void WebkitMouseClick(base::DictionaryValue* args,
                        IPC::Message* message);

  // Sends the WebKit event for a mouse move to a given coordinate.
  // The pair |windex| and |tab_index| or the single |auto_id| must be given
  // to specify the render view.
  // Example:
  //   input: { "windex": 1,
  //            "tab_index": 1,
  //            "auto_id": { "type": 0, "id": "awoein" },
  //            "x": 100,
  //            "y": 100
  //          }
  //   output: none
  void WebkitMouseMove(base::DictionaryValue* args,
                       IPC::Message* message);

  // Sends the WebKit events for a mouse drag between two coordinates.
  // The pair |windex| and |tab_index| or the single |auto_id| must be given
  // to specify the render view.
  // Example:
  //   input: { "windex": 1,
  //            "tab_index": 1,
  //            "auto_id": { "type": 0, "id": "awoein" },
  //            "start_x": 100,
  //            "start_y": 100,
  //            "end_x": 100,
  //            "end_y": 100
  //          }
  //   output: none
  void WebkitMouseDrag(base::DictionaryValue* args,
                       IPC::Message* message);

  // Sends the WebKit events for a mouse button down at a given coordinate.
  // The pair |windex| and |tab_index| or the single |auto_id| must be given
  // to specify the render view.
  // Example:
  //   input: { "windex": 1,
  //            "tab_index": 1,
  //            "auto_id": { "type": 0, "id": "awoein" },
  //            "x": 100,
  //            "y": 100
  //          }
  //   output: none
  void WebkitMouseButtonDown(base::DictionaryValue* args,
                             IPC::Message* message);

  // Sends the WebKit events for a mouse button up at a given coordinate.
  // The pair |windex| and |tab_index| or the single |auto_id| must be given
  // to specify the render view.
  // Example:
  //   input: { "windex": 1,
  //            "tab_index": 1,
  //            "auto_id": { "type": 0, "id": "awoein" },
  //            "x": 100,
  //            "y": 100
  //          }
  //   output: none
  void WebkitMouseButtonUp(base::DictionaryValue* args,
                           IPC::Message* message);

  // Sends the WebKit events for a mouse double click at a given coordinate.
  // The pair |windex| and |tab_index| or the single |auto_id| must be given
  // to specify the render view.
  // Example:
  //   input: { "windex": 1,
  //            "tab_index": 1,
  //            "auto_id": { "type": 0, "id": "awoein" },
  //            "x": 100,
  //            "y": 100
  //          }
  //   output: none
  void WebkitMouseDoubleClick(base::DictionaryValue* args,
                              IPC::Message* message);

  // Drag and drop file paths at a given coordinate.
  // The pair |windex| and |tab_index| or the single |auto_id| must be given
  // to specify the render view.
  // Example:
  //   input: { "windex": 1,
  //            "tab_index": 1,
  //            "auto_id": { "type": 0, "id": "awoein" },
  //            "x": 100,
  //            "y": 100,
  //            "paths": [
  //              "/tmp/file.txt"
  //            ],
  //          }
  //   output: none
  void DragAndDropFilePaths(base::DictionaryValue* args,
                            IPC::Message* message);

  // Sends the WebKit key event with the specified properties.
  // The pair |windex| and |tab_index| or the single |auto_id| must be given
  // to specify the render view.
  // Example:
  //   input: { "windex": 1,
  //            "tab_index": 1,
  //            "auto_id": { "type": 0, "id": "awoein" },
  //            "type": automation::kRawKeyDownType,
  //            "nativeKeyCode": ui::VKEY_X,
  //            "windowsKeyCode": ui::VKEY_X,
  //            "unmodifiedText": "x",
  //            "text": "X",
  //            "modifiers": automation::kShiftKeyMask,
  //            "isSystemKey": false
  //          }
  //   output: none
  void SendWebkitKeyEvent(base::DictionaryValue* args,
                          IPC::Message* message);

  // Processes the WebKit mouse event with the specified properties.
  // The pair |windex| and |tab_index| or the single |auto_id| must be given
  // to specify the render view.
  // Example:
  //   input: { "windex": 1,
  //            "tab_index": 1,
  //            "auto_id": { "type": 0, "id": "awoein" },
  //            "type": automation::kMouseDown,
  //            "button": automation::kLeftButton,
  //            "x": 100,
  //            "y": 200,
  //            "click_count": 1,
  //            "modifiers": automation::kShiftKeyMask,
  //          }
  //   output: none
  void ProcessWebMouseEvent(base::DictionaryValue* args,
                            IPC::Message* message);

  // Gets the active JavaScript modal dialog's message.
  // Example:
  //   input: none
  //   output: { "message": "This is an alert!" }
  void GetAppModalDialogMessage(
      base::DictionaryValue* args, IPC::Message* reply_message);

  // Accepts or dismisses the active JavaScript modal dialog. If optional
  // prompt text is given, it will be used as the result of the prompt dialog.
  // Example:
  //   input: { "accept": true,
  //            "prompt_text": "hello"  // optional
  //          }
  //   output: none
  void AcceptOrDismissAppModalDialog(
      base::DictionaryValue* args, IPC::Message* reply_message);

  // Activates the given tab.
  // The pair |windex| and |tab_index| or the single |auto_id| must be given
  // to specify the tab.
  // Example:
  //   input: { "windex": 1,
  //            "tab_index": 1,
  //            "auto_id": { "type": 0, "id": "awoein" }
  //          }
  //   output: none
  void ActivateTabJSON(base::DictionaryValue* args, IPC::Message* message);

  // Blocks until the given tab is restored.
  // Uses the JSON interface.
  void WaitForTabToBeRestored(DictionaryValue* args,
                              IPC::Message* reply_message);

  // Simulates an action on the SSL blocking page at the specified tab.
  // If |proceed| is true, it is equivalent to the user pressing the
  // 'Proceed' button, if false the 'Get me out of there button'.
  // Note that this fails if the tab is not displaying a SSL blocking page.
  // Uses the JSON interface.
  // Example:
  //   input: { "windex": 1,
  //            "tab_index": 1,
  //            "proceed": true
  //          }
  //   output: none
  void ActionOnSSLBlockingPage(DictionaryValue* args,
                               IPC::Message* reply_message);

  // Gets the security state for the given tab. Uses the JSON interface.
  // Example:
  //   input: { "windex": 1,
  //            "tab_index": 1,
  //          }
  //   output: { "security_style": SECURITY_STYLE_AUTHENTICATED,
  //             "ssl_cert_status": 3,  // bitmask of status flags
  //             "insecure_content_status": 1,  // bitmask of ContentStatusFlags
  //           }
  void GetSecurityState(DictionaryValue* args,
                        IPC::Message* reply_message);

  // Brings the given brower's window to the front.
  // Example:
  //   input: { "windex": 1 }
  //   output: none
  void BringBrowserToFrontJSON(base::DictionaryValue* args,
                               IPC::Message* message);

  // Gets the version of ChromeDriver automation supported by this server.
  // Example:
  //   input: none
  //   output: { "version": 1 }
  void GetChromeDriverAutomationVersion(base::DictionaryValue* args,
                                        IPC::Message* message);

  // Determines whether the extension page action is visible in the given tab.
  // Example:
  //   input: { "auto_id": { "type": 0, "id": "awoein" },
  //            "extension_id": "byzaaoiea",
  //          }
  //   output: none
  void IsPageActionVisible(base::DictionaryValue* args,
                           IPC::Message* reply_message);

  // Creates a new |TestingAutomationProvider| that opens a server channel
  // for the given |channel_id|.
  // The server channel will be available for connection when this returns.
  // Example:
  //   input: { "channel_id": "testChannel123" }
  void CreateNewAutomationProvider(base::DictionaryValue* args,
                                   IPC::Message* reply_message);

  // Triggers a policy update on the platform and cloud providers, if they
  // exist. Returns after the update notifications are received.
  // Example:
  //   input: none
  //   output: none
  void RefreshPolicies(base::DictionaryValue* args,
                       IPC::Message* reply_message);

  // Simulates a memory bug (reference an array out of bounds) to cause Address
  // Sanitizer (if it was built it) to catch the bug and abort the process.
  // Example:
  //   input: none
  //   output: none
  void SimulateAsanMemoryBug(base::DictionaryValue* args,
                             IPC::Message* reply_message);

#if defined(OS_CHROMEOS)
  // OOBE wizard.

  // Accepts the network screen and continues to EULA.
  // Example:
  //   input: none
  //   ouput: { "next_screen": "eula" }
  void AcceptOOBENetworkScreen(base::DictionaryValue* args,
                               IPC::Message* reply_message);

  // Accepts or declines EULA, moving forward or back from EULA screen.
  // Example:
  //    input: { "accepted": true, "usage_stats_reporting": false }
  //    output: { "next_screen": "update" }
  void AcceptOOBEEula(base::DictionaryValue* args, IPC::Message* reply_message);

  // Forces the ongoing update to cancel and proceed to the login screen.
  // Example:
  //    input: none
  //    output: { "next_screen": "login" }
  void CancelOOBEUpdate(base::DictionaryValue* args,
                        IPC::Message* reply_message);

  // Chooses user image on the image picker screen and starts browser session.
  // Example:
  //    input: { "image": "profile" } - Google profile image
  //    input: { "image": 2 } - default image number 2 (0-based)
  //    output: { "next_screen": "session" }
  void PickUserImage(base::DictionaryValue* args, IPC::Message* reply_message);

  // Skips OOBE to login step. Can be called when already at login screen,
  // in which case does nothing and sends return value immediately.
  // Example:
  //    input: { "skip_image_selection": true }
  //    output: { "next_screen": "login" }
  void SkipToLogin(DictionaryValue* args, IPC::Message* reply_message);

  // Returns info about the current OOBE screen.
  // Example:
  //    input: none
  //    output: { "screen_name": "network" }
  //    output: none  (when already logged in)
  void GetOOBEScreenInfo(DictionaryValue* args, IPC::Message* reply_message);

  // Login / Logout.
  void GetLoginInfo(base::DictionaryValue* args, IPC::Message* reply_message);

  void ShowCreateAccountUI(base::DictionaryValue* args,
                           IPC::Message* reply_message);

  void LoginAsGuest(base::DictionaryValue* args, IPC::Message* reply_message);

  // Submits the Chrome OS login form. Watch for the login to complete using
  // the AddLoginObserver and GetNextEvent commands.
  // Example:
  //   input: { "username": "user@gmail.com",
  //            "password": "fakepassword",
  //          }
  void SubmitLoginForm(base::DictionaryValue* args,
                       IPC::Message* reply_message);

  void AddLoginEventObserver(DictionaryValue* args,
                             IPC::Message* reply_message);

  // Executes javascript in the specified frame in the OOBE WebUI on chromeos.
  // Waits for a result from the |DOMAutomationController|. The javascript must
  // send a string. Must be run before a user has logged in.
  // Example:
  //   input: { "frame_xpath": "//frames[1]",
  //            "javascript":
  //                "window.domAutomationController.send(window.name)",
  //           }
  //   output: { "result": "My Window Name" }
  void ExecuteJavascriptInOOBEWebUI(
      base::DictionaryValue* args, IPC::Message* reply_message);

  void SignOut(base::DictionaryValue* args, IPC::Message* reply_message);

  // Screen locker.
  void LockScreen(base::DictionaryValue* args, IPC::Message* reply_message);

  void UnlockScreen(base::DictionaryValue* args, IPC::Message* reply_message);

  void SignoutInScreenLocker(base::DictionaryValue* args,
                             IPC::Message* reply_message);

  // Battery.
  void GetBatteryInfo(base::DictionaryValue* args, IPC::Message* reply_message);

  // Network.
  void GetNetworkInfo(base::DictionaryValue* args, IPC::Message* reply_message);

  void NetworkScan(base::DictionaryValue* args, IPC::Message* reply_message);

  void ToggleNetworkDevice(base::DictionaryValue* args,
                           IPC::Message* reply_message);

  void GetProxySettings(base::DictionaryValue* args,
                        IPC::Message* reply_message);

  void SetProxySettings(base::DictionaryValue* args,
                        IPC::Message* reply_message);

  void SetSharedProxies(base::DictionaryValue* args,
                        IPC::Message* reply_message);

  void RefreshInternetDetails(base::DictionaryValue* args,
                              IPC::Message* reply_message);

  void ConnectToCellularNetwork(base::DictionaryValue* args,
                            IPC::Message* reply_message);

  void DisconnectFromCellularNetwork(base::DictionaryValue* args,
                                 IPC::Message* reply_message);

  void ConnectToWifiNetwork(base::DictionaryValue* args,
                            IPC::Message* reply_message);

  void ConnectToHiddenWifiNetwork(base::DictionaryValue* args,
                                  IPC::Message* reply_message);

  void DisconnectFromWifiNetwork(base::DictionaryValue* args,
                                 IPC::Message* reply_message);

  void ForgetWifiNetwork(DictionaryValue* args, IPC::Message* reply_message);

  // VPN.
  void AddPrivateNetwork(DictionaryValue* args, IPC::Message* reply_message);

  void GetPrivateNetworkInfo(base::DictionaryValue* args,
                             IPC::Message* reply_message);

  void ConnectToPrivateNetwork(base::DictionaryValue* args,
                               IPC::Message* reply_message);

  void DisconnectFromPrivateNetwork(base::DictionaryValue* args,
                                    IPC::Message* reply_message);

  // Accessibility.
  void EnableSpokenFeedback(DictionaryValue* args, IPC::Message* reply_message);

  void IsSpokenFeedbackEnabled(DictionaryValue* args,
                               IPC::Message* reply_message);

  // Time.
  void GetTimeInfo(Browser* browser, base::DictionaryValue* args,
                   IPC::Message* reply_message);

  void GetTimeInfo(base::DictionaryValue* args, IPC::Message* reply_message);

  void SetTimezone(base::DictionaryValue* args, IPC::Message* reply_message);

  // Update.
  void GetUpdateInfo(base::DictionaryValue* args, IPC::Message* reply_message);

  void UpdateCheck(base::DictionaryValue* args, IPC::Message* reply_message);

  void SetReleaseTrack(base::DictionaryValue* args,
                       IPC::Message* reply_message);

  // Volume.
  void GetVolumeInfo(base::DictionaryValue* args, IPC::Message* reply_message);

  void SetVolume(base::DictionaryValue* args, IPC::Message* reply_message);

  void SetMute(base::DictionaryValue* args, IPC::Message* reply_message);

  void CaptureProfilePhoto(Browser* browser,
                           DictionaryValue* args,
                           IPC::Message* reply_message);

  // Html terminal.
  void OpenCrosh(base::DictionaryValue* args, IPC::Message* reply_message);

  void AddChromeosObservers();
  void RemoveChromeosObservers();

#endif  // defined(OS_CHROMEOS)

  void WaitForTabCountToBecome(int browser_handle,
                               int target_tab_count,
                               IPC::Message* reply_message);

  void WaitForInfoBarCount(int tab_handle,
                           size_t target_count,
                           IPC::Message* reply_message);

  void WaitForProcessLauncherThreadToGoIdle(IPC::Message* reply_message);

  void OnRemoveProvider();  // Called via PostTask

  // Execute Javascript in the context of a specific render view.
  void ExecuteJavascriptInRenderViewFrame(
      const string16& frame_xpath, const string16& script,
      IPC::Message* reply_message, content::RenderViewHost* render_view_host);

  // Selects the given |tab| if not selected already.
  void EnsureTabSelected(Browser* browser, content::WebContents* tab);

#if defined(OS_CHROMEOS)
  // Avoid scoped ptr here to avoid having to define it completely in the
  // non-ChromeOS code.
  PowerManagerClientObserverForTesting* power_manager_observer_;
#endif  // defined(OS_CHROMEOS)

  std::map<std::string, JsonHandler> handler_map_;
  std::map<std::string, BrowserJsonHandler> browser_handler_map_;

  content::NotificationRegistrar registrar_;

  // Used to enumerate browser profiles.
  scoped_refptr<ImporterList> importer_list_;

  // The stored data for the ImportSettings operation.
  ImportSettingsData import_settings_data_;

  // The automation event observer queue. It is lazily created when an observer
  // is added to avoid overhead when not needed.
  scoped_ptr<AutomationEventQueue> automation_event_queue_;

  // List of commands which just finish synchronously and don't require
  // setting up an observer.
  static const int kSynchronousCommands[];

  DISALLOW_COPY_AND_ASSIGN(TestingAutomationProvider);
};

#endif  // CHROME_BROWSER_AUTOMATION_TESTING_AUTOMATION_PROVIDER_H_
