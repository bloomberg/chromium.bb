// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOMATION_TESTING_AUTOMATION_PROVIDER_H_
#define CHROME_BROWSER_AUTOMATION_TESTING_AUTOMATION_PROVIDER_H_
#pragma once

#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/automation/automation_provider.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/history/history.h"
#include "chrome/browser/importer/importer_list.h"
#include "chrome/browser/sync/profile_sync_service_harness.h"
#include "chrome/common/notification_registrar.h"
#include "chrome/common/page_type.h"

class DictionaryValue;
class ImporterHost;
class TemplateURLModel;

// This is an automation provider containing testing calls.
class TestingAutomationProvider : public AutomationProvider,
                                  public BrowserList::Observer,
                                  public ImporterList::Observer,
                                  public NotificationObserver {
 public:
  explicit TestingAutomationProvider(Profile* profile);

  // BrowserList::Observer implementation.
  virtual void OnBrowserAdded(const Browser* browser);
  virtual void OnBrowserRemoved(const Browser* browser);

  // IPC::Channel::Listener implementation.
  virtual bool OnMessageReceived(const IPC::Message& msg);
  virtual void OnChannelError();

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

  // ImporterList::Observer implementation.
  virtual void SourceProfilesLoaded();

  // NotificationObserver implementation.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

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
                 const std::string value,
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
  // Be aware that the browser window returned might be of non TYPE_NORMAL
  // or in incognito mode.
  void GetBrowserWindow(int index, int* handle);
  void FindNormalBrowserWindow(int* handle);
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
  void GetTab(int win_handle, int tab_index, int* tab_handle);
  void GetTabProcessID(int handle, int* process_id);
  void GetTabTitle(int handle, int* title_string_size, std::wstring* title);
  void GetTabIndex(int handle, int* tabstrip_index);
  void GetTabURL(int handle, bool* success, GURL* url);
  void GetShelfVisibility(int handle, bool* visible);
  void IsFullscreen(int handle, bool* is_fullscreen);
  void GetFullscreenBubbleVisibility(int handle, bool* is_visible);
  void GetAutocompleteEditForBrowser(int browser_handle, bool* success,
                                     int* autocomplete_edit_handle);

  // Retrieves the visible text from the autocomplete edit.
  void GetAutocompleteEditText(int autocomplete_edit_handle,
                               bool* success, string16* text);

  // Sets the visible text from the autocomplete edit.
  void SetAutocompleteEditText(int autocomplete_edit_handle,
                               const string16& text,
                               bool* success);

  // Retrieves if a query to an autocomplete provider is in progress.
  void AutocompleteEditIsQueryInProgress(int autocomplete_edit_handle,
                                         bool* success,
                                         bool* query_in_progress);

  // Retrieves the individual autocomplete matches displayed by the popup.
  void AutocompleteEditGetMatches(int autocomplete_edit_handle,
                                  bool* success,
                                  std::vector<AutocompleteMatchData>* matches);

  // Waits for the autocomplete edit to receive focus
  void WaitForAutocompleteEditFocus(int autocomplete_edit_handle,
                                    IPC::Message* reply_message);

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
  void GetSecurityState(int handle, bool* success,
                        SecurityStyle* security_style, int* ssl_cert_status,
                        int* insecure_content_status);

  // Gets the page type for the tab associated to the specified |handle|.
  void GetPageType(int handle, bool* success, PageType* page_type);

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
  void GetBookmarkBarVisibility(int handle, bool* visible, bool* animating);

  // Get the bookmarks as a JSON string.
  void GetBookmarksAsJSON(int handle, std::string* bookmarks_as_json,
                          bool *success);

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
  void GetInfoBarCount(int handle, int* count);

  // Causes a click on the "accept" button of the info-bar at |info_bar_index|.
  // If |wait_for_navigation| is true, it sends the reply after a navigation has
  // occurred.
  void ClickInfoBarAccept(int handle,
                          int info_bar_index,
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

  // Captures the entire page for the given tab and saves it as PNG at the
  // given path.
  void CaptureEntirePageAsPNG(int tab_handle, const FilePath& path,
                              IPC::Message* reply_message);

  // Generic pattern for pyautolib
  // Uses the JSON interface for input/output.
  void SendJSONRequest(int handle,
                       std::string json_request,
                       IPC::Message* reply_message);

  // Method ptr for json handlers.
  // Uses the JSON interface for input/output.
  typedef void (TestingAutomationProvider::*JsonHandler)(
      Browser* browser,
      DictionaryValue*,
      IPC::Message*);

    // Set window dimensions.
  // Uses the JSON interface for input/output.
  void SetWindowDimensions(Browser* browser,
                           DictionaryValue* args,
                           IPC::Message* reply_message);

  // Get info about infobars in the given TabContents object.
  // This includes info about the type of infobars, the message text,
  // buttons, etc.
  // Caller owns the returned object.
  ListValue* GetInfobarsInfo(TabContents* tc);

  // Perform actions on an infobar like dismiss, accept, cancel.
  // Uses the JSON interface for input/output.
  void PerformActionOnInfobar(Browser* browser,
                              DictionaryValue* args,
                              IPC::Message* reply_message);

  // Get info about the chromium/chrome in use.
  // This includes things like version, executable name, executable path.
  // Uses the JSON interface for input/output.
  void GetBrowserInfo(Browser* browser,
                      DictionaryValue* args,
                      IPC::Message* reply_message);

  // Get info about the state of navigation in a given tab.
  // This includes ssl info.
  // Uses the JSON interface for input/output.
  void GetNavigationInfo(Browser* browser,
                         DictionaryValue* args,
                         IPC::Message* reply_message);

  // Get info about downloads. This includes only ones that have been
  // registered by the history system.
  // Uses the JSON interface for input/output.
  void GetDownloadsInfo(Browser* browser,
                        DictionaryValue* args,
                        IPC::Message* reply_message);

  // Wait for all downloads to complete.
  // Uses the JSON interface for input/output.
  void WaitForDownloadsToComplete(Browser* browser,
                                  DictionaryValue* args,
                                  IPC::Message* reply_message);

  // Performs the given action on the specified download.
  // Uses the JSON interface for input/output.
  void PerformActionOnDownload(Browser* browser,
                               DictionaryValue* args,
                               IPC::Message* reply_message);

  // Get info about history.
  // Uses the JSON interface for input/output.
  void GetHistoryInfo(Browser* browser,
                      DictionaryValue* args,
                      IPC::Message* reply_message);

  // Add an item to the history service.
  // Uses the JSON interface for input/output.
  void AddHistoryItem(Browser* browser,
                      DictionaryValue* args,
                      IPC::Message* reply_message);

  // Invoke loading of template url model.
  // Uses the JSON interface for input/output.
  void LoadSearchEngineInfo(Browser* browser,
                            DictionaryValue* args,
                            IPC::Message* reply_message);

  // Get search engines list.
  // Assumes that the profile's template url model is loaded.
  // Uses the JSON interface for input/output.
  void GetSearchEngineInfo(Browser* browser,
                           DictionaryValue* args,
                           IPC::Message* reply_message);

  // Add or edit search engine.
  // Assumes that the profile's template url model is loaded.
  // Uses the JSON interface for input/output.
  void AddOrEditSearchEngine(Browser* browser,
                             DictionaryValue* args,
                             IPC::Message* reply_message);

  // Perform a given action on an existing search engine.
  // Assumes that the profile's template url model is loaded.
  // Uses the JSON interface for input/output.
  void PerformActionOnSearchEngine(Browser* browser,
                                   DictionaryValue* args,
                                   IPC::Message* reply_message);

  // Get info about preferences.
  // Uses the JSON interface for input/output.
  void GetPrefsInfo(Browser* browser,
                    DictionaryValue* args,
                    IPC::Message* reply_message);

  // Set prefs.
  // Uses the JSON interface for input/output.
  void SetPrefs(Browser* browser,
                DictionaryValue* args,
                IPC::Message* reply_message);

  // Return load times of initial tabs.
  // Uses the JSON interface for input/output.
  // Only includes tabs from command line arguments or session restore.
  // See declaration of InitialLoadObserver in automation_provider_observers.h
  // for example response.
  void GetInitialLoadTimes(Browser* browser,
                           DictionaryValue* args,
                           IPC::Message* reply_message);

  // Get info about plugins.
  // Uses the JSON interface for input/output.
  void GetPluginsInfo(Browser* browser,
                      DictionaryValue* args,
                      IPC::Message* reply_message);

  // Enable a plugin.
  // Uses the JSON interface for input/output.
  void EnablePlugin(Browser* browser,
                    DictionaryValue* args,
                    IPC::Message* reply_message);

  // Disable a plugin.
  // Uses the JSON interface for input/output.
  void DisablePlugin(Browser* browser,
                     DictionaryValue* args,
                     IPC::Message* reply_message);

  // Get info about omnibox.
  // Contains data about the matches (url, content, description)
  // in the omnibox popup, the text in the omnibox.
  // Uses the JSON interface for input/output.
  void GetOmniboxInfo(Browser* browser,
                      DictionaryValue* args,
                      IPC::Message* reply_message);

  // Set text in the omnibox. This sets focus to the omnibox.
  // Uses the JSON interface for input/output.
  void SetOmniboxText(Browser* browser,
                      DictionaryValue* args,
                      IPC::Message* reply_message);

  // Move omnibox popup selection up or down.
  // Uses the JSON interface for input/output.
  void OmniboxMovePopupSelection(Browser* browser,
                                 DictionaryValue* args,
                                 IPC::Message* reply_message);

  // Accept the current string of text in the omnibox.
  // This is equivalent to clicking or hiting enter on a popup selection.
  // Blocks until the page loads.
  // Uses the JSON interface for input/output.
  void OmniboxAcceptInput(Browser* browser,
                          DictionaryValue* args,
                          IPC::Message* reply_message);

  // Generate dictionary info about instant tab.
  // Uses the JSON interface for input/output.
  void GetInstantInfo(Browser* browser,
                      DictionaryValue* args,
                      IPC::Message* reply_message);

  // Save the contents of a tab into a file.
  // Uses the JSON interface for input/output.
  void SaveTabContents(Browser* browser,
                       DictionaryValue* args,
                       IPC::Message* reply_message);

  // Import the given settings from the given browser.
  // Uses the JSON interface for input/output.
  void ImportSettings(Browser* browser,
                      DictionaryValue* args,
                      IPC::Message* reply_message);

  // Add a new entry to the password store based on the password information
  // provided. This method can also be used to add a blacklisted site (which
  // will never fill in the password).
  // Uses the JSON interface for input/output.
  void AddSavedPassword(Browser* browser,
                        DictionaryValue* args,
                        IPC::Message* reply_message);

  // Removes the password matching the information provided. This method can
  // also be used to remove a blacklisted site.
  // Uses the JSON interface for input/output.
  void RemoveSavedPassword(Browser* browser,
                           DictionaryValue* args,
                           IPC::Message* reply_message);

  // Return the saved username/password combinations.
  // Uses the JSON interface for input/output.
  void GetSavedPasswords(Browser* browser,
                         DictionaryValue* args,
                         IPC::Message* reply_message);

  // Clear the specified browsing data. This call provides similar
  // functionality to RemoveBrowsingData but is synchronous.
  // Uses the JSON interface for input/output.
  void ClearBrowsingData(Browser* browser,
                         DictionaryValue* args,
                         IPC::Message* reply_message);

  // Get info about blocked popups in a tab.
  // Uses the JSON interface for input/output.
  void GetBlockedPopupsInfo(Browser* browser,
                            DictionaryValue* args,
                            IPC::Message* reply_message);

  // Launch a blocked popup.
  // Uses the JSON interface for input/output.
  void UnblockAndLaunchBlockedPopup(Browser* browser,
                                    DictionaryValue* args,
                                    IPC::Message* reply_message);

  // Get info about theme.
  // Uses the JSON interface for input/output.
  void GetThemeInfo(Browser* browser,
                    DictionaryValue* args,
                    IPC::Message* reply_message);

  // Get info about all intalled extensions.
  // Uses the JSON interface for input/output.
  void GetExtensionsInfo(Browser* browser,
                         DictionaryValue* args,
                         IPC::Message* reply_message);

  // Uninstalls the extension with the given id.
  // Uses the JSON interface for input/output.
  void UninstallExtensionById(Browser* browser,
                              DictionaryValue* args,
                              IPC::Message* reply_message);

  // Responds to the Find request and returns the match count.
  void FindInPage(Browser* browser,
                  DictionaryValue* args,
                  IPC::Message* reply_message);

  // Returns information about translation for a given tab. Includes
  // information about the translate bar if it is showing.
  void GetTranslateInfo(Browser* browser,
                        DictionaryValue* args,
                        IPC::Message* reply_message);

  // Takes the specified action on the translate bar.
  // Uses the JSON interface for input/output.
  void SelectTranslateOption(Browser* browser,
                             DictionaryValue* args,
                             IPC::Message* reply_message);

  // Get the profiles that are currently saved to the DB.
  // Uses the JSON interface for input/output.
  void GetAutoFillProfile(Browser* browser,
                          DictionaryValue* args,
                          IPC::Message* reply_message);

  // Fill in an AutoFillProfile with the given profile information.
  // Uses the JSON interface for input/output.
  void FillAutoFillProfile(Browser* browser,
                           DictionaryValue* args,
                           IPC::Message* reply_message);

  // Signs in to sync using the given username and password.
  // Uses the JSON interface for input/output.
  void SignInToSync(Browser* browser,
                    DictionaryValue* args,
                    IPC::Message* reply_message);

  // Returns info about sync.
  // Uses the JSON interface for input/output.
  void GetSyncInfo(Browser* browser,
                   DictionaryValue* args,
                   IPC::Message* reply_message);

  // Waits for the ongoing sync cycle to complete.
  // Uses the JSON interface for input/output.
  void AwaitSyncCycleCompletion(Browser* browser,
                                DictionaryValue* args,
                                IPC::Message* reply_message);

  // Enables sync for one or more sync datatypes.
  // Uses the JSON interface for input/output.
  void EnableSyncForDatatypes(Browser* browser,
                              DictionaryValue* args,
                              IPC::Message* reply_message);

  // Disables sync for one or more sync datatypes.
  // Uses the JSON interface for input/output.
  void DisableSyncForDatatypes(Browser* browser,
                               DictionaryValue* args,
                               IPC::Message* reply_message);

  // Translate DictionaryValues of autofill profiles and credit cards to the
  // data structure used in the browser.
  // Args:
  //   profiles/cards: the ListValue of profiles/credit cards to translate.
  //   error_message: a pointer to the return string in case of error.
  static std::vector<AutoFillProfile> GetAutoFillProfilesFromList(
      const ListValue& profiles, std::string* error_message);
  static std::vector<CreditCard> GetCreditCardsFromList(
      const ListValue& cards, std::string* error_message);

  // The opposite of the above: translates from the internal data structure
  // for profiles and credit cards to a ListValue of DictionaryValues. The
  // caller owns the returned object.
  static ListValue* GetListFromAutoFillProfiles(
      const std::vector<AutoFillProfile*>& autofill_profiles);
  static ListValue* GetListFromCreditCards(
      const std::vector<CreditCard*>& credit_cards);

  // Return the map from the internal data representation to the string value
  // of auto fill fields and credit card fields.
  static std::map<AutoFillFieldType, std::string>
      GetAutoFillFieldToStringMap();
  static std::map<AutoFillFieldType, std::string>
      GetCreditCardFieldToStringMap();

  // Get a list of active HTML5 notifications.
  // Uses the JSON interface for input/output.
  void GetActiveNotifications(Browser* browser,
                              DictionaryValue* args,
                              IPC::Message* reply_message);

  // Close an active HTML5 notification.
  // Uses the JSON interface for input/output.
  void CloseNotification(Browser* browser,
                         DictionaryValue* args,
                         IPC::Message* reply_message);

  // Waits for the number of active HTML5 notifications to reach a given count.
  // Uses the JSON interface for input/output.
  void WaitForNotificationCount(Browser* browser,
                                DictionaryValue* args,
                                IPC::Message* reply_message);

  // Gets info about the elements in the NTP.
  // Uses the JSON interface for input/output.
  void GetNTPInfo(Browser* browser,
                  DictionaryValue* args,
                  IPC::Message* reply_message);

  // Moves a thumbnail in the NTP's Most Visited sites section to a different
  // index.
  // Uses the JSON interface for input/output.
  void MoveNTPMostVisitedThumbnail(Browser* browser,
                                   DictionaryValue* args,
                                   IPC::Message* reply_message);

  // Removes a thumbnail from the NTP's Most Visited sites section.
  // Uses the JSON interface for input/output.
  void RemoveNTPMostVisitedThumbnail(Browser* browser,
                                     DictionaryValue* args,
                                     IPC::Message* reply_message);

  // Unpins a thumbnail in the NTP's Most Visited sites section.
  // Uses the JSON interface for input/output.
  void UnpinNTPMostVisitedThumbnail(Browser* browser,
                                    DictionaryValue* args,
                                    IPC::Message* reply_message);

  // Restores all thumbnails that have been removed (i.e., blacklisted) from the
  // NTP's Most Visited sites section.
  // Uses the JSON interface for input/output.
  void RestoreAllNTPMostVisitedThumbnails(Browser* browser,
                                          DictionaryValue* args,
                                          IPC::Message* reply_message);

  // Kills the given renderer process and returns after the associated
  // RenderProcessHost receives notification of its closing.
  void KillRendererProcess(Browser* browser,
                           DictionaryValue* args,
                           IPC::Message* reply_message);

  void WaitForTabCountToBecome(int browser_handle,
                               int target_tab_count,
                               IPC::Message* reply_message);

  void WaitForInfoBarCount(int tab_handle,
                           int target_count,
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

  NotificationRegistrar registrar_;

  // Used to import settings from browser profiles.
  scoped_refptr<ImporterHost> importer_host_;

  // The stored data for the ImportSettings operation.
  ImportSettingsData import_settings_data_;

  DISALLOW_COPY_AND_ASSIGN(TestingAutomationProvider);
};

#endif  // CHROME_BROWSER_AUTOMATION_TESTING_AUTOMATION_PROVIDER_H_
