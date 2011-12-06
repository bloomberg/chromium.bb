// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOMATION_TESTING_AUTOMATION_PROVIDER_H_
#define CHROME_BROWSER_AUTOMATION_TESTING_AUTOMATION_PROVIDER_H_
#pragma once

#include <map>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/automation/automation_provider.h"
#include "chrome/browser/automation/automation_provider_json.h"
#include "chrome/browser/history/history.h"
#include "chrome/browser/importer/importer_list_observer.h"
#include "chrome/browser/sync/profile_sync_service_harness.h"
#include "chrome/browser/ui/browser_list.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/common/page_type.h"
#include "net/base/cert_status_flags.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebInputEvent.h"

#if defined(OS_CHROMEOS)
// TODO(sque): move to a ChromeOS-specific class.  See crosbug.com/22081.
#include "chrome/browser/chromeos/dbus/power_manager_client.h"
#endif  // defined(OS_CHROMEOS)

class AutofillProfile;
class CreditCard;
class ImporterList;

namespace base {
class DictionaryValue;
}

namespace webkit {
struct WebPluginInfo;
}

// This is an automation provider containing testing calls.
class TestingAutomationProvider : public AutomationProvider,
                                  public BrowserList::Observer,
                                  public importer::ImporterListObserver,
#if defined(OS_CHROMEOS)
                                  public chromeos::PowerManagerClient::Observer,
#endif  // defined(OS_CHROMEOS)
                                  public content::NotificationObserver {
 public:
  explicit TestingAutomationProvider(Profile* profile);

  virtual IPC::Channel::Mode GetChannelMode(bool use_named_interface);

  // IPC::Channel::Listener:
  virtual bool OnMessageReceived(const IPC::Message& msg) OVERRIDE;
  virtual void OnChannelError() OVERRIDE;

 private:
  class PopupMenuWaiter;

  // Storage for ImportSettings() to resume operations after a callback.
  struct ImportSettingsData {
    string16 browser_name;
    int import_items;
    bool first_run;
    Browser* browser;
    IPC::Message* reply_message;
  };

  virtual ~TestingAutomationProvider();

  // BrowserList::Observer:
  virtual void OnBrowserAdded(const Browser* browser) OVERRIDE;
  virtual void OnBrowserRemoved(const Browser* browser) OVERRIDE;

  // importer::ImporterListObserver:
  virtual void OnSourceProfilesLoaded() OVERRIDE;

  // content::NotificationObserver:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // IPC Message callbacks.
  void CloseBrowser(int handle, IPC::Message* reply_message);
  void CloseBrowserAsync(int browser_handle);
  void ActivateTab(int handle, int at_index, int* status);
  void AppendTab(int handle, const GURL& url, IPC::Message* reply_message);
  void GetActiveTabIndex(int handle, int* active_tab_index);
  void CloseTab(int tab_handle, bool wait_until_closed,
                IPC::Message* reply_message);
  void GetCookies(const GURL& url, int handle, int* value_size,
                  std::string* value);
  void SetCookie(const GURL& url,
                 const std::string& value,
                 int handle,
                 int* response_value);
  void DeleteCookie(const GURL& url, const std::string& cookie_name,
                    int handle, bool* success);
  void ShowCollectedCookiesDialog(int handle, bool* success);
  void NavigateToURLBlockUntilNavigationsComplete(int handle, const GURL& url,
                                                  int number_of_navigations,
                                                  IPC::Message* reply_message);
  void NavigationAsync(int handle, const GURL& url, bool* status);
  void NavigationAsyncWithDisposition(int handle,
                                      const GURL& url,
                                      WindowOpenDisposition disposition,
                                      bool* status);
  void Reload(int handle, IPC::Message* reply_message);
  void SetAuth(int tab_handle, const std::wstring& username,
               const std::wstring& password, IPC::Message* reply_message);
  void CancelAuth(int tab_handle, IPC::Message* reply_message);
  void NeedsAuth(int tab_handle, bool* needs_auth);
  void GetRedirectsFrom(int tab_handle,
                        const GURL& source_url,
                        IPC::Message* reply_message);
  void GetBrowserWindowCount(int* window_count);
  void GetNormalBrowserWindowCount(int* window_count);
  // Be aware that the browser window returned might be of non TYPE_TABBED
  // or in incognito mode.
  void GetBrowserWindow(int index, int* handle);
  void FindTabbedBrowserWindow(int* handle);
  void GetLastActiveBrowserWindow(int* handle);
  void GetActiveWindow(int* handle);
  void ExecuteBrowserCommandAsync(int handle, int command, bool* success);
  void ExecuteBrowserCommand(int handle, int command,
                             IPC::Message* reply_message);
  void GetBrowserLocale(string16* locale);
  void IsWindowActive(int handle, bool* success, bool* is_active);
  void ActivateWindow(int handle);
  void IsWindowMaximized(int handle, bool* is_maximized, bool* success);
  void TerminateSession(int handle, bool* success);
  void WindowGetViewBounds(int handle, int view_id, bool screen_coordinates,
                           bool* success, gfx::Rect* bounds);
  void GetWindowBounds(int handle, gfx::Rect* bounds, bool* result);
  void SetWindowBounds(int handle, const gfx::Rect& bounds, bool* result);
  void SetWindowVisible(int handle, bool visible, bool* result);
  void WindowSimulateClick(const IPC::Message& message,
                           int handle,
                           const gfx::Point& click,
                           int flags);
  void WindowSimulateMouseMove(const IPC::Message& message,
                               int handle,
                               const gfx::Point& location);
  void WindowSimulateKeyPress(const IPC::Message& message,
                              int handle,
                              int key,
                              int flags);
  void GetTabCount(int handle, int* tab_count);
  void GetType(int handle, int* type_as_int);
  void IsBrowserInApplicationMode(int handle,
                                  bool* is_application,
                                  bool* success);
  void GetTab(int win_handle, int tab_index, int* tab_handle);
  void GetTabProcessID(int handle, int* process_id);
  void GetTabTitle(int handle, int* title_string_size, std::wstring* title);
  void GetTabIndex(int handle, int* tabstrip_index);
  void GetTabURL(int handle, bool* success, GURL* url);
  void GetShelfVisibility(int handle, bool* visible);
  void IsFullscreen(int handle, bool* is_fullscreen);
  void GetFullscreenBubbleVisibility(int handle, bool* is_visible);

  void ExecuteJavascript(int handle,
                         const std::wstring& frame_xpath,
                         const std::wstring& script,
                         IPC::Message* reply_message);

  void GetConstrainedWindowCount(int handle, int* count);

#if defined(TOOLKIT_VIEWS)
  void GetFocusedViewID(int handle, int* view_id);

  // Block until the focused view ID changes to something other than
  // previous_view_id.
  void WaitForFocusedViewIDToChange(int handle,
                                    int previous_view_id,
                                    IPC::Message* reply_message);

  // Start tracking popup menus. Must be called before executing the
  // command that might open the popup menu; then call WaitForPopupMenuToOpen.
  void StartTrackingPopupMenus(int browser_handle, bool* success);

  // Wait until a popup menu has opened.
  void WaitForPopupMenuToOpen(IPC::Message* reply_message);
#endif  // defined(TOOLKIT_VIEWS)

  void HandleInspectElementRequest(int handle,
                                   int x,
                                   int y,
                                   IPC::Message* reply_message);

  void GetDownloadDirectory(int handle, FilePath* download_directory);

  // If |show| is true, call Show() on the new window after creating it.
  void OpenNewBrowserWindowOfType(int type,
                                  bool show,
                                  IPC::Message* reply_message);

  // Retrieves a Browser from a Window and vice-versa.
  void GetWindowForBrowser(int window_handle, bool* success, int* handle);
  void GetBrowserForWindow(int window_handle, bool* success,
                           int* browser_handle);

  void ShowInterstitialPage(int tab_handle,
                            const std::string& html_text,
                            IPC::Message* reply_message);
  void HideInterstitialPage(int tab_handle, bool* success);

  void WaitForTabToBeRestored(int tab_handle, IPC::Message* reply_message);

  // Gets the security state for the tab associated to the specified |handle|.
  void GetSecurityState(int handle,
                        bool* success,
                        content::SecurityStyle* security_style,
                        net::CertStatus* ssl_cert_status,
                        int* insecure_content_status);

  // Gets the page type for the tab associated to the specified |handle|.
  void GetPageType(int handle, bool* success, content::PageType* page_type);

  // Gets the duration in ms of the last event matching |event_name|.
  // |duration_ms| is -1 if the event hasn't occurred yet.
  void GetMetricEventDuration(const std::string& event_name, int* duration_ms);

  // Simulates an action on the SSL blocking page at the tab specified by
  // |handle|. If |proceed| is true, it is equivalent to the user pressing the
  // 'Proceed' button, if false the 'Get me out of there button'.
  // Not that this fails if the tab is not displaying a SSL blocking page.
  void ActionOnSSLBlockingPage(int handle,
                               bool proceed,
                               IPC::Message* reply_message);

  // Brings the browser window to the front and activates it.
  void BringBrowserToFront(int browser_handle, bool* success);

  // Checks to see if a command on the browser's CommandController is enabled.
  void IsMenuCommandEnabled(int browser_handle,
                            int message_num,
                            bool* menu_item_enabled);

  // Prints the current tab immediately.
  void PrintNow(int tab_handle, IPC::Message* reply_message);

  // Save the current web page.
  void SavePage(int tab_handle,
                const FilePath& file_name,
                const FilePath& dir_path,
                int type,
                bool* success);

  // Responds to requests to open the FindInPage window.
  void HandleOpenFindInPageRequest(const IPC::Message& message,
                                   int handle);

  // Get the visibility state of the Find window.
  void GetFindWindowVisibility(int handle, bool* visible);

  // Responds to requests to find the location of the Find window.
  void HandleFindWindowLocationRequest(int handle, int* x, int* y);

  // Get the visibility state of the Bookmark bar.
  void GetBookmarkBarVisibility(
      int handle, bool* visible, bool* animating, bool* detached);

  // Get the bookmarks as a JSON string.
  void GetBookmarksAsJSON(int handle, std::string* bookmarks_as_json,
                          bool* success);

  // Wait for the bookmark model to load.
  void WaitForBookmarkModelToLoad(int handle, IPC::Message* reply_message);

  // Set |loaded| to true if the bookmark model has loaded, else false.
  void BookmarkModelHasLoaded(int handle, bool* loaded);

  // Editing, modification, and removal of bookmarks.
  // Bookmarks are referenced by id.
  void AddBookmarkGroup(int handle,
                        int64 parent_id, int index, std::wstring title,
                        bool* success);
  void AddBookmarkURL(int handle,
                      int64 parent_id, int index,
                      std::wstring title, const GURL& url,
                      bool* success);
  void ReparentBookmark(int handle,
                        int64 id, int64 new_parent_id, int index,
                        bool* success);
  void SetBookmarkTitle(int handle,
                        int64 id, std::wstring title,
                        bool* success);
  void SetBookmarkURL(int handle,
                      int64 id, const GURL& url,
                      bool* success);
  void RemoveBookmark(int handle,
                      int64 id,
                      bool* success);

  // Retrieves the number of info-bars currently showing in |count|.
  void GetInfoBarCount(int handle, size_t* count);

  // Causes a click on the "accept" button of the info-bar at |info_bar_index|.
  // If |wait_for_navigation| is true, it sends the reply after a navigation has
  // occurred.
  void ClickInfoBarAccept(int handle,
                          size_t info_bar_index,
                          bool wait_for_navigation,
                          IPC::Message* reply_message);

  // Retrieves the last time a navigation occurred for the tab.
  void GetLastNavigationTime(int handle, int64* last_navigation_time);

  // Waits for a new navigation in the tab if none has happened since
  // |last_navigation_time|.
  void WaitForNavigation(int handle,
                         int64 last_navigation_time,
                         IPC::Message* reply_message);

  // Sets the int value for preference with name |name|.
  void SetIntPreference(int handle,
                        const std::string& name,
                        int value,
                        bool* success);

  // Sets the string value for preference with name |name|.
  void SetStringPreference(int handle,
                           const std::string& name,
                           const std::string& value,
                           bool* success);

  // Gets the bool value for preference with name |name|.
  void GetBooleanPreference(int handle,
                            const std::string& name,
                            bool* success,
                            bool* value);

  // Sets the bool value for preference with name |name|.
  void SetBooleanPreference(int handle,
                            const std::string& name,
                            bool value,
                            bool* success);

  void GetShowingAppModalDialog(bool* showing_dialog, int* dialog_button);
  void ClickAppModalDialogButton(int button, bool* success);

  void WaitForBrowserWindowCountToBecome(int target_count,
                                         IPC::Message* reply_message);

  void WaitForAppModalDialogToBeShown(IPC::Message* reply_message);

  void GoBackBlockUntilNavigationsComplete(int handle,
                                           int number_of_navigations,
                                           IPC::Message* reply_message);

  void GoForwardBlockUntilNavigationsComplete(int handle,
                                              int number_of_navigations,
                                              IPC::Message* reply_message);

  void SavePackageShouldPromptUser(bool should_prompt);

  void GetWindowTitle(int handle, string16* text);

  void SetShelfVisibility(int handle, bool visible);

  // Returns the number of blocked popups in the tab |handle|.
  void GetBlockedPopupCount(int handle, int* count);

  // Generic pattern for pyautolib
  // Uses the JSON interface for input/output.
  void SendJSONRequest(int handle,
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

    // Set window dimensions.
  // Uses the JSON interface for input/output.
  void SetWindowDimensions(Browser* browser,
                           base::DictionaryValue* args,
                           IPC::Message* reply_message);

  // Get info about infobars in the given TabContents object.
  // This includes info about the type of infobars, the message text,
  // buttons, etc.
  // Caller owns the returned object.
  ListValue* GetInfobarsInfo(TabContents* tc);

  // Perform actions on an infobar like dismiss, accept, cancel.
  // Uses the JSON interface for input/output.
  void PerformActionOnInfobar(Browser* browser,
                              base::DictionaryValue* args,
                              IPC::Message* reply_message);

  // Create a new profile and open a new browser window with this profile. Uses
  // the JSON interface for input/output.
  void OpenNewBrowserWindowWithNewProfile(
      base::DictionaryValue* args,
      IPC::Message* reply_message);

  // Get info about multi-profile users.
  // Uses the JSON interface for input/output.
  void GetMultiProfileInfo(
      base::DictionaryValue* args,
      IPC::Message* reply_message);

  // Get info about the chromium/chrome in use.
  // This includes things like version, executable name, executable path.
  // Uses the JSON interface for input/output.
  void GetBrowserInfo(base::DictionaryValue* args,
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

  // Add an item to the history service.
  // Uses the JSON interface for input/output.
  void AddHistoryItem(Browser* browser,
                      base::DictionaryValue* args,
                      IPC::Message* reply_message);

  // Invoke loading of template url model.
  // Uses the JSON interface for input/output.
  void LoadSearchEngineInfo(Browser* browser,
                            base::DictionaryValue* args,
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
  void GetPrefsInfo(Browser* browser,
                    base::DictionaryValue* args,
                    IPC::Message* reply_message);

  // Set prefs.
  // Uses the JSON interface for input/output.
  void SetPrefs(Browser* browser,
                base::DictionaryValue* args,
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

  // Generate dictionary info about instant tab.
  // Uses the JSON interface for input/output.
  void GetInstantInfo(Browser* browser,
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

  // Clear the specified browsing data. This call provides similar
  // functionality to RemoveBrowsingData but is synchronous.
  // Uses the JSON interface for input/output.
  void ClearBrowsingData(Browser* browser,
                         base::DictionaryValue* args,
                         IPC::Message* reply_message);

  // Get info about blocked popups in a tab.
  // Uses the JSON interface for input/output.
  void GetBlockedPopupsInfo(Browser* browser,
                            base::DictionaryValue* args,
                            IPC::Message* reply_message);

  // Launch a blocked popup.
  // Uses the JSON interface for input/output.
  void UnblockAndLaunchBlockedPopup(Browser* browser,
                                    base::DictionaryValue* args,
                                    IPC::Message* reply_message);

  // Get info about theme.
  // Uses the JSON interface for input/output.
  void GetThemeInfo(Browser* browser,
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

  // Responds to the Find request and returns the match count.
  void FindInPage(Browser* browser,
                  base::DictionaryValue* args,
                  IPC::Message* reply_message);

  // Returns information about translation for a given tab. Includes
  // information about the translate bar if it is showing.
  void GetTranslateInfo(Browser* browser,
                        base::DictionaryValue* args,
                        IPC::Message* reply_message);

  // Takes the specified action on the translate bar.
  // Uses the JSON interface for input/output.
  void SelectTranslateOption(Browser* browser,
                             base::DictionaryValue* args,
                             IPC::Message* reply_message);

  // Get the profiles that are currently saved to the DB.
  // Uses the JSON interface for input/output.
  void GetAutofillProfile(Browser* browser,
                          base::DictionaryValue* args,
                          IPC::Message* reply_message);

  // Fill in an AutofillProfile with the given profile information.
  // Uses the JSON interface for input/output.
  void FillAutofillProfile(Browser* browser,
                           base::DictionaryValue* args,
                           IPC::Message* reply_message);

  // Injects Javascript into a specified frame that is assumed to submit
  // Autofill data via a webpage form, then waits for Autofill's personal data
  // manager to finish processing the data.
  void SubmitAutofillForm(Browser* browser,
                          base::DictionaryValue* args,
                          IPC::Message* reply_message);

  // Causes the autofill popup to be displayed in an already-focused webpage
  // form field.  Waits until the popup is displayed before returning.
  void AutofillTriggerSuggestions(Browser* browser,
                                  base::DictionaryValue* args,
                                  IPC::Message* message);

  // Highlights the previous or next autofill entry in an already-displayed
  // autofill popup.  This is done by sending either an "up arrow" or
  // "down arrow" keypress, then waiting for a preview of the filled-in state
  // to be displayed in the webpage form before returning.  Use
  // AutofillTriggerSuggestions() to cause the autofill popup to be displayed.
  void AutofillHighlightSuggestion(Browser* browser,
                                   base::DictionaryValue* args,
                                   IPC::Message* message);

  // Causes a webpage form to be filled with autofill information from an
  // autofill profile that is already highlighted in an autofill popup.  Use
  // AutofillHighlightSuggestion() as needed to highlight the desired profile
  // in the autofill popup.
  void AutofillAcceptSelection(Browser* browser,
                               base::DictionaryValue* args,
                               IPC::Message* message);

  // Signs in to sync using the given username and password.
  // Uses the JSON interface for input/output.
  void SignInToSync(Browser* browser,
                    base::DictionaryValue* args,
                    IPC::Message* reply_message);

  // Returns info about sync.
  // Uses the JSON interface for input/output.
  void GetSyncInfo(Browser* browser,
                   base::DictionaryValue* args,
                   IPC::Message* reply_message);

  // Waits for the ongoing sync cycle to complete.
  // Uses the JSON interface for input/output.
  void AwaitFullSyncCompletion(Browser* browser,
                               base::DictionaryValue* args,
                               IPC::Message* reply_message);

  // Waits for sync to reinitialize (for example, after a browser restart).
  // Uses the JSON interface for input/output.
  void AwaitSyncRestart(Browser* browser,
                        base::DictionaryValue* args,
                        IPC::Message* reply_message);

  // Enables sync for one or more sync datatypes.
  // Uses the JSON interface for input/output.
  void EnableSyncForDatatypes(Browser* browser,
                              base::DictionaryValue* args,
                              IPC::Message* reply_message);

  // Disables sync for one or more sync datatypes.
  // Uses the JSON interface for input/output.
  void DisableSyncForDatatypes(Browser* browser,
                               base::DictionaryValue* args,
                               IPC::Message* reply_message);

  // Translate DictionaryValues of autofill profiles and credit cards to the
  // data structure used in the browser.
  // Args:
  //   profiles/cards: the ListValue of profiles/credit cards to translate.
  //   error_message: a pointer to the return string in case of error.
  static std::vector<AutofillProfile> GetAutofillProfilesFromList(
      const base::ListValue& profiles, std::string* error_message);
  static std::vector<CreditCard> GetCreditCardsFromList(
      const base::ListValue& cards, std::string* error_message);

  // The opposite of the above: translates from the internal data structure
  // for profiles and credit cards to a ListValue of DictionaryValues. The
  // caller owns the returned object.
  static base::ListValue* GetListFromAutofillProfiles(
      const std::vector<AutofillProfile*>& autofill_profiles);
  static base::ListValue* GetListFromCreditCards(
      const std::vector<CreditCard*>& credit_cards);

  // Return the map from the internal data representation to the string value
  // of auto fill fields and credit card fields.
  static std::map<AutofillFieldType, std::string>
      GetAutofillFieldToStringMap();
  static std::map<AutofillFieldType, std::string>
      GetCreditCardFieldToStringMap();

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

  // Moves a thumbnail in the NTP's Most Visited sites section to a different
  // index.
  // Uses the JSON interface for input/output.
  void MoveNTPMostVisitedThumbnail(Browser* browser,
                                   base::DictionaryValue* args,
                                   IPC::Message* reply_message);

  // Removes a thumbnail from the NTP's Most Visited sites section.
  // Uses the JSON interface for input/output.
  void RemoveNTPMostVisitedThumbnail(Browser* browser,
                                     base::DictionaryValue* args,
                                     IPC::Message* reply_message);

  // Unpins a thumbnail in the NTP's Most Visited sites section.
  // Uses the JSON interface for input/output.
  void UnpinNTPMostVisitedThumbnail(Browser* browser,
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
                                NativeWebKeyboardEvent* event);

  // Populates the fields of the event parameter with default data, except for
  // the specified key type and key code.
  void BuildSimpleWebKeyEvent(WebKit::WebInputEvent::Type type,
                              int windows_key_code,
                              NativeWebKeyboardEvent* event);

  // Sends a key press event using the given key code to the specified tab.
  // A key press is a combination of a "key down" event and a "key up" event.
  // This function does not wait before returning.
  void SendWebKeyPressEventAsync(int key_code,
                                 TabContents* tab_contents);

  // Launches the specified app from the currently-selected tab.
  void LaunchApp(Browser* browser,
                 base::DictionaryValue* args,
                 IPC::Message* reply_message);

  // Sets the launch type for the specified app.
  void SetAppLaunchType(Browser* browser,
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

  // Sends the key event from the OS level to the browser window,
  // allowing it to be preprocessed by some external application (ie. IME).
  // Will switch to the tab specified by tab_index before sending the event.
  // The pair |windex| and |tab_index| or the single |auto_id| must be given
  // to specify the tab.
  // Example:
  //   input: { "windex": 1,
  //            "tab_index": 1,
  //            "auto_id": { "type": 0, "id": "awoein" },
  //            "keyCode": ui::VKEY_X,
  //            "modifiers": automation::kShiftKeyMask,
  //          }
  //   output: none
  void SendOSLevelKeyEventToTab(base::DictionaryValue* args,
                                IPC::Message* message);

  // Method used as a Task that sends a success AutomationJSONReply.
  void SendSuccessReply(IPC::Message* reply_message);

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

  // Gets the version of ChromeDriver automation supported by this server.
  // Example:
  //   input: none
  //   output: { "version": 1 }
  void GetChromeDriverAutomationVersion(base::DictionaryValue* args,
                                        IPC::Message* message);

  // Auto-updates installed extensions.
  // Uses the JSON interface for input/output.
  void UpdateExtensionsNow(base::DictionaryValue* args,
                           IPC::Message* reply_message);

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

  // Enforces the policies in |args|. The policies are provided as a dictionary
  // of policy names to their values. The top-level dictionary maps the policy
  // provider level to its dictionary.
  // Example:
  //   input: { "managed_platform": {
  //                                  "ShowHomeButton": true,
  //                                  "SafeBrowsingEnabled": false,
  //                                },
  //            "recommended_platform": {
  //                                      ...
  //                                    },
  //            "managed_cloud": {
  //                               ...
  //                             },
  //            "recommended_cloud": {
  //                                   ...
  //                                 }
  //          }
  //  output: none
  void SetPolicies(base::DictionaryValue* args,
                   IPC::Message* reply_message);

  // Gets a list of supported policies. The output is a map of policy name to
  // its value type.
  // Example:
  //   input: none
  //   output: { "ShowHomeButton": "bool",
  //             "DefaultSearchProviderSearchURL": "str"
  //           }
  void GetPolicyDefinitionList(base::DictionaryValue* args,
                               IPC::Message* reply_message);

  // Triggers a policy update on the platform and cloud providers, if they
  // exist. Returns after the update notifications are received.
  // Example:
  //   input: none
  //   output: none
  void RefreshPolicies(base::DictionaryValue* args,
                       IPC::Message* reply_message);

#if defined(OS_CHROMEOS)
  // Login.
  void GetLoginInfo(base::DictionaryValue* args, IPC::Message* reply_message);

  void ShowCreateAccountUI(base::DictionaryValue* args,
                           IPC::Message* reply_message);

  void LoginAsGuest(base::DictionaryValue* args, IPC::Message* reply_message);

  void Login(base::DictionaryValue* args, IPC::Message* reply_message);

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

  void GetProxySettings(Browser* browser,
                        base::DictionaryValue* args,
                        IPC::Message* reply_message);

  void SetProxySettings(Browser* browser,
                        base::DictionaryValue* args,
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

  // Enterprise policy.
  void IsEnterpriseDevice(DictionaryValue* args, IPC::Message* reply_message);

  void EnrollEnterpriseDevice(DictionaryValue* args,
                              IPC::Message* reply_message);

  void GetEnterprisePolicyInfo(DictionaryValue* args,
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

  // chromeos::PowerManagerClient::Observer overrides.
  virtual void PowerChanged(const chromeos::PowerSupplyStatus& status) OVERRIDE;

#endif  // defined(OS_CHROMEOS)

  void WaitForTabCountToBecome(int browser_handle,
                               int target_tab_count,
                               IPC::Message* reply_message);

  void WaitForInfoBarCount(int tab_handle,
                           size_t target_count,
                           IPC::Message* reply_message);

  // Gets the current used encoding name of the page in the specified tab.
  void GetPageCurrentEncoding(int tab_handle, std::string* current_encoding);

  void ShutdownSessionService(int handle, bool* result);

  void SetContentSetting(int handle,
                         const std::string& host,
                         ContentSettingsType content_type,
                         ContentSetting setting,
                         bool* success);

  // Load all plug-ins on the page.
  void LoadBlockedPlugins(int tab_handle, bool* success);

  // Resets to the default theme.
  void ResetToDefaultTheme();

  void WaitForProcessLauncherThreadToGoIdle(IPC::Message* reply_message);

  // Gets the browser that contains the given tab.
  void GetParentBrowserOfTab(
      int tab_handle, int* browser_handle, bool* success);

  // Callback for history redirect queries.
  virtual void OnRedirectQueryComplete(
      HistoryService::Handle request_handle,
      GURL from_url,
      bool success,
      history::RedirectList* redirects);

  void OnRemoveProvider();  // Called via PostTask

#if defined(TOOLKIT_VIEWS)
  // Keep track of whether a popup menu has been opened since the last time
  // that StartTrackingPopupMenus has been called.
  bool popup_menu_opened_;

  // A temporary object that receives a notification when a popup menu opens.
  PopupMenuWaiter* popup_menu_waiter_;
#endif  // defined(TOOLKIT_VIEWS)

  // Used to wait on various browser sync events.
  scoped_ptr<ProfileSyncServiceHarness> sync_waiter_;

  // Handle for an in-process redirect query. We expect only one redirect query
  // at a time (we should have only one caller, and it will block while waiting
  // for the results) so there is only one handle. When non-0, indicates a
  // query in progress.
  HistoryService::Handle redirect_query_;

  content::NotificationRegistrar registrar_;

  // Used to enumerate browser profiles.
  scoped_refptr<ImporterList> importer_list_;

  // The stored data for the ImportSettings operation.
  ImportSettingsData import_settings_data_;

  DISALLOW_COPY_AND_ASSIGN(TestingAutomationProvider);
};

#endif  // CHROME_BROWSER_AUTOMATION_TESTING_AUTOMATION_PROVIDER_H_
