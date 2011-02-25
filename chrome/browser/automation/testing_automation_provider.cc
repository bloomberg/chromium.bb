// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/automation/testing_automation_provider.h"

#include <map>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/json/string_escape.h"
#include "base/path_service.h"
#include "base/process.h"
#include "base/process_util.h"
#include "base/stringprintf.h"
#include "base/threading/thread_restrictions.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/autocomplete/autocomplete.h"
#include "chrome/browser/autocomplete/autocomplete_edit.h"
#include "chrome/browser/autocomplete/autocomplete_match.h"
#include "chrome/browser/autofill/autofill_manager.h"
#include "chrome/browser/automation/automation_autocomplete_edit_tracker.h"
#include "chrome/browser/automation/automation_browser_tracker.h"
#include "chrome/browser/automation/automation_provider_json.h"
#include "chrome/browser/automation/automation_provider_list.h"
#include "chrome/browser/automation/automation_provider_observers.h"
#include "chrome/browser/automation/automation_tab_tracker.h"
#include "chrome/browser/automation/automation_window_tracker.h"
#include "chrome/browser/automation/ui_controls.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/bookmarks/bookmark_storage.h"
#include "chrome/browser/blocked_content_container.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_shutdown.h"
#include "chrome/browser/browser_window.h"
#include "chrome/browser/debugger/devtools_manager.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/download/download_shelf.h"
#include "chrome/browser/extensions/extension_host.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/instant/instant_controller.h"
#include "chrome/browser/history/top_sites.h"
#include "chrome/browser/importer/importer.h"
#include "chrome/browser/notifications/balloon.h"
#include "chrome/browser/notifications/balloon_collection.h"
#include "chrome/browser/notifications/notification.h"
#include "chrome/browser/notifications/notification_ui_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/renderer_host/render_process_host.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/browser/search_engines/template_url_model.h"
#include "chrome/browser/tab_contents/infobar_delegate.h"
#include "chrome/browser/tab_contents/interstitial_page.h"
#include "chrome/browser/translate/translate_infobar_delegate.h"
#include "chrome/browser/ui/app_modal_dialogs/app_modal_dialog.h"
#include "chrome/browser/ui/app_modal_dialogs/app_modal_dialog_queue.h"
#include "chrome/browser/ui/app_modal_dialogs/native_app_modal_dialog.h"
#include "chrome/browser/ui/find_bar/find_bar.h"
#include "chrome/browser/ui/login/login_prompt.h"
#include "chrome/browser/ui/omnibox/location_bar.h"
#include "chrome/browser/ui/search_engines/keyword_editor_controller.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/net/url_request_context_getter.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/automation_messages.h"
#include "net/base/cookie_store.h"
#include "net/url_request/url_request_context.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebInputEvent.h"
#include "ui/base/events.h"
#include "ui/base/message_box_flags.h"
#include "webkit/plugins/npapi/plugin_list.h"

namespace {

void GetCookiesOnIOThread(
    const GURL& url,
    const scoped_refptr<URLRequestContextGetter>& context_getter,
    base::WaitableEvent* event,
    std::string* cookies) {
  *cookies = context_getter->GetCookieStore()->GetCookies(url);
  event->Signal();
}

void SetCookieOnIOThread(
    const GURL& url,
    const std::string& value,
    const scoped_refptr<URLRequestContextGetter>& context_getter,
    base::WaitableEvent* event,
    bool* success) {
  *success = context_getter->GetCookieStore()->SetCookie(url, value);
  event->Signal();
}

void DeleteCookieOnIOThread(
    const GURL& url,
    const std::string& name,
    const scoped_refptr<URLRequestContextGetter>& context_getter,
    base::WaitableEvent* event) {
  context_getter->GetCookieStore()->DeleteCookie(url, name);
  event->Signal();
}

void SendMouseClick(int flags) {
  ui_controls::MouseButton button = ui_controls::LEFT;
  if ((flags & ui::EF_LEFT_BUTTON_DOWN) ==
      ui::EF_LEFT_BUTTON_DOWN) {
    button = ui_controls::LEFT;
  } else if ((flags & ui::EF_RIGHT_BUTTON_DOWN) ==
             ui::EF_RIGHT_BUTTON_DOWN) {
    button = ui_controls::RIGHT;
  } else if ((flags & ui::EF_MIDDLE_BUTTON_DOWN) ==
             ui::EF_MIDDLE_BUTTON_DOWN) {
    button = ui_controls::MIDDLE;
  } else {
    NOTREACHED();
  }

  ui_controls::SendMouseClick(button);
}

class AutomationInterstitialPage : public InterstitialPage {
 public:
  AutomationInterstitialPage(TabContents* tab,
                             const GURL& url,
                             const std::string& contents)
      : InterstitialPage(tab, true, url),
        contents_(contents) {
  }

  virtual std::string GetHTMLContents() { return contents_; }

 private:
  const std::string contents_;

  DISALLOW_COPY_AND_ASSIGN(AutomationInterstitialPage);
};

}  // namespace

TestingAutomationProvider::TestingAutomationProvider(Profile* profile)
    : AutomationProvider(profile),
#if defined(TOOLKIT_VIEWS)
      popup_menu_waiter_(NULL),
#endif
      redirect_query_(0) {
  BrowserList::AddObserver(this);
  registrar_.Add(this, NotificationType::SESSION_END,
                 NotificationService::AllSources());
}

TestingAutomationProvider::~TestingAutomationProvider() {
  BrowserList::RemoveObserver(this);
}

void TestingAutomationProvider::SourceProfilesLoaded() {
  DCHECK_NE(static_cast<ImporterList*>(NULL), importer_list_.get());

  // Get the correct ProfileInfo based on the browser the user provided.
  importer::ProfileInfo profile_info;
  int num_browsers = importer_list_->GetAvailableProfileCount();
  int i = 0;
  for ( ; i < num_browsers; i++) {
    string16 name = WideToUTF16Hack(importer_list_->GetSourceProfileNameAt(i));
    if (name == import_settings_data_.browser_name) {
      profile_info = importer_list_->GetSourceProfileInfoAt(i);
      break;
    }
  }
  // If we made it to the end of the loop, then the input was bad.
  if (i == num_browsers) {
    AutomationJSONReply(this, import_settings_data_.reply_message)
        .SendError("Invalid browser name string found.");
    return;
  }

  scoped_refptr<ImporterHost> importer_host(new ImporterHost);
  importer_host->SetObserver(
      new AutomationProviderImportSettingsObserver(
          this, import_settings_data_.reply_message));

  Profile* profile = import_settings_data_.browser->profile();
  importer_host->StartImportSettings(profile_info,
                                     profile,
                                     import_settings_data_.import_items,
                                     new ProfileWriter(profile),
                                     import_settings_data_.first_run);
}

void TestingAutomationProvider::Observe(NotificationType type,
                                        const NotificationSource& source,
                                        const NotificationDetails& details) {
  DCHECK(type == NotificationType::SESSION_END);
  // OnBrowserRemoved does a ReleaseLater. When session end is received we exit
  // before the task runs resulting in this object not being deleted. This
  // Release balance out the Release scheduled by OnBrowserRemoved.
  Release();
}

bool TestingAutomationProvider::OnMessageReceived(
    const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(TestingAutomationProvider, message)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(AutomationMsg_CloseBrowser, CloseBrowser)
    IPC_MESSAGE_HANDLER(AutomationMsg_CloseBrowserRequestAsync,
                        CloseBrowserAsync)
    IPC_MESSAGE_HANDLER(AutomationMsg_ActivateTab, ActivateTab)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(AutomationMsg_AppendTab, AppendTab)
    IPC_MESSAGE_HANDLER(AutomationMsg_ActiveTabIndex, GetActiveTabIndex)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(AutomationMsg_CloseTab, CloseTab)
    IPC_MESSAGE_HANDLER(AutomationMsg_GetCookies, GetCookies)
    IPC_MESSAGE_HANDLER(AutomationMsg_SetCookie, SetCookie)
    IPC_MESSAGE_HANDLER(AutomationMsg_DeleteCookie, DeleteCookie)
    IPC_MESSAGE_HANDLER(AutomationMsg_ShowCollectedCookiesDialog,
                        ShowCollectedCookiesDialog)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(
        AutomationMsg_NavigateToURLBlockUntilNavigationsComplete,
        NavigateToURLBlockUntilNavigationsComplete)
    IPC_MESSAGE_HANDLER(AutomationMsg_NavigationAsync, NavigationAsync)
    IPC_MESSAGE_HANDLER(AutomationMsg_NavigationAsyncWithDisposition,
                        NavigationAsyncWithDisposition)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(AutomationMsg_Reload, Reload)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(AutomationMsg_SetAuth, SetAuth)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(AutomationMsg_CancelAuth, CancelAuth)
    IPC_MESSAGE_HANDLER(AutomationMsg_NeedsAuth, NeedsAuth)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(AutomationMsg_RedirectsFrom,
                                    GetRedirectsFrom)
    IPC_MESSAGE_HANDLER(AutomationMsg_BrowserWindowCount, GetBrowserWindowCount)
    IPC_MESSAGE_HANDLER(AutomationMsg_NormalBrowserWindowCount,
                        GetNormalBrowserWindowCount)
    IPC_MESSAGE_HANDLER(AutomationMsg_BrowserWindow, GetBrowserWindow)
    IPC_MESSAGE_HANDLER(AutomationMsg_GetBrowserLocale, GetBrowserLocale)
    IPC_MESSAGE_HANDLER(AutomationMsg_LastActiveBrowserWindow,
                        GetLastActiveBrowserWindow)
    IPC_MESSAGE_HANDLER(AutomationMsg_ActiveWindow, GetActiveWindow)
    IPC_MESSAGE_HANDLER(AutomationMsg_FindNormalBrowserWindow,
                        FindNormalBrowserWindow)
    IPC_MESSAGE_HANDLER(AutomationMsg_IsWindowActive, IsWindowActive)
    IPC_MESSAGE_HANDLER(AutomationMsg_ActivateWindow, ActivateWindow)
    IPC_MESSAGE_HANDLER(AutomationMsg_IsWindowMaximized, IsWindowMaximized)
    IPC_MESSAGE_HANDLER(AutomationMsg_WindowExecuteCommandAsync,
                        ExecuteBrowserCommandAsync)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(AutomationMsg_WindowExecuteCommand,
                        ExecuteBrowserCommand)
    IPC_MESSAGE_HANDLER(AutomationMsg_TerminateSession, TerminateSession)
    IPC_MESSAGE_HANDLER(AutomationMsg_WindowViewBounds, WindowGetViewBounds)
    IPC_MESSAGE_HANDLER(AutomationMsg_GetWindowBounds, GetWindowBounds)
    IPC_MESSAGE_HANDLER(AutomationMsg_SetWindowBounds, SetWindowBounds)
    IPC_MESSAGE_HANDLER(AutomationMsg_SetWindowVisible, SetWindowVisible)
    IPC_MESSAGE_HANDLER(AutomationMsg_WindowClick, WindowSimulateClick)
    IPC_MESSAGE_HANDLER(AutomationMsg_WindowMouseMove, WindowSimulateMouseMove)
    IPC_MESSAGE_HANDLER(AutomationMsg_WindowKeyPress, WindowSimulateKeyPress)
    IPC_MESSAGE_HANDLER(AutomationMsg_TabCount, GetTabCount)
    IPC_MESSAGE_HANDLER(AutomationMsg_Type, GetType)
    IPC_MESSAGE_HANDLER(AutomationMsg_Tab, GetTab)
    IPC_MESSAGE_HANDLER(AutomationMsg_TabProcessID, GetTabProcessID)
    IPC_MESSAGE_HANDLER(AutomationMsg_TabTitle, GetTabTitle)
    IPC_MESSAGE_HANDLER(AutomationMsg_TabIndex, GetTabIndex)
    IPC_MESSAGE_HANDLER(AutomationMsg_TabURL, GetTabURL)
    IPC_MESSAGE_HANDLER(AutomationMsg_ShelfVisibility, GetShelfVisibility)
    IPC_MESSAGE_HANDLER(AutomationMsg_IsFullscreen, IsFullscreen)
    IPC_MESSAGE_HANDLER(AutomationMsg_IsFullscreenBubbleVisible,
                        GetFullscreenBubbleVisibility)
    IPC_MESSAGE_HANDLER(AutomationMsg_AutocompleteEditForBrowser,
                        GetAutocompleteEditForBrowser)
    IPC_MESSAGE_HANDLER(AutomationMsg_AutocompleteEditGetText,
                        GetAutocompleteEditText)
    IPC_MESSAGE_HANDLER(AutomationMsg_AutocompleteEditSetText,
                        SetAutocompleteEditText)
    IPC_MESSAGE_HANDLER(AutomationMsg_AutocompleteEditIsQueryInProgress,
                        AutocompleteEditIsQueryInProgress)
    IPC_MESSAGE_HANDLER(AutomationMsg_AutocompleteEditGetMatches,
                        AutocompleteEditGetMatches)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(AutomationMsg_WaitForAutocompleteEditFocus,
                                    WaitForAutocompleteEditFocus)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(AutomationMsg_DomOperation,
                                    ExecuteJavascript)
    IPC_MESSAGE_HANDLER(AutomationMsg_ConstrainedWindowCount,
                        GetConstrainedWindowCount)
#if defined(TOOLKIT_VIEWS)
    IPC_MESSAGE_HANDLER(AutomationMsg_GetFocusedViewID, GetFocusedViewID)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(AutomationMsg_WaitForFocusedViewIDToChange,
                                    WaitForFocusedViewIDToChange)
    IPC_MESSAGE_HANDLER(AutomationMsg_StartTrackingPopupMenus,
                        StartTrackingPopupMenus)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(AutomationMsg_WaitForPopupMenuToOpen,
                                    WaitForPopupMenuToOpen)
#endif  // defined(TOOLKIT_VIEWS)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(AutomationMsg_InspectElement,
                                    HandleInspectElementRequest)
    IPC_MESSAGE_HANDLER(AutomationMsg_DownloadDirectory, GetDownloadDirectory)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(AutomationMsg_OpenNewBrowserWindowOfType,
                                    OpenNewBrowserWindowOfType)
    IPC_MESSAGE_HANDLER(AutomationMsg_WindowForBrowser, GetWindowForBrowser)
    IPC_MESSAGE_HANDLER(AutomationMsg_BrowserForWindow, GetBrowserForWindow)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(AutomationMsg_ShowInterstitialPage,
                                    ShowInterstitialPage)
    IPC_MESSAGE_HANDLER(AutomationMsg_HideInterstitialPage,
                        HideInterstitialPage)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(AutomationMsg_WaitForTabToBeRestored,
                                    WaitForTabToBeRestored)
    IPC_MESSAGE_HANDLER(AutomationMsg_GetSecurityState, GetSecurityState)
    IPC_MESSAGE_HANDLER(AutomationMsg_GetPageType, GetPageType)
    IPC_MESSAGE_HANDLER(AutomationMsg_GetMetricEventDuration,
                        GetMetricEventDuration)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(AutomationMsg_ActionOnSSLBlockingPage,
                                    ActionOnSSLBlockingPage)
    IPC_MESSAGE_HANDLER(AutomationMsg_BringBrowserToFront, BringBrowserToFront)
    IPC_MESSAGE_HANDLER(AutomationMsg_IsMenuCommandEnabled,
                        IsMenuCommandEnabled)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(AutomationMsg_PrintNow, PrintNow)
    IPC_MESSAGE_HANDLER(AutomationMsg_SavePage, SavePage)
    IPC_MESSAGE_HANDLER(AutomationMsg_OpenFindInPage,
                        HandleOpenFindInPageRequest)
    IPC_MESSAGE_HANDLER(AutomationMsg_FindWindowVisibility,
                        GetFindWindowVisibility)
    IPC_MESSAGE_HANDLER(AutomationMsg_FindWindowLocation,
                        HandleFindWindowLocationRequest)
    IPC_MESSAGE_HANDLER(AutomationMsg_BookmarkBarVisibility,
                        GetBookmarkBarVisibility)
    IPC_MESSAGE_HANDLER(AutomationMsg_GetBookmarksAsJSON,
                        GetBookmarksAsJSON)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(AutomationMsg_WaitForBookmarkModelToLoad,
                                    WaitForBookmarkModelToLoad)
    IPC_MESSAGE_HANDLER(AutomationMsg_AddBookmarkGroup,
                        AddBookmarkGroup)
    IPC_MESSAGE_HANDLER(AutomationMsg_AddBookmarkURL,
                        AddBookmarkURL)
    IPC_MESSAGE_HANDLER(AutomationMsg_ReparentBookmark,
                        ReparentBookmark)
    IPC_MESSAGE_HANDLER(AutomationMsg_SetBookmarkTitle,
                        SetBookmarkTitle)
    IPC_MESSAGE_HANDLER(AutomationMsg_SetBookmarkURL,
                        SetBookmarkURL)
    IPC_MESSAGE_HANDLER(AutomationMsg_RemoveBookmark,
                        RemoveBookmark)
    IPC_MESSAGE_HANDLER(AutomationMsg_GetInfoBarCount, GetInfoBarCount)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(AutomationMsg_ClickInfoBarAccept,
                                    ClickInfoBarAccept)
    IPC_MESSAGE_HANDLER(AutomationMsg_GetLastNavigationTime,
                        GetLastNavigationTime)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(AutomationMsg_WaitForNavigation,
                                    WaitForNavigation)
    IPC_MESSAGE_HANDLER(AutomationMsg_SetIntPreference, SetIntPreference)
    IPC_MESSAGE_HANDLER(AutomationMsg_ShowingAppModalDialog,
                        GetShowingAppModalDialog)
    IPC_MESSAGE_HANDLER(AutomationMsg_ClickAppModalDialogButton,
                        ClickAppModalDialogButton)
    IPC_MESSAGE_HANDLER(AutomationMsg_SetStringPreference, SetStringPreference)
    IPC_MESSAGE_HANDLER(AutomationMsg_GetBooleanPreference,
                        GetBooleanPreference)
    IPC_MESSAGE_HANDLER(AutomationMsg_SetBooleanPreference,
                        SetBooleanPreference)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(
        AutomationMsg_WaitForBrowserWindowCountToBecome,
        WaitForBrowserWindowCountToBecome)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(
        AutomationMsg_WaitForAppModalDialogToBeShown,
        WaitForAppModalDialogToBeShown)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(
        AutomationMsg_GoBackBlockUntilNavigationsComplete,
        GoBackBlockUntilNavigationsComplete)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(
        AutomationMsg_GoForwardBlockUntilNavigationsComplete,
        GoForwardBlockUntilNavigationsComplete)
    IPC_MESSAGE_HANDLER(AutomationMsg_SavePackageShouldPromptUser,
                        SavePackageShouldPromptUser)
    IPC_MESSAGE_HANDLER(AutomationMsg_WindowTitle, GetWindowTitle)
    IPC_MESSAGE_HANDLER(AutomationMsg_SetShelfVisibility, SetShelfVisibility)
    IPC_MESSAGE_HANDLER(AutomationMsg_BlockedPopupCount, GetBlockedPopupCount)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(AutomationMsg_CaptureEntirePageAsPNG,
                                    CaptureEntirePageAsPNG)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(AutomationMsg_SendJSONRequest,
                                    SendJSONRequest)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(AutomationMsg_WaitForTabCountToBecome,
                                    WaitForTabCountToBecome)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(AutomationMsg_WaitForInfoBarCount,
                                    WaitForInfoBarCount)
    IPC_MESSAGE_HANDLER(AutomationMsg_GetPageCurrentEncoding,
                        GetPageCurrentEncoding)
    IPC_MESSAGE_HANDLER(AutomationMsg_ShutdownSessionService,
                        ShutdownSessionService)
    IPC_MESSAGE_HANDLER(AutomationMsg_SetContentSetting, SetContentSetting)
    IPC_MESSAGE_HANDLER(AutomationMsg_LoadBlockedPlugins, LoadBlockedPlugins)
    IPC_MESSAGE_HANDLER(AutomationMsg_ResetToDefaultTheme, ResetToDefaultTheme)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(
        AutomationMsg_WaitForProcessLauncherThreadToGoIdle,
        WaitForProcessLauncherThreadToGoIdle)
    IPC_MESSAGE_HANDLER(AutomationMsg_GetParentBrowserOfTab,
                        GetParentBrowserOfTab)

    IPC_MESSAGE_UNHANDLED(
        handled = AutomationProvider::OnMessageReceived(message))
  IPC_END_MESSAGE_MAP()
  return handled;
}

void TestingAutomationProvider::OnChannelError() {
  if (!reinitialize_on_channel_error_ &&
      browser_shutdown::GetShutdownType() == browser_shutdown::NOT_VALID)
    BrowserList::CloseAllBrowsersAndExit();
  AutomationProvider::OnChannelError();
}

void TestingAutomationProvider::CloseBrowser(int browser_handle,
                                             IPC::Message* reply_message) {
  if (!browser_tracker_->ContainsHandle(browser_handle))
    return;

  Browser* browser = browser_tracker_->GetResource(browser_handle);
  new BrowserClosedNotificationObserver(browser, this, reply_message);
  browser->window()->Close();
}

void TestingAutomationProvider::CloseBrowserAsync(int browser_handle) {
  if (!browser_tracker_->ContainsHandle(browser_handle))
    return;

  Browser* browser = browser_tracker_->GetResource(browser_handle);
  browser->window()->Close();
}

void TestingAutomationProvider::ActivateTab(int handle,
                                            int at_index,
                                            int* status) {
  *status = -1;
  if (browser_tracker_->ContainsHandle(handle) && at_index > -1) {
    Browser* browser = browser_tracker_->GetResource(handle);
    if (at_index >= 0 && at_index < browser->tab_count()) {
      browser->SelectTabContentsAt(at_index, true);
      *status = 0;
    }
  }
}

void TestingAutomationProvider::AppendTab(int handle,
                                          const GURL& url,
                                          IPC::Message* reply_message) {
  int append_tab_response = -1;  // -1 is the error code
  NotificationObserver* observer = NULL;

  if (browser_tracker_->ContainsHandle(handle)) {
    Browser* browser = browser_tracker_->GetResource(handle);
    observer = new TabAppendedNotificationObserver(browser, this,
                                                   reply_message);
    TabContentsWrapper* contents =
        browser->AddSelectedTabWithURL(url, PageTransition::TYPED);
    if (contents) {
      append_tab_response =
          GetIndexForNavigationController(&contents->controller(), browser);
    }
  }

  if (append_tab_response < 0) {
    // Appending tab failed. Clean up and send failure response.

    if (observer)
      delete observer;

    AutomationMsg_AppendTab::WriteReplyParams(reply_message,
                                              append_tab_response);
    Send(reply_message);
  }
}

void TestingAutomationProvider::GetActiveTabIndex(int handle,
                                                  int* active_tab_index) {
  *active_tab_index = -1;  // -1 is the error code
  if (browser_tracker_->ContainsHandle(handle)) {
    Browser* browser = browser_tracker_->GetResource(handle);
    *active_tab_index = browser->selected_index();
  }
}

void TestingAutomationProvider::CloseTab(int tab_handle,
                                         bool wait_until_closed,
                                         IPC::Message* reply_message) {
  if (tab_tracker_->ContainsHandle(tab_handle)) {
    NavigationController* controller = tab_tracker_->GetResource(tab_handle);
    int index;
    Browser* browser = Browser::GetBrowserForController(controller, &index);
    DCHECK(browser);
    new TabClosedNotificationObserver(this, wait_until_closed, reply_message);
    browser->CloseTabContents(controller->tab_contents());
    return;
  }

  AutomationMsg_CloseTab::WriteReplyParams(reply_message, false);
  Send(reply_message);
}

void TestingAutomationProvider::GetCookies(const GURL& url, int handle,
                                           int* value_size,
                                           std::string* value) {
  *value_size = -1;
  if (url.is_valid() && tab_tracker_->ContainsHandle(handle)) {
    // Since we are running on the UI thread don't call GetURLRequestContext().
    scoped_refptr<URLRequestContextGetter> context_getter =
        tab_tracker_->GetResource(handle)->profile()->GetRequestContext();

    base::WaitableEvent event(true /* manual reset */,
                              false /* not initially signaled */);
    CHECK(BrowserThread::PostTask(
              BrowserThread::IO, FROM_HERE,
              NewRunnableFunction(&GetCookiesOnIOThread,
                                  url, context_getter, &event, value)));
    event.Wait();

    *value_size = static_cast<int>(value->size());
  }
}

void TestingAutomationProvider::SetCookie(const GURL& url,
                                          const std::string value,
                                          int handle,
                                          int* response_value) {
  *response_value = -1;

  if (url.is_valid() && tab_tracker_->ContainsHandle(handle)) {
    // Since we are running on the UI thread don't call GetURLRequestContext().
    scoped_refptr<URLRequestContextGetter> context_getter =
        tab_tracker_->GetResource(handle)->profile()->GetRequestContext();

    base::WaitableEvent event(true /* manual reset */,
                              false /* not initially signaled */);
    bool success = false;
    CHECK(BrowserThread::PostTask(
              BrowserThread::IO, FROM_HERE,
              NewRunnableFunction(&SetCookieOnIOThread,
                                  url, value, context_getter, &event,
                                  &success)));
    event.Wait();
    if (success)
      *response_value = 1;
  }
}

void TestingAutomationProvider::DeleteCookie(const GURL& url,
                                             const std::string& cookie_name,
                                             int handle, bool* success) {
  *success = false;
  if (url.is_valid() && tab_tracker_->ContainsHandle(handle)) {
    // Since we are running on the UI thread don't call GetURLRequestContext().
    scoped_refptr<URLRequestContextGetter> context_getter =
        tab_tracker_->GetResource(handle)->profile()->GetRequestContext();

    base::WaitableEvent event(true /* manual reset */,
                              false /* not initially signaled */);
    CHECK(BrowserThread::PostTask(
              BrowserThread::IO, FROM_HERE,
              NewRunnableFunction(&DeleteCookieOnIOThread,
                                  url, cookie_name, context_getter, &event)));
    event.Wait();
    *success = true;
  }
}

void TestingAutomationProvider::ShowCollectedCookiesDialog(
    int handle, bool* success) {
  *success = false;
  if (tab_tracker_->ContainsHandle(handle)) {
    TabContents* tab_contents =
        tab_tracker_->GetResource(handle)->tab_contents();
    tab_contents->delegate()->ShowCollectedCookiesDialog(tab_contents);
    *success = true;
  }
}

void TestingAutomationProvider::NavigateToURLBlockUntilNavigationsComplete(
    int handle, const GURL& url, int number_of_navigations,
    IPC::Message* reply_message) {
  if (tab_tracker_->ContainsHandle(handle)) {
    NavigationController* tab = tab_tracker_->GetResource(handle);

    // Simulate what a user would do. Activate the tab and then navigate.
    // We could allow navigating in a background tab in future.
    Browser* browser = FindAndActivateTab(tab);

    if (browser) {
      new NavigationNotificationObserver(tab, this, reply_message,
                                         number_of_navigations, false);

      // TODO(darin): avoid conversion to GURL.
      browser->OpenURL(url, GURL(), CURRENT_TAB, PageTransition::TYPED);
      return;
    }
  }

  AutomationMsg_NavigateToURLBlockUntilNavigationsComplete::WriteReplyParams(
      reply_message, AUTOMATION_MSG_NAVIGATION_ERROR);
  Send(reply_message);
}

void TestingAutomationProvider::NavigationAsync(int handle,
                                                const GURL& url,
                                                bool* status) {
  NavigationAsyncWithDisposition(handle, url, CURRENT_TAB, status);
}

void TestingAutomationProvider::NavigationAsyncWithDisposition(
    int handle,
    const GURL& url,
    WindowOpenDisposition disposition,
    bool* status) {
  *status = false;

  if (tab_tracker_->ContainsHandle(handle)) {
    NavigationController* tab = tab_tracker_->GetResource(handle);

    // Simulate what a user would do. Activate the tab and then navigate.
    // We could allow navigating in a background tab in future.
    Browser* browser = FindAndActivateTab(tab);

    if (browser) {
      // Don't add any listener unless a callback mechanism is desired.
      // TODO(vibhor): Do this if such a requirement arises in future.
      browser->OpenURL(url, GURL(), disposition, PageTransition::TYPED);
      *status = true;
    }
  }
}

void TestingAutomationProvider::Reload(int handle,
                                       IPC::Message* reply_message) {
  if (tab_tracker_->ContainsHandle(handle)) {
    NavigationController* tab = tab_tracker_->GetResource(handle);
    Browser* browser = FindAndActivateTab(tab);
    if (browser && browser->command_updater()->IsCommandEnabled(IDC_RELOAD)) {
      new NavigationNotificationObserver(tab, this, reply_message, 1, false);
      browser->Reload(CURRENT_TAB);
      return;
    }
  }

  AutomationMsg_Reload::WriteReplyParams(
      reply_message, AUTOMATION_MSG_NAVIGATION_ERROR);
  Send(reply_message);
}

void TestingAutomationProvider::SetAuth(int tab_handle,
                                        const std::wstring& username,
                                        const std::wstring& password,
                                        IPC::Message* reply_message) {
  if (tab_tracker_->ContainsHandle(tab_handle)) {
    NavigationController* tab = tab_tracker_->GetResource(tab_handle);
    LoginHandlerMap::iterator iter = login_handler_map_.find(tab);

    if (iter != login_handler_map_.end()) {
      // If auth is needed again after this, assume login has failed.  This is
      // not strictly correct, because a navigation can require both proxy and
      // server auth, but it should be OK for now.
      LoginHandler* handler = iter->second;
      new NavigationNotificationObserver(tab, this, reply_message, 1, false);
      handler->SetAuth(WideToUTF16Hack(username), WideToUTF16Hack(password));
      return;
    }
  }

  AutomationMsg_SetAuth::WriteReplyParams(
      reply_message, AUTOMATION_MSG_NAVIGATION_AUTH_NEEDED);
  Send(reply_message);
}

void TestingAutomationProvider::CancelAuth(int tab_handle,
                                           IPC::Message* reply_message) {
  if (tab_tracker_->ContainsHandle(tab_handle)) {
    NavigationController* tab = tab_tracker_->GetResource(tab_handle);
    LoginHandlerMap::iterator iter = login_handler_map_.find(tab);

    if (iter != login_handler_map_.end()) {
      // If auth is needed again after this, something is screwy.
      LoginHandler* handler = iter->second;
      new NavigationNotificationObserver(tab, this, reply_message, 1, false);
      handler->CancelAuth();
      return;
    }
  }

  AutomationMsg_CancelAuth::WriteReplyParams(
      reply_message, AUTOMATION_MSG_NAVIGATION_AUTH_NEEDED);
  Send(reply_message);
}

void TestingAutomationProvider::NeedsAuth(int tab_handle, bool* needs_auth) {
  *needs_auth = false;

  if (tab_tracker_->ContainsHandle(tab_handle)) {
    NavigationController* tab = tab_tracker_->GetResource(tab_handle);
    LoginHandlerMap::iterator iter = login_handler_map_.find(tab);

    if (iter != login_handler_map_.end()) {
      // The LoginHandler will be in our map IFF the tab needs auth.
      *needs_auth = true;
    }
  }
}

void TestingAutomationProvider::GetRedirectsFrom(int tab_handle,
                                                 const GURL& source_url,
                                                 IPC::Message* reply_message) {
  if (redirect_query_) {
    LOG(ERROR) << "Can only handle one redirect query at once.";
  } else if (tab_tracker_->ContainsHandle(tab_handle)) {
    NavigationController* tab = tab_tracker_->GetResource(tab_handle);
    HistoryService* history_service =
        tab->profile()->GetHistoryService(Profile::EXPLICIT_ACCESS);

    DCHECK(history_service) << "Tab " << tab_handle << "'s profile " <<
                               "has no history service";
    if (history_service) {
      DCHECK(!reply_message_);
      reply_message_ = reply_message;
      // Schedule a history query for redirects. The response will be sent
      // asynchronously from the callback the history system uses to notify us
      // that it's done: OnRedirectQueryComplete.
      redirect_query_ = history_service->QueryRedirectsFrom(
          source_url, &consumer_,
          NewCallback(this,
                      &TestingAutomationProvider::OnRedirectQueryComplete));
      return;  // Response will be sent when query completes.
    }
  }

  // Send failure response.
  std::vector<GURL> empty;
  AutomationMsg_RedirectsFrom::WriteReplyParams(reply_message, false, empty);
  Send(reply_message);
}

void TestingAutomationProvider::GetBrowserWindowCount(int* window_count) {
  *window_count = static_cast<int>(BrowserList::size());
}

void TestingAutomationProvider::GetNormalBrowserWindowCount(int* window_count) {
  *window_count = static_cast<int>(
      BrowserList::GetBrowserCountForType(profile_, Browser::TYPE_NORMAL));
}

void TestingAutomationProvider::GetBrowserWindow(int index, int* handle) {
  *handle = 0;
  if (index >= 0) {
    BrowserList::const_iterator iter = BrowserList::begin();
    for (; (iter != BrowserList::end()) && (index > 0); ++iter, --index) {}
    if (iter != BrowserList::end()) {
      *handle = browser_tracker_->Add(*iter);
    }
  }
}

void TestingAutomationProvider::FindNormalBrowserWindow(int* handle) {
  *handle = 0;
  Browser* browser = BrowserList::FindBrowserWithType(profile_,
                                                      Browser::TYPE_NORMAL,
                                                      false);
  if (browser)
    *handle = browser_tracker_->Add(browser);
}

void TestingAutomationProvider::GetLastActiveBrowserWindow(int* handle) {
  *handle = 0;
  Browser* browser = BrowserList::GetLastActive();
  if (browser)
    *handle = browser_tracker_->Add(browser);
}

void TestingAutomationProvider::GetActiveWindow(int* handle) {
  *handle = 0;
  Browser* browser = BrowserList::GetLastActive();
  if (browser) {
    gfx::NativeWindow window = browser->window()->GetNativeHandle();
    *handle = window_tracker_->Add(window);
  }
}

void TestingAutomationProvider::ExecuteBrowserCommandAsync(int handle,
                                                           int command,
                                                           bool* success) {
  *success = false;
  if (browser_tracker_->ContainsHandle(handle)) {
    Browser* browser = browser_tracker_->GetResource(handle);
    if (browser->command_updater()->SupportsCommand(command) &&
        browser->command_updater()->IsCommandEnabled(command)) {
      browser->ExecuteCommand(command);
      *success = true;
    }
  }
}

void TestingAutomationProvider::ExecuteBrowserCommand(
    int handle, int command, IPC::Message* reply_message) {
  // List of commands which just finish synchronously and don't require
  // setting up an observer.
  static const int kSynchronousCommands[] = {
    IDC_HOME,
    IDC_SELECT_NEXT_TAB,
    IDC_SELECT_PREVIOUS_TAB,
    IDC_SHOW_BOOKMARK_MANAGER,
  };
  if (browser_tracker_->ContainsHandle(handle)) {
    Browser* browser = browser_tracker_->GetResource(handle);
    if (browser->command_updater()->SupportsCommand(command) &&
        browser->command_updater()->IsCommandEnabled(command)) {
      // First check if we can handle the command without using an observer.
      for (size_t i = 0; i < arraysize(kSynchronousCommands); i++) {
        if (command == kSynchronousCommands[i]) {
          browser->ExecuteCommand(command);
          AutomationMsg_WindowExecuteCommand::WriteReplyParams(reply_message,
                                                               true);
          Send(reply_message);
          return;
        }
      }

      // Use an observer if we have one, otherwise fail.
      if (ExecuteBrowserCommandObserver::CreateAndRegisterObserver(
          this, browser, command, reply_message)) {
        browser->ExecuteCommand(command);
        return;
      }
    }
  }
  AutomationMsg_WindowExecuteCommand::WriteReplyParams(reply_message, false);
  Send(reply_message);
}

void TestingAutomationProvider::GetBrowserLocale(string16* locale) {
  *locale = ASCIIToUTF16(g_browser_process->GetApplicationLocale());
}

void TestingAutomationProvider::IsWindowActive(int handle,
                                               bool* success,
                                               bool* is_active) {
  if (window_tracker_->ContainsHandle(handle)) {
    *is_active =
        platform_util::IsWindowActive(window_tracker_->GetResource(handle));
    *success = true;
  } else {
    *success = false;
    *is_active = false;
  }
}

void TestingAutomationProvider::WindowSimulateClick(const IPC::Message& message,
                                                    int handle,
                                                    const gfx::Point& click,
                                                    int flags) {
  if (window_tracker_->ContainsHandle(handle)) {
    // TODO(phajdan.jr): This is flaky. We should wait for the final click.
    ui_controls::SendMouseMoveNotifyWhenDone(
        click.x(), click.y(), NewRunnableFunction(&SendMouseClick, flags));
  }
}

void TestingAutomationProvider::WindowSimulateMouseMove(
    const IPC::Message& message,
    int handle,
    const gfx::Point& location) {
  if (window_tracker_->ContainsHandle(handle))
    ui_controls::SendMouseMove(location.x(), location.y());
}

void TestingAutomationProvider::WindowSimulateKeyPress(
    const IPC::Message& message,
    int handle,
    int key,
    int flags) {
  if (!window_tracker_->ContainsHandle(handle))
    return;

  gfx::NativeWindow window = window_tracker_->GetResource(handle);
  // The key event is sent to whatever window is active.
  ui_controls::SendKeyPress(window, static_cast<ui::KeyboardCode>(key),
                            ((flags & ui::EF_CONTROL_DOWN) ==
                             ui::EF_CONTROL_DOWN),
                            ((flags & ui::EF_SHIFT_DOWN) ==
                             ui::EF_SHIFT_DOWN),
                            ((flags & ui::EF_ALT_DOWN) ==
                             ui::EF_ALT_DOWN),
                            ((flags & ui::EF_COMMAND_DOWN) ==
                             ui::EF_COMMAND_DOWN));
}

void TestingAutomationProvider::WebkitMouseClick(Browser* browser,
                                                 DictionaryValue* args,
                                                 IPC::Message* reply_message) {
  WebKit::WebMouseEvent mouse_event;

  if (!args->GetInteger("x", &mouse_event.x) ||
      !args->GetInteger("y", &mouse_event.y)) {
    AutomationJSONReply(this, reply_message)
        .SendError("(X,Y) coordinates missing or invalid");
    return;
  }

  int button_flags;
  if (!args->GetInteger("button_flags", &button_flags)) {
    AutomationJSONReply(this, reply_message)
        .SendError("Mouse button missing or invalid");
    return;
  }

  if (button_flags == ui::EF_LEFT_BUTTON_DOWN) {
    mouse_event.button = WebKit::WebMouseEvent::ButtonLeft;
  } else if (button_flags == ui::EF_RIGHT_BUTTON_DOWN) {
    mouse_event.button = WebKit::WebMouseEvent::ButtonRight;
  } else if (button_flags == ui::EF_MIDDLE_BUTTON_DOWN) {
    mouse_event.button = WebKit::WebMouseEvent::ButtonMiddle;
  } else {
    AutomationJSONReply(this, reply_message)
        .SendError("Invalid button press requested");
    return;
  }

  TabContents* tab_contents = browser->GetSelectedTabContents();
  mouse_event.type = WebKit::WebInputEvent::MouseDown;
  mouse_event.clickCount = 1;

  tab_contents->render_view_host()->ForwardMouseEvent(mouse_event);

  mouse_event.type = WebKit::WebInputEvent::MouseUp;
  new InputEventAckNotificationObserver(this, reply_message, mouse_event.type);
  tab_contents->render_view_host()->ForwardMouseEvent(mouse_event);
}

void TestingAutomationProvider::WebkitMouseMove(Browser* browser,
                                                DictionaryValue* args,
                                                IPC::Message* reply_message) {
  WebKit::WebMouseEvent mouse_event;

  if (!args->GetInteger("x", &mouse_event.x) ||
      !args->GetInteger("y", &mouse_event.y)) {
    AutomationJSONReply(this, reply_message)
        .SendError("(X,Y) coordinates missing or invalid");
    return;
  }

  TabContents* tab_contents = browser->GetSelectedTabContents();
  mouse_event.type = WebKit::WebInputEvent::MouseMove;
  new InputEventAckNotificationObserver(this, reply_message, mouse_event.type);
  tab_contents->render_view_host()->ForwardMouseEvent(mouse_event);
}

void TestingAutomationProvider::WebkitMouseDrag(Browser* browser,
                                                DictionaryValue* args,
                                                IPC::Message* reply_message) {
  WebKit::WebMouseEvent mouse_event;
  int start_x, start_y, end_x, end_y;

  if (!args->GetInteger("start_x", &start_x) ||
      !args->GetInteger("start_y", &start_y) ||
      !args->GetInteger("end_x", &end_x) ||
      !args->GetInteger("end_y", &end_y)) {
    AutomationJSONReply(this, reply_message)
        .SendError("Invalid start/end positions");
    return;
  }

  mouse_event.type = WebKit::WebInputEvent::MouseMove;
  TabContents* tab_contents = browser->GetSelectedTabContents();
  // Step 1- Move the mouse to the start position.
  mouse_event.x = start_x;
  mouse_event.y = start_y;
  tab_contents->render_view_host()->ForwardMouseEvent(mouse_event);

  // Step 2- Left click mouse down, the mouse button is fixed.
  mouse_event.type = WebKit::WebInputEvent::MouseDown;
  mouse_event.button = WebKit::WebMouseEvent::ButtonLeft;
  mouse_event.clickCount = 1;
  tab_contents->render_view_host()->ForwardMouseEvent(mouse_event);

  // Step 3 - Move the mouse to the end position.
  // TODO(JMikhail): See if we should simulate the by not making such
  // a drastic jump by placing incrmental stops along the way.
  mouse_event.type = WebKit::WebInputEvent::MouseMove;
  mouse_event.x = end_x;
  mouse_event.y = end_y;
  mouse_event.clickCount = 0;
  tab_contents->render_view_host()->ForwardMouseEvent(mouse_event);

  // Step 4 - Release the left mouse button.
  mouse_event.type = WebKit::WebInputEvent::MouseUp;
  mouse_event.clickCount = 1;
  new InputEventAckNotificationObserver(this, reply_message, mouse_event.type);
  tab_contents->render_view_host()->ForwardMouseEvent(mouse_event);
}

void TestingAutomationProvider::GetTabCount(int handle, int* tab_count) {
  *tab_count = -1;  // -1 is the error code

  if (browser_tracker_->ContainsHandle(handle)) {
    Browser* browser = browser_tracker_->GetResource(handle);
    *tab_count = browser->tab_count();
  }
}

void TestingAutomationProvider::GetType(int handle, int* type_as_int) {
  *type_as_int = -1;  // -1 is the error code

  if (browser_tracker_->ContainsHandle(handle)) {
    Browser* browser = browser_tracker_->GetResource(handle);
    *type_as_int = static_cast<int>(browser->type());
  }
}

void TestingAutomationProvider::GetTab(int win_handle,
                                       int tab_index,
                                       int* tab_handle) {
  *tab_handle = 0;
  if (browser_tracker_->ContainsHandle(win_handle) && (tab_index >= 0)) {
    Browser* browser = browser_tracker_->GetResource(win_handle);
    if (tab_index < browser->tab_count()) {
      TabContents* tab_contents = browser->GetTabContentsAt(tab_index);
      *tab_handle = tab_tracker_->Add(&tab_contents->controller());
    }
  }
}

void TestingAutomationProvider::GetTabProcessID(int handle, int* process_id) {
  *process_id = -1;

  if (tab_tracker_->ContainsHandle(handle)) {
    *process_id = 0;
    TabContents* tab_contents =
        tab_tracker_->GetResource(handle)->tab_contents();
    RenderProcessHost* rph = tab_contents->GetRenderProcessHost();
    if (rph)
      *process_id = base::GetProcId(rph->GetHandle());
  }
}

void TestingAutomationProvider::GetTabTitle(int handle,
                                            int* title_string_size,
                                            std::wstring* title) {
  *title_string_size = -1;  // -1 is the error code
  if (tab_tracker_->ContainsHandle(handle)) {
    NavigationController* tab = tab_tracker_->GetResource(handle);
    NavigationEntry* entry = tab->GetActiveEntry();
    if (entry != NULL) {
      *title = UTF16ToWideHack(entry->title());
    } else {
      *title = std::wstring();
    }
    *title_string_size = static_cast<int>(title->size());
  }
}

void TestingAutomationProvider::GetTabIndex(int handle, int* tabstrip_index) {
  *tabstrip_index = -1;  // -1 is the error code

  if (tab_tracker_->ContainsHandle(handle)) {
    NavigationController* tab = tab_tracker_->GetResource(handle);
    Browser* browser = Browser::GetBrowserForController(tab, NULL);
    *tabstrip_index = browser->tabstrip_model()->GetIndexOfController(tab);
  }
}

void TestingAutomationProvider::GetTabURL(int handle,
                                          bool* success,
                                          GURL* url) {
  *success = false;
  if (tab_tracker_->ContainsHandle(handle)) {
    NavigationController* tab = tab_tracker_->GetResource(handle);
    // Return what the user would see in the location bar.
    *url = tab->GetActiveEntry()->virtual_url();
    *success = true;
  }
}

void TestingAutomationProvider::GetShelfVisibility(int handle, bool* visible) {
  *visible = false;

  if (browser_tracker_->ContainsHandle(handle)) {
#if defined(OS_CHROMEOS)
    // Chromium OS shows FileBrowse ui rather than download shelf. So we
    // enumerate all browsers and look for a chrome://filebrowse... pop up.
    for (BrowserList::const_iterator it = BrowserList::begin();
         it != BrowserList::end(); ++it) {
      if ((*it)->type() == Browser::TYPE_POPUP) {
        const GURL& url =
            (*it)->GetTabContentsAt((*it)->selected_index())->GetURL();

        if (url.SchemeIs(chrome::kChromeUIScheme) &&
            url.host() == chrome::kChromeUIFileBrowseHost) {
          *visible = true;
          break;
        }
      }
    }
#else
    Browser* browser = browser_tracker_->GetResource(handle);
    if (browser) {
      *visible = browser->window()->IsDownloadShelfVisible();
    }
#endif
  }
}

void TestingAutomationProvider::IsFullscreen(int handle, bool* visible) {
  *visible = false;

  if (browser_tracker_->ContainsHandle(handle)) {
    Browser* browser = browser_tracker_->GetResource(handle);
    if (browser)
      *visible = browser->window()->IsFullscreen();
  }
}

void TestingAutomationProvider::GetFullscreenBubbleVisibility(int handle,
                                                              bool* visible) {
  *visible = false;

  if (browser_tracker_->ContainsHandle(handle)) {
    Browser* browser = browser_tracker_->GetResource(handle);
    if (browser)
      *visible = browser->window()->IsFullscreenBubbleVisible();
  }
}

void TestingAutomationProvider::GetAutocompleteEditText(
    int autocomplete_edit_handle,
    bool* success,
    string16* text) {
  *success = false;
  if (autocomplete_edit_tracker_->ContainsHandle(autocomplete_edit_handle)) {
    *text = autocomplete_edit_tracker_->GetResource(autocomplete_edit_handle)->
        GetText();
    *success = true;
  }
}

void TestingAutomationProvider::SetAutocompleteEditText(
    int autocomplete_edit_handle,
    const string16& text,
    bool* success) {
  *success = false;
  if (autocomplete_edit_tracker_->ContainsHandle(autocomplete_edit_handle)) {
    autocomplete_edit_tracker_->GetResource(autocomplete_edit_handle)->
        SetUserText(text);
    *success = true;
  }
}

void TestingAutomationProvider::AutocompleteEditGetMatches(
    int autocomplete_edit_handle,
    bool* success,
    std::vector<AutocompleteMatchData>* matches) {
  *success = false;
  if (autocomplete_edit_tracker_->ContainsHandle(autocomplete_edit_handle)) {
    const AutocompleteResult& result = autocomplete_edit_tracker_->
        GetResource(autocomplete_edit_handle)->model()->result();
    for (AutocompleteResult::const_iterator i = result.begin();
        i != result.end(); ++i)
      matches->push_back(AutocompleteMatchData(*i));
    *success = true;
  }
}

// Waits for the autocomplete edit to receive focus
void  TestingAutomationProvider::WaitForAutocompleteEditFocus(
    int autocomplete_edit_handle,
    IPC::Message* reply_message) {
  if (!autocomplete_edit_tracker_->ContainsHandle(autocomplete_edit_handle)) {
    AutomationMsg_WaitForAutocompleteEditFocus::WriteReplyParams(
        reply_message_, false);
    Send(reply_message);
    return;
  }

  AutocompleteEditModel* model = autocomplete_edit_tracker_->
      GetResource(autocomplete_edit_handle)-> model();
  if (model->has_focus()) {
    AutomationMsg_WaitForAutocompleteEditFocus::WriteReplyParams(
        reply_message, true);
    Send(reply_message);
    return;
  }

  // The observer deletes itself when the notification arrives.
  new AutocompleteEditFocusedObserver(this, model, reply_message);
}

void TestingAutomationProvider::GetAutocompleteEditForBrowser(
    int browser_handle,
    bool* success,
    int* autocomplete_edit_handle) {
  *success = false;
  *autocomplete_edit_handle = 0;

  if (browser_tracker_->ContainsHandle(browser_handle)) {
    Browser* browser = browser_tracker_->GetResource(browser_handle);
    LocationBar* loc_bar = browser->window()->GetLocationBar();
    AutocompleteEditView* edit_view = loc_bar->location_entry();
    // Add() returns the existing handle for the resource if any.
    *autocomplete_edit_handle = autocomplete_edit_tracker_->Add(edit_view);
    *success = true;
  }
}

void TestingAutomationProvider::AutocompleteEditIsQueryInProgress(
    int autocomplete_edit_handle,
    bool* success,
    bool* query_in_progress) {
  *success = false;
  *query_in_progress = false;
  if (autocomplete_edit_tracker_->ContainsHandle(autocomplete_edit_handle)) {
    *query_in_progress = !autocomplete_edit_tracker_->
        GetResource(autocomplete_edit_handle)->model()->
        autocomplete_controller()->done();
    *success = true;
  }
}

void TestingAutomationProvider::ExecuteJavascript(
    int handle,
    const std::wstring& frame_xpath,
    const std::wstring& script,
    IPC::Message* reply_message) {
  TabContents* tab_contents = GetTabContentsForHandle(handle, NULL);
  if (!tab_contents) {
    AutomationMsg_DomOperation::WriteReplyParams(reply_message, std::string());
    Send(reply_message);
    return;
  }

  // Set the routing id of this message with the controller.
  // This routing id needs to be remembered for the reverse
  // communication while sending back the response of
  // this javascript execution.
  std::string set_automation_id;
  base::SStringPrintf(&set_automation_id,
                      "window.domAutomationController.setAutomationId(%d);",
                      reply_message->routing_id());

  DCHECK(!reply_message_);
  reply_message_ = reply_message;

  tab_contents->render_view_host()->ExecuteJavascriptInWebFrame(
      WideToUTF16Hack(frame_xpath), UTF8ToUTF16(set_automation_id));
  tab_contents->render_view_host()->ExecuteJavascriptInWebFrame(
      WideToUTF16Hack(frame_xpath), WideToUTF16Hack(script));
}

void TestingAutomationProvider::GetConstrainedWindowCount(int handle,
                                                          int* count) {
  *count = -1;  // -1 is the error code
  if (tab_tracker_->ContainsHandle(handle)) {
    NavigationController* nav_controller = tab_tracker_->GetResource(handle);
    TabContents* tab_contents = nav_controller->tab_contents();
    if (tab_contents)
      *count = static_cast<int>(tab_contents->child_windows_.size());
  }
}

void TestingAutomationProvider::HandleInspectElementRequest(
    int handle, int x, int y, IPC::Message* reply_message) {
  TabContents* tab_contents = GetTabContentsForHandle(handle, NULL);
  if (tab_contents) {
    DCHECK(!reply_message_);
    reply_message_ = reply_message;

    DevToolsManager::GetInstance()->InspectElement(
        tab_contents->render_view_host(), x, y);
  } else {
    AutomationMsg_InspectElement::WriteReplyParams(reply_message, -1);
    Send(reply_message);
  }
}

void TestingAutomationProvider::GetDownloadDirectory(
    int handle, FilePath* download_directory) {
  if (tab_tracker_->ContainsHandle(handle)) {
    NavigationController* tab = tab_tracker_->GetResource(handle);
    DownloadManager* dlm = tab->profile()->GetDownloadManager();
    *download_directory = dlm->download_prefs()->download_path();
  }
}

void TestingAutomationProvider::OpenNewBrowserWindowOfType(
    int type, bool show, IPC::Message* reply_message) {
  new BrowserOpenedNotificationObserver(this, reply_message);
  // We may have no current browser windows open so don't rely on
  // asking an existing browser to execute the IDC_NEWWINDOW command
  Browser* browser = new Browser(static_cast<Browser::Type>(type), profile_);
  browser->CreateBrowserWindow();
  browser->AddBlankTab(true);
  if (show)
    browser->window()->Show();
}

void TestingAutomationProvider::GetWindowForBrowser(int browser_handle,
                                                    bool* success,
                                                    int* handle) {
  *success = false;
  *handle = 0;

  if (browser_tracker_->ContainsHandle(browser_handle)) {
    Browser* browser = browser_tracker_->GetResource(browser_handle);
    gfx::NativeWindow win = browser->window()->GetNativeHandle();
    // Add() returns the existing handle for the resource if any.
    *handle = window_tracker_->Add(win);
    *success = true;
  }
}

void TestingAutomationProvider::GetBrowserForWindow(int window_handle,
                                                    bool* success,
                                                    int* browser_handle) {
  *success = false;
  *browser_handle = 0;

  gfx::NativeWindow window = window_tracker_->GetResource(window_handle);
  if (!window)
    return;

  BrowserList::const_iterator iter = BrowserList::begin();
  for (;iter != BrowserList::end(); ++iter) {
    gfx::NativeWindow this_window = (*iter)->window()->GetNativeHandle();
    if (window == this_window) {
      // Add() returns the existing handle for the resource if any.
      *browser_handle = browser_tracker_->Add(*iter);
      *success = true;
      return;
    }
  }
}

void TestingAutomationProvider::ShowInterstitialPage(
    int tab_handle,
    const std::string& html_text,
    IPC::Message* reply_message) {
  if (tab_tracker_->ContainsHandle(tab_handle)) {
    NavigationController* controller = tab_tracker_->GetResource(tab_handle);
    TabContents* tab_contents = controller->tab_contents();

    new NavigationNotificationObserver(controller, this, reply_message, 1,
                                       false);

    AutomationInterstitialPage* interstitial =
        new AutomationInterstitialPage(tab_contents,
                                       GURL("about:interstitial"),
                                       html_text);
    interstitial->Show();
    return;
  }

  AutomationMsg_ShowInterstitialPage::WriteReplyParams(
      reply_message, AUTOMATION_MSG_NAVIGATION_ERROR);
  Send(reply_message);
}

void TestingAutomationProvider::HideInterstitialPage(int tab_handle,
                                                     bool* success) {
  *success = false;
  TabContents* tab_contents = GetTabContentsForHandle(tab_handle, NULL);
  if (tab_contents && tab_contents->interstitial_page()) {
    tab_contents->interstitial_page()->DontProceed();
    *success = true;
  }
}

void TestingAutomationProvider::WaitForTabToBeRestored(
    int tab_handle,
    IPC::Message* reply_message) {
  if (tab_tracker_->ContainsHandle(tab_handle)) {
    NavigationController* tab = tab_tracker_->GetResource(tab_handle);
    restore_tracker_.reset(
        new NavigationControllerRestoredObserver(this, tab, reply_message));
  } else {
    AutomationMsg_WaitForTabToBeRestored::WriteReplyParams(
        reply_message, false);
    Send(reply_message);
  }
}

void TestingAutomationProvider::GetSecurityState(int handle,
                                                 bool* success,
                                                 SecurityStyle* security_style,
                                                 int* ssl_cert_status,
                                                 int* insecure_content_status) {
  if (tab_tracker_->ContainsHandle(handle)) {
    NavigationController* tab = tab_tracker_->GetResource(handle);
    NavigationEntry* entry = tab->GetActiveEntry();
    *success = true;
    *security_style = entry->ssl().security_style();
    *ssl_cert_status = entry->ssl().cert_status();
    *insecure_content_status = entry->ssl().content_status();
  } else {
    *success = false;
    *security_style = SECURITY_STYLE_UNKNOWN;
    *ssl_cert_status = 0;
    *insecure_content_status = 0;
  }
}

void TestingAutomationProvider::GetPageType(
    int handle,
    bool* success,
    PageType* page_type) {
  if (tab_tracker_->ContainsHandle(handle)) {
    NavigationController* tab = tab_tracker_->GetResource(handle);
    NavigationEntry* entry = tab->GetActiveEntry();
    *page_type = entry->page_type();
    *success = true;
    // In order to return the proper result when an interstitial is shown and
    // no navigation entry were created for it we need to ask the TabContents.
    if (*page_type == NORMAL_PAGE &&
        tab->tab_contents()->showing_interstitial_page())
      *page_type = INTERSTITIAL_PAGE;
  } else {
    *success = false;
    *page_type = NORMAL_PAGE;
  }
}

void TestingAutomationProvider::GetMetricEventDuration(
    const std::string& event_name,
    int* duration_ms) {
  *duration_ms = metric_event_duration_observer_->GetEventDurationMs(
      event_name);
}

void TestingAutomationProvider::ActionOnSSLBlockingPage(
    int handle,
    bool proceed,
    IPC::Message* reply_message) {
  if (tab_tracker_->ContainsHandle(handle)) {
    NavigationController* tab = tab_tracker_->GetResource(handle);
    NavigationEntry* entry = tab->GetActiveEntry();
    if (entry->page_type() == INTERSTITIAL_PAGE) {
      TabContents* tab_contents = tab->tab_contents();
      InterstitialPage* ssl_blocking_page =
          InterstitialPage::GetInterstitialPage(tab_contents);
      if (ssl_blocking_page) {
        if (proceed) {
          new NavigationNotificationObserver(tab, this, reply_message, 1,
                                             false);
          ssl_blocking_page->Proceed();
          return;
        }
        ssl_blocking_page->DontProceed();
        AutomationMsg_ActionOnSSLBlockingPage::WriteReplyParams(
            reply_message, AUTOMATION_MSG_NAVIGATION_SUCCESS);
        Send(reply_message);
        return;
      }
    }
  }
  // We failed.
  AutomationMsg_ActionOnSSLBlockingPage::WriteReplyParams(
      reply_message, AUTOMATION_MSG_NAVIGATION_ERROR);
  Send(reply_message);
}

void TestingAutomationProvider::BringBrowserToFront(int browser_handle,
                                                    bool* success) {
  *success = false;
  if (browser_tracker_->ContainsHandle(browser_handle)) {
    Browser* browser = browser_tracker_->GetResource(browser_handle);
    browser->window()->Activate();
    *success = true;
  }
}

void TestingAutomationProvider::IsMenuCommandEnabled(int browser_handle,
                                                     int message_num,
                                                     bool* menu_item_enabled) {
  *menu_item_enabled = false;
  if (browser_tracker_->ContainsHandle(browser_handle)) {
    Browser* browser = browser_tracker_->GetResource(browser_handle);
    *menu_item_enabled =
        browser->command_updater()->IsCommandEnabled(message_num);
  }
}

void TestingAutomationProvider::PrintNow(int tab_handle,
                                         IPC::Message* reply_message) {

  NavigationController* tab = NULL;
  TabContents* tab_contents = GetTabContentsForHandle(tab_handle, &tab);
  if (tab_contents) {
    FindAndActivateTab(tab);

    NotificationObserver* observer =
        new DocumentPrintedNotificationObserver(this, reply_message);

    if (!tab_contents->PrintNow()) {
      // Clean up the observer. It will send the reply message.
      delete observer;
    }

    // Return now to avoid sending reply message twice.
    return;
  }

  AutomationMsg_PrintNow::WriteReplyParams(reply_message, false);
  Send(reply_message);
}

void TestingAutomationProvider::SavePage(int tab_handle,
                                         const FilePath& file_name,
                                         const FilePath& dir_path,
                                         int type,
                                         bool* success) {
  SavePackage::SavePackageType save_type =
      static_cast<SavePackage::SavePackageType>(type);
  if (save_type < SavePackage::SAVE_AS_ONLY_HTML ||
      save_type > SavePackage::SAVE_AS_COMPLETE_HTML) {
    *success = false;
    return;
  }

  if (!tab_tracker_->ContainsHandle(tab_handle)) {
    *success = false;
    return;
  }

  NavigationController* nav = tab_tracker_->GetResource(tab_handle);
  Browser* browser = FindAndActivateTab(nav);
  if (!browser->command_updater()->IsCommandEnabled(IDC_SAVE_PAGE)) {
    *success = false;
    return;
  }

  nav->tab_contents()->SavePage(file_name, dir_path, save_type);
  *success = true;
}

void TestingAutomationProvider::HandleOpenFindInPageRequest(
    const IPC::Message& message, int handle) {
  if (browser_tracker_->ContainsHandle(handle)) {
    Browser* browser = browser_tracker_->GetResource(handle);
    browser->FindInPage(false, false);
  }
}

void TestingAutomationProvider::GetFindWindowVisibility(int handle,
                                                        bool* visible) {
  *visible = false;
  Browser* browser = browser_tracker_->GetResource(handle);
  if (browser) {
    FindBarTesting* find_bar =
        browser->GetFindBarController()->find_bar()->GetFindBarTesting();
    find_bar->GetFindBarWindowInfo(NULL, visible);
  }
}

void TestingAutomationProvider::HandleFindWindowLocationRequest(int handle,
                                                                int* x,
                                                                int* y) {
  gfx::Point position(0, 0);
  bool visible = false;
  if (browser_tracker_->ContainsHandle(handle)) {
     Browser* browser = browser_tracker_->GetResource(handle);
     FindBarTesting* find_bar =
       browser->GetFindBarController()->find_bar()->GetFindBarTesting();
     find_bar->GetFindBarWindowInfo(&position, &visible);
  }

  *x = position.x();
  *y = position.y();
}

// Bookmark bar visibility is based on the pref (e.g. is it in the toolbar).
// Presence in the NTP is NOT considered visible by this call.
void TestingAutomationProvider::GetBookmarkBarVisibility(int handle,
                                                         bool* visible,
                                                         bool* animating) {
  *visible = false;
  *animating = false;

  if (browser_tracker_->ContainsHandle(handle)) {
    Browser* browser = browser_tracker_->GetResource(handle);
    if (browser) {
#if 0  // defined(TOOLKIT_VIEWS) && defined(OS_LINUX)
      // TODO(jrg): Was removed in rev43789 for perf. Need to investigate.

      // IsBookmarkBarVisible() line looks correct but is not
      // consistent across platforms.  Specifically, on Mac/Linux, it
      // returns false if the bar is hidden in a pref (even if visible
      // on the NTP).  On ChromeOS, it returned true if on NTP
      // independent of the pref.  Making the code more consistent
      // caused a perf bot regression on Windows (which shares views).
      // See http://crbug.com/40225
      *visible = browser->profile()->GetPrefs()->GetBoolean(
          prefs::kShowBookmarkBar);
#else
      *visible = browser->window()->IsBookmarkBarVisible();
#endif
      *animating = browser->window()->IsBookmarkBarAnimating();
    }
  }
}

void TestingAutomationProvider::GetBookmarksAsJSON(
    int handle,
    std::string* bookmarks_as_json,
    bool *success) {
  *success = false;
  if (browser_tracker_->ContainsHandle(handle)) {
    Browser* browser = browser_tracker_->GetResource(handle);
    if (browser) {
      if (!browser->profile()->GetBookmarkModel()->IsLoaded()) {
        return;
      }
      scoped_refptr<BookmarkStorage> storage(new BookmarkStorage(
          browser->profile(),
          browser->profile()->GetBookmarkModel()));
      *success = storage->SerializeData(bookmarks_as_json);
    }
  }
}

void TestingAutomationProvider::WaitForBookmarkModelToLoad(
    int handle,
    IPC::Message* reply_message) {
  if (browser_tracker_->ContainsHandle(handle)) {
    Browser* browser = browser_tracker_->GetResource(handle);
    BookmarkModel* model = browser->profile()->GetBookmarkModel();
    if (model->IsLoaded()) {
      AutomationMsg_WaitForBookmarkModelToLoad::WriteReplyParams(
          reply_message, true);
      Send(reply_message);
    } else {
      // The observer will delete itself when done.
      new AutomationProviderBookmarkModelObserver(this, reply_message,
                                                  model);
    }
  }
}

void TestingAutomationProvider::AddBookmarkGroup(int handle,
                                                 int64 parent_id,
                                                 int index,
                                                 std::wstring title,
                                                 bool* success) {
  if (browser_tracker_->ContainsHandle(handle)) {
    Browser* browser = browser_tracker_->GetResource(handle);
    if (browser) {
      BookmarkModel* model = browser->profile()->GetBookmarkModel();
      if (!model->IsLoaded()) {
        *success = false;
        return;
      }
      const BookmarkNode* parent = model->GetNodeByID(parent_id);
      DCHECK(parent);
      if (parent) {
        const BookmarkNode* child = model->AddGroup(parent, index,
                                                    WideToUTF16Hack(title));
        DCHECK(child);
        if (child)
          *success = true;
      }
    }
  }
  *success = false;
}

void TestingAutomationProvider::AddBookmarkURL(int handle,
                                               int64 parent_id,
                                               int index,
                                               std::wstring title,
                                               const GURL& url,
                                               bool* success) {
  if (browser_tracker_->ContainsHandle(handle)) {
    Browser* browser = browser_tracker_->GetResource(handle);
    if (browser) {
      BookmarkModel* model = browser->profile()->GetBookmarkModel();
      if (!model->IsLoaded()) {
        *success = false;
        return;
      }
      const BookmarkNode* parent = model->GetNodeByID(parent_id);
      DCHECK(parent);
      if (parent) {
        const BookmarkNode* child = model->AddURL(parent, index,
                                                  WideToUTF16Hack(title), url);
        DCHECK(child);
        if (child)
          *success = true;
      }
    }
  }
  *success = false;
}

void TestingAutomationProvider::ReparentBookmark(int handle,
                                                 int64 id,
                                                 int64 new_parent_id,
                                                 int index,
                                                 bool* success) {
  if (browser_tracker_->ContainsHandle(handle)) {
    Browser* browser = browser_tracker_->GetResource(handle);
    if (browser) {
      BookmarkModel* model = browser->profile()->GetBookmarkModel();
      if (!model->IsLoaded()) {
        *success = false;
        return;
      }
      const BookmarkNode* node = model->GetNodeByID(id);
      DCHECK(node);
      const BookmarkNode* new_parent = model->GetNodeByID(new_parent_id);
      DCHECK(new_parent);
      if (node && new_parent) {
        model->Move(node, new_parent, index);
        *success = true;
      }
    }
  }
  *success = false;
}

void TestingAutomationProvider::SetBookmarkTitle(int handle,
                                                 int64 id,
                                                 std::wstring title,
                                                 bool* success) {
  if (browser_tracker_->ContainsHandle(handle)) {
    Browser* browser = browser_tracker_->GetResource(handle);
    if (browser) {
      BookmarkModel* model = browser->profile()->GetBookmarkModel();
      if (!model->IsLoaded()) {
        *success = false;
        return;
      }
      const BookmarkNode* node = model->GetNodeByID(id);
      DCHECK(node);
      if (node) {
        model->SetTitle(node, WideToUTF16Hack(title));
        *success = true;
      }
    }
  }
  *success = false;
}

void TestingAutomationProvider::SetBookmarkURL(int handle,
                                               int64 id,
                                               const GURL& url,
                                               bool* success) {
  if (browser_tracker_->ContainsHandle(handle)) {
    Browser* browser = browser_tracker_->GetResource(handle);
    if (browser) {
      BookmarkModel* model = browser->profile()->GetBookmarkModel();
      if (!model->IsLoaded()) {
        *success = false;
        return;
      }
      const BookmarkNode* node = model->GetNodeByID(id);
      DCHECK(node);
      if (node) {
        model->SetURL(node, url);
        *success = true;
      }
    }
  }
  *success = false;
}

void TestingAutomationProvider::RemoveBookmark(int handle,
                                               int64 id,
                                               bool* success) {
  if (browser_tracker_->ContainsHandle(handle)) {
    Browser* browser = browser_tracker_->GetResource(handle);
    if (browser) {
      BookmarkModel* model = browser->profile()->GetBookmarkModel();
      if (!model->IsLoaded()) {
        *success = false;
        return;
      }
      const BookmarkNode* node = model->GetNodeByID(id);
      DCHECK(node);
      if (node) {
        const BookmarkNode* parent = node->GetParent();
        DCHECK(parent);
        model->Remove(parent, parent->IndexOfChild(node));
        *success = true;
      }
    }
  }
  *success = false;
}

void TestingAutomationProvider::GetInfoBarCount(int handle, size_t* count) {
  *count = static_cast<size_t>(-1);  // -1 means error.
  if (tab_tracker_->ContainsHandle(handle)) {
    NavigationController* nav_controller = tab_tracker_->GetResource(handle);
    if (nav_controller)
      *count = nav_controller->tab_contents()->infobar_count();
  }
}

void TestingAutomationProvider::ClickInfoBarAccept(
    int handle,
    size_t info_bar_index,
    bool wait_for_navigation,
    IPC::Message* reply_message) {
  bool success = false;
  if (tab_tracker_->ContainsHandle(handle)) {
    NavigationController* nav_controller = tab_tracker_->GetResource(handle);
    if (nav_controller) {
      if (info_bar_index < nav_controller->tab_contents()->infobar_count()) {
        if (wait_for_navigation) {
          new NavigationNotificationObserver(nav_controller, this,
                                             reply_message, 1, false);
        }
        InfoBarDelegate* delegate =
            nav_controller->tab_contents()->GetInfoBarDelegateAt(
                info_bar_index);
        if (delegate->AsConfirmInfoBarDelegate())
          delegate->AsConfirmInfoBarDelegate()->Accept();
        success = true;
      }
    }
  }

  // This "!wait_for_navigation || !success condition" logic looks suspicious.
  // It will send a failure message when success is true but
  // |wait_for_navigation| is false.
  // TODO(phajdan.jr): investgate whether the reply param (currently
  // AUTOMATION_MSG_NAVIGATION_ERROR) should depend on success.
  if (!wait_for_navigation || !success)
    AutomationMsg_ClickInfoBarAccept::WriteReplyParams(
        reply_message, AUTOMATION_MSG_NAVIGATION_ERROR);
}

void TestingAutomationProvider::GetLastNavigationTime(
    int handle,
    int64* last_navigation_time) {
  base::Time time(tab_tracker_->GetLastNavigationTime(handle));
  *last_navigation_time = time.ToInternalValue();
}

void TestingAutomationProvider::WaitForNavigation(int handle,
                                                  int64 last_navigation_time,
                                                  IPC::Message* reply_message) {
  NavigationController* controller = tab_tracker_->GetResource(handle);
  base::Time time(tab_tracker_->GetLastNavigationTime(handle));

  if (time.ToInternalValue() > last_navigation_time || !controller) {
    AutomationMsg_WaitForNavigation::WriteReplyParams(reply_message,
        controller == NULL ? AUTOMATION_MSG_NAVIGATION_ERROR :
                             AUTOMATION_MSG_NAVIGATION_SUCCESS);
    Send(reply_message);
    return;
  }

  new NavigationNotificationObserver(controller, this, reply_message, 1, true);
}

void TestingAutomationProvider::SetIntPreference(int handle,
                                                 const std::string& name,
                                                 int value,
                                                 bool* success) {
  *success = false;
  if (browser_tracker_->ContainsHandle(handle)) {
    Browser* browser = browser_tracker_->GetResource(handle);
    browser->profile()->GetPrefs()->SetInteger(name.c_str(), value);
    *success = true;
  }
}

void TestingAutomationProvider::SetStringPreference(int handle,
                                                    const std::string& name,
                                                    const std::string& value,
                                                    bool* success) {
  *success = false;
  if (browser_tracker_->ContainsHandle(handle)) {
    Browser* browser = browser_tracker_->GetResource(handle);
    browser->profile()->GetPrefs()->SetString(name.c_str(), value);
    *success = true;
  }
}

void TestingAutomationProvider::GetBooleanPreference(int handle,
                                                     const std::string& name,
                                                     bool* success,
                                                     bool* value) {
  *success = false;
  *value = false;
  if (browser_tracker_->ContainsHandle(handle)) {
    Browser* browser = browser_tracker_->GetResource(handle);
    *value = browser->profile()->GetPrefs()->GetBoolean(name.c_str());
    *success = true;
  }
}

void TestingAutomationProvider::SetBooleanPreference(int handle,
                                                     const std::string& name,
                                                     bool value,
                                                     bool* success) {
  *success = false;
  if (browser_tracker_->ContainsHandle(handle)) {
    Browser* browser = browser_tracker_->GetResource(handle);
    browser->profile()->GetPrefs()->SetBoolean(name.c_str(), value);
    *success = true;
  }
}

void TestingAutomationProvider::GetShowingAppModalDialog(bool* showing_dialog,
                                                         int* dialog_button) {
  AppModalDialog* active_dialog =
      AppModalDialogQueue::GetInstance()->active_dialog();
  if (!active_dialog) {
    *showing_dialog = false;
    *dialog_button = ui::MessageBoxFlags::DIALOGBUTTON_NONE;
    return;
  }
  NativeAppModalDialog* native_dialog = active_dialog->native_dialog();
  *showing_dialog = (native_dialog != NULL);
  if (*showing_dialog)
    *dialog_button = native_dialog->GetAppModalDialogButtons();
  else
    *dialog_button = ui::MessageBoxFlags::DIALOGBUTTON_NONE;
}

void TestingAutomationProvider::ClickAppModalDialogButton(int button,
                                                          bool* success) {
  *success = false;

  NativeAppModalDialog* native_dialog =
      AppModalDialogQueue::GetInstance()->active_dialog()->native_dialog();
  if (native_dialog &&
      (native_dialog->GetAppModalDialogButtons() & button) == button) {
    if ((button & ui::MessageBoxFlags::DIALOGBUTTON_OK) ==
        ui::MessageBoxFlags::DIALOGBUTTON_OK) {
      native_dialog->AcceptAppModalDialog();
      *success =  true;
    }
    if ((button & ui::MessageBoxFlags::DIALOGBUTTON_CANCEL) ==
        ui::MessageBoxFlags::DIALOGBUTTON_CANCEL) {
      DCHECK(!*success) << "invalid param, OK and CANCEL specified";
      native_dialog->CancelAppModalDialog();
      *success =  true;
    }
  }
}

void TestingAutomationProvider::WaitForBrowserWindowCountToBecome(
    int target_count,
    IPC::Message* reply_message) {
  if (static_cast<int>(BrowserList::size()) == target_count) {
    AutomationMsg_WaitForBrowserWindowCountToBecome::WriteReplyParams(
        reply_message, true);
    Send(reply_message);
    return;
  }

  // Set up an observer (it will delete itself).
  new BrowserCountChangeNotificationObserver(target_count, this, reply_message);
}

void TestingAutomationProvider::WaitForAppModalDialogToBeShown(
    IPC::Message* reply_message) {
  if (AppModalDialogQueue::GetInstance()->HasActiveDialog()) {
    AutomationMsg_WaitForAppModalDialogToBeShown::WriteReplyParams(
        reply_message, true);
    Send(reply_message);
    return;
  }

  // Set up an observer (it will delete itself).
  new AppModalDialogShownObserver(this, reply_message);
}

void TestingAutomationProvider::GoBackBlockUntilNavigationsComplete(
    int handle, int number_of_navigations, IPC::Message* reply_message) {
  if (tab_tracker_->ContainsHandle(handle)) {
    NavigationController* tab = tab_tracker_->GetResource(handle);
    Browser* browser = FindAndActivateTab(tab);
    if (browser && browser->command_updater()->IsCommandEnabled(IDC_BACK)) {
      new NavigationNotificationObserver(tab, this, reply_message,
                                         number_of_navigations, false);
      browser->GoBack(CURRENT_TAB);
      return;
    }
  }

  AutomationMsg_GoBackBlockUntilNavigationsComplete::WriteReplyParams(
      reply_message, AUTOMATION_MSG_NAVIGATION_ERROR);
  Send(reply_message);
}

void TestingAutomationProvider::GoForwardBlockUntilNavigationsComplete(
    int handle, int number_of_navigations, IPC::Message* reply_message) {
  if (tab_tracker_->ContainsHandle(handle)) {
    NavigationController* tab = tab_tracker_->GetResource(handle);
    Browser* browser = FindAndActivateTab(tab);
    if (browser && browser->command_updater()->IsCommandEnabled(IDC_FORWARD)) {
      new NavigationNotificationObserver(tab, this, reply_message,
                                         number_of_navigations, false);
      browser->GoForward(CURRENT_TAB);
      return;
    }
  }

  AutomationMsg_GoForwardBlockUntilNavigationsComplete::WriteReplyParams(
      reply_message, AUTOMATION_MSG_NAVIGATION_ERROR);
  Send(reply_message);
}

void TestingAutomationProvider::SavePackageShouldPromptUser(
    bool should_prompt) {
  SavePackage::SetShouldPromptUser(should_prompt);
}

void TestingAutomationProvider::SetShelfVisibility(int handle, bool visible) {
  if (browser_tracker_->ContainsHandle(handle)) {
    Browser* browser = browser_tracker_->GetResource(handle);
    if (browser) {
      if (visible)
        browser->window()->GetDownloadShelf()->Show();
      else
        browser->window()->GetDownloadShelf()->Close();
    }
  }
}

void TestingAutomationProvider::GetBlockedPopupCount(int handle, int* count) {
  *count = -1;  // -1 is the error code
  if (tab_tracker_->ContainsHandle(handle)) {
      NavigationController* nav_controller = tab_tracker_->GetResource(handle);
      TabContents* tab_contents = nav_controller->tab_contents();
      if (tab_contents) {
        BlockedContentContainer* container =
            tab_contents->blocked_content_container();
        if (container) {
          *count = static_cast<int>(container->GetBlockedContentsCount());
        } else {
          // If we don't have a container, we don't have any blocked popups to
          // contain!
          *count = 0;
        }
      }
  }
}

void TestingAutomationProvider::CaptureEntirePageAsPNG(
    int tab_handle, const FilePath& path, IPC::Message* reply_message) {
  RenderViewHost* render_view = GetViewForTab(tab_handle);
  if (render_view) {
    // This will delete itself when finished.
    PageSnapshotTaker* snapshot_taker = new PageSnapshotTaker(
        this, reply_message, render_view, path);
    snapshot_taker->Start();
  } else {
    LOG(ERROR) << "Could not get render view for tab handle";
    AutomationMsg_CaptureEntirePageAsPNG::WriteReplyParams(reply_message,
                                                           false);
    Send(reply_message);
  }
}

void TestingAutomationProvider::SendJSONRequest(int handle,
                                                std::string json_request,
                                                IPC::Message* reply_message) {
  scoped_ptr<Value> values;
  base::JSONReader reader;
  std::string error;
  values.reset(reader.ReadAndReturnError(json_request, true, NULL, &error));
  if (!error.empty()) {
    AutomationJSONReply(this, reply_message).SendError(error);
    return;
  }

  // Make sure input is a dict with a string command.
  std::string command;
  DictionaryValue* dict_value = NULL;
  if (values->GetType() != Value::TYPE_DICTIONARY) {
    AutomationJSONReply(this, reply_message).SendError("not a dict");
    return;
  }
  // Ownership remains with "values" variable.
  dict_value = static_cast<DictionaryValue*>(values.get());
  if (!dict_value->GetStringASCII(std::string("command"), &command)) {
    AutomationJSONReply(this, reply_message)
        .SendError("no command key in dict or not a string command");
    return;
  }

  // Map json commands to their handlers.
  std::map<std::string, JsonHandler> handler_map;
#if defined(OS_CHROMEOS)
  handler_map["LoginAsGuest"] = &TestingAutomationProvider::LoginAsGuest;
  handler_map["Login"] = &TestingAutomationProvider::Login;
  handler_map["Logout"] = &TestingAutomationProvider::Logout;
  handler_map["ScreenLock"] = &TestingAutomationProvider::ScreenLock;
  handler_map["ScreenUnlock"] = &TestingAutomationProvider::ScreenUnlock;
#endif  // defined(OS_CHROMEOS)

  std::map<std::string, BrowserJsonHandler> browser_handler_map;
  browser_handler_map["DisablePlugin"] =
      &TestingAutomationProvider::DisablePlugin;
  browser_handler_map["EnablePlugin"] =
      &TestingAutomationProvider::EnablePlugin;
  browser_handler_map["GetPluginsInfo"] =
      &TestingAutomationProvider::GetPluginsInfo;

  browser_handler_map["GetBrowserInfo"] =
      &TestingAutomationProvider::GetBrowserInfo;

  browser_handler_map["GetNavigationInfo"] =
      &TestingAutomationProvider::GetNavigationInfo;

  browser_handler_map["PerformActionOnInfobar"] =
      &TestingAutomationProvider::PerformActionOnInfobar;

  browser_handler_map["GetHistoryInfo"] =
      &TestingAutomationProvider::GetHistoryInfo;
  browser_handler_map["AddHistoryItem"] =
      &TestingAutomationProvider::AddHistoryItem;

  browser_handler_map["GetOmniboxInfo"] =
      &TestingAutomationProvider::GetOmniboxInfo;
  browser_handler_map["SetOmniboxText"] =
      &TestingAutomationProvider::SetOmniboxText;
  browser_handler_map["OmniboxAcceptInput"] =
      &TestingAutomationProvider::OmniboxAcceptInput;
  browser_handler_map["OmniboxMovePopupSelection"] =
      &TestingAutomationProvider::OmniboxMovePopupSelection;

  browser_handler_map["GetInstantInfo"] =
      &TestingAutomationProvider::GetInstantInfo;

  browser_handler_map["LoadSearchEngineInfo"] =
      &TestingAutomationProvider::LoadSearchEngineInfo;
  browser_handler_map["GetSearchEngineInfo"] =
      &TestingAutomationProvider::GetSearchEngineInfo;
  browser_handler_map["AddOrEditSearchEngine"] =
      &TestingAutomationProvider::AddOrEditSearchEngine;
  browser_handler_map["PerformActionOnSearchEngine"] =
      &TestingAutomationProvider::PerformActionOnSearchEngine;

  browser_handler_map["GetPrefsInfo"] =
      &TestingAutomationProvider::GetPrefsInfo;
  browser_handler_map["SetPrefs"] = &TestingAutomationProvider::SetPrefs;

  browser_handler_map["SetWindowDimensions"] =
      &TestingAutomationProvider::SetWindowDimensions;

  browser_handler_map["GetDownloadsInfo"] =
      &TestingAutomationProvider::GetDownloadsInfo;
  browser_handler_map["WaitForAllDownloadsToComplete"] =
      &TestingAutomationProvider::WaitForDownloadsToComplete;
  browser_handler_map["PerformActionOnDownload"] =
      &TestingAutomationProvider::PerformActionOnDownload;

  browser_handler_map["GetInitialLoadTimes"] =
      &TestingAutomationProvider::GetInitialLoadTimes;

  browser_handler_map["SaveTabContents"] =
      &TestingAutomationProvider::SaveTabContents;

  browser_handler_map["ImportSettings"] =
      &TestingAutomationProvider::ImportSettings;

  browser_handler_map["AddSavedPassword"] =
      &TestingAutomationProvider::AddSavedPassword;
  browser_handler_map["RemoveSavedPassword"] =
      &TestingAutomationProvider::RemoveSavedPassword;
  browser_handler_map["GetSavedPasswords"] =
      &TestingAutomationProvider::GetSavedPasswords;

  browser_handler_map["ClearBrowsingData"] =
      &TestingAutomationProvider::ClearBrowsingData;

  browser_handler_map["GetBlockedPopupsInfo"] =
      &TestingAutomationProvider::GetBlockedPopupsInfo;
  browser_handler_map["UnblockAndLaunchBlockedPopup"] =
      &TestingAutomationProvider::UnblockAndLaunchBlockedPopup;

  // SetTheme() implemented using InstallExtension().
  browser_handler_map["GetThemeInfo"] =
      &TestingAutomationProvider::GetThemeInfo;

  // InstallExtension() present in pyauto.py.
  browser_handler_map["GetExtensionsInfo"] =
      &TestingAutomationProvider::GetExtensionsInfo;
  browser_handler_map["UninstallExtensionById"] =
      &TestingAutomationProvider::UninstallExtensionById;

  browser_handler_map["FindInPage"] = &TestingAutomationProvider::FindInPage;

  browser_handler_map["SelectTranslateOption"] =
      &TestingAutomationProvider::SelectTranslateOption;
  browser_handler_map["GetTranslateInfo"] =
      &TestingAutomationProvider::GetTranslateInfo;

  browser_handler_map["GetAutoFillProfile"] =
      &TestingAutomationProvider::GetAutoFillProfile;
  browser_handler_map["FillAutoFillProfile"] =
      &TestingAutomationProvider::FillAutoFillProfile;

  browser_handler_map["GetActiveNotifications"] =
      &TestingAutomationProvider::GetActiveNotifications;
  browser_handler_map["CloseNotification"] =
      &TestingAutomationProvider::CloseNotification;
  browser_handler_map["WaitForNotificationCount"] =
      &TestingAutomationProvider::WaitForNotificationCount;

  browser_handler_map["SignInToSync"] =
      &TestingAutomationProvider::SignInToSync;
  browser_handler_map["GetSyncInfo"] = &TestingAutomationProvider::GetSyncInfo;
  browser_handler_map["AwaitSyncCycleCompletion"] =
      &TestingAutomationProvider::AwaitSyncCycleCompletion;
  browser_handler_map["EnableSyncForDatatypes"] =
      &TestingAutomationProvider::EnableSyncForDatatypes;
  browser_handler_map["DisableSyncForDatatypes"] =
      &TestingAutomationProvider::DisableSyncForDatatypes;

  browser_handler_map["GetNTPInfo"] =
      &TestingAutomationProvider::GetNTPInfo;
  browser_handler_map["MoveNTPMostVisitedThumbnail"] =
      &TestingAutomationProvider::MoveNTPMostVisitedThumbnail;
  browser_handler_map["RemoveNTPMostVisitedThumbnail"] =
      &TestingAutomationProvider::RemoveNTPMostVisitedThumbnail;
  browser_handler_map["UnpinNTPMostVisitedThumbnail"] =
      &TestingAutomationProvider::UnpinNTPMostVisitedThumbnail;
  browser_handler_map["RestoreAllNTPMostVisitedThumbnails"] =
      &TestingAutomationProvider::RestoreAllNTPMostVisitedThumbnails;

  browser_handler_map["KillRendererProcess"] =
      &TestingAutomationProvider::KillRendererProcess;

  browser_handler_map["SendKeyEventToActiveTab"] =
      &TestingAutomationProvider::SendKeyEventToActiveTab;

  browser_handler_map["WebkitMouseMove"] =
      &TestingAutomationProvider::WebkitMouseMove;
  browser_handler_map["WebkitMouseClick"] =
      &TestingAutomationProvider::WebkitMouseClick;
  browser_handler_map["WebkitMouseDrag"] =
      &TestingAutomationProvider::WebkitMouseDrag;

  if (handler_map.find(std::string(command)) != handler_map.end()) {
    (this->*handler_map[command])(dict_value, reply_message);
  } else if (browser_handler_map.find(std::string(command)) !=
             browser_handler_map.end()) {
    Browser* browser = NULL;
    if (!browser_tracker_->ContainsHandle(handle) ||
        !(browser = browser_tracker_->GetResource(handle))) {
      AutomationJSONReply(this, reply_message).SendError("No browser object.");
      return;
    }
    (this->*browser_handler_map[command])(browser, dict_value, reply_message);
  } else {
    std::string error_string = "Unknown command. Options: ";
    for (std::map<std::string, JsonHandler>::const_iterator it =
         handler_map.begin(); it != handler_map.end(); ++it) {
      error_string += it->first + ", ";
    }
    for (std::map<std::string, BrowserJsonHandler>::const_iterator it =
         browser_handler_map.begin(); it != browser_handler_map.end(); ++it) {
      error_string += it->first + ", ";
    }
    AutomationJSONReply(this, reply_message).SendError(error_string);
  }
}

// Sample json input: { "command": "SetWindowDimensions",
//                      "x": 20,         # optional
//                      "y": 20,         # optional
//                      "width": 800,    # optional
//                      "height": 600 }  # optional
void TestingAutomationProvider::SetWindowDimensions(
    Browser* browser,
    DictionaryValue* args,
    IPC::Message* reply_message) {
  gfx::Rect rect = browser->window()->GetRestoredBounds();
  int x, y, width, height;
  if (args->GetInteger("x", &x))
    rect.set_x(x);
  if (args->GetInteger("y", &y))
    rect.set_y(y);
  if (args->GetInteger("width", &width))
    rect.set_width(width);
  if (args->GetInteger("height", &height))
    rect.set_height(height);
  browser->window()->SetBounds(rect);
  AutomationJSONReply(this, reply_message).SendSuccess(NULL);
}

ListValue* TestingAutomationProvider::GetInfobarsInfo(TabContents* tc) {
  // Each infobar may have different properties depending on the type.
  ListValue* infobars = new ListValue;
  for (size_t i = 0; i < tc->infobar_count(); ++i) {
    DictionaryValue* infobar_item = new DictionaryValue;
    InfoBarDelegate* infobar = tc->GetInfoBarDelegateAt(i);
    if (infobar->AsConfirmInfoBarDelegate()) {
      // Also covers ThemeInstalledInfoBarDelegate and
      // CrashedExtensionInfoBarDelegate.
      infobar_item->SetString("type", "confirm_infobar");
      ConfirmInfoBarDelegate* confirm_infobar =
        infobar->AsConfirmInfoBarDelegate();
      infobar_item->SetString("text", confirm_infobar->GetMessageText());
      infobar_item->SetString("link_text", confirm_infobar->GetLinkText());
      ListValue* buttons_list = new ListValue;
      int buttons = confirm_infobar->GetButtons();
      if (buttons & ConfirmInfoBarDelegate::BUTTON_OK) {
        StringValue* button_label = new StringValue(
            confirm_infobar->GetButtonLabel(
              ConfirmInfoBarDelegate::BUTTON_OK));
        buttons_list->Append(button_label);
      }
      if (buttons & ConfirmInfoBarDelegate::BUTTON_CANCEL) {
        StringValue* button_label = new StringValue(
            confirm_infobar->GetButtonLabel(
              ConfirmInfoBarDelegate::BUTTON_CANCEL));
        buttons_list->Append(button_label);
      }
      infobar_item->Set("buttons", buttons_list);
    } else if (infobar->AsLinkInfoBarDelegate()) {
      infobar_item->SetString("type", "link_infobar");
      LinkInfoBarDelegate* link_infobar = infobar->AsLinkInfoBarDelegate();
      infobar_item->SetString("link_text", link_infobar->GetLinkText());
    } else if (infobar->AsTranslateInfoBarDelegate()) {
      infobar_item->SetString("type", "translate_infobar");
      TranslateInfoBarDelegate* translate_infobar =
          infobar->AsTranslateInfoBarDelegate();
      infobar_item->SetString("original_lang_code",
                              translate_infobar->GetOriginalLanguageCode());
      infobar_item->SetString("target_lang_code",
                              translate_infobar->GetTargetLanguageCode());
    } else if (infobar->AsExtensionInfoBarDelegate()) {
      infobar_item->SetString("type", "extension_infobar");
    } else {
      infobar_item->SetString("type", "unknown_infobar");
    }
    infobars->Append(infobar_item);
  }
  return infobars;
}

// Sample json input: { "command": "PerformActionOnInfobar",
//                      "action": "dismiss",
//                      "infobar_index": 0,
//                      "tab_index": 0 }
// Sample output: {}
void TestingAutomationProvider::PerformActionOnInfobar(
    Browser* browser,
    DictionaryValue* args,
    IPC::Message* reply_message) {
  AutomationJSONReply reply(this, reply_message);
  int tab_index;
  int infobar_index_int;
  std::string action;
  if (!args->GetInteger("tab_index", &tab_index) ||
      !args->GetInteger("infobar_index", &infobar_index_int) ||
      !args->GetString("action", &action)) {
    reply.SendError("Invalid or missing args");
    return;
  }
  TabContents* tab_contents = browser->GetTabContentsAt(tab_index);
  if (!tab_contents) {
    reply.SendError(StringPrintf("No such tab at index %d", tab_index));
    return;
  }
  InfoBarDelegate* infobar = NULL;
  size_t infobar_index = static_cast<size_t>(infobar_index_int);
  if (infobar_index >= tab_contents->infobar_count() ||
      !(infobar = tab_contents->GetInfoBarDelegateAt(infobar_index))) {
    reply.SendError(StringPrintf("No such infobar at index %" PRIuS,
                                 infobar_index));
    return;
  }
  if ("dismiss" == action) {
    infobar->InfoBarDismissed();
    tab_contents->RemoveInfoBar(infobar);
    reply.SendSuccess(NULL);
    return;
  }
  if ("accept" == action || "cancel" == action) {
    ConfirmInfoBarDelegate* confirm_infobar;
    if (!(confirm_infobar = infobar->AsConfirmInfoBarDelegate())) {
      reply.SendError("Not a confirm infobar");
      return;
    }
    if ("accept" == action) {
      if (confirm_infobar->Accept())
        tab_contents->RemoveInfoBar(infobar);
    } else if ("cancel" == action) {
      if (confirm_infobar->Cancel())
        tab_contents->RemoveInfoBar(infobar);
    }
    reply.SendSuccess(NULL);
    return;
  }
  reply.SendError("Invalid action");
}

namespace {

// Task to get info about BrowserChildProcessHost. Must run on IO thread to
// honor the semantics of BrowserChildProcessHost.
// Used by AutomationProvider::GetBrowserInfo().
class GetChildProcessHostInfoTask : public Task {
 public:
  GetChildProcessHostInfoTask(base::WaitableEvent* event,
                              ListValue* child_processes)
    : event_(event),
      child_processes_(child_processes) {}

  virtual void Run() {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
    for (BrowserChildProcessHost::Iterator iter; !iter.Done(); ++iter) {
      // Only add processes which are already started,
      // since we need their handle.
      if ((*iter)->handle() == base::kNullProcessHandle) {
        continue;
      }
      ChildProcessInfo* info = *iter;
      DictionaryValue* item = new DictionaryValue;
      item->SetString("name", WideToUTF16Hack(info->name()));
      item->SetString("type",
                      ChildProcessInfo::GetTypeNameInEnglish(info->type()));
      item->SetInteger("pid", base::GetProcId(info->handle()));
      child_processes_->Append(item);
    }
    event_->Signal();
  }

 private:
  base::WaitableEvent* const event_;  // weak
  ListValue* child_processes_;

  DISALLOW_COPY_AND_ASSIGN(GetChildProcessHostInfoTask);
};

}  // namespace

// Sample json input: { "command": "GetBrowserInfo" }
// Refer to GetBrowserInfo() in chrome/test/pyautolib/pyauto.py for
// sample json output.
void TestingAutomationProvider::GetBrowserInfo(
    Browser* browser,
    DictionaryValue* args,
    IPC::Message* reply_message) {
  base::ThreadRestrictions::ScopedAllowIO allow_io;  // needed for PathService
  DictionaryValue* properties = new DictionaryValue;
  properties->SetString("ChromeVersion", chrome::kChromeVersion);
  properties->SetString("BrowserProcessExecutableName",
                        chrome::kBrowserProcessExecutableName);
  properties->SetString("HelperProcessExecutableName",
                        chrome::kHelperProcessExecutableName);
  properties->SetString("BrowserProcessExecutablePath",
                        chrome::kBrowserProcessExecutablePath);
  properties->SetString("HelperProcessExecutablePath",
                        chrome::kHelperProcessExecutablePath);
  properties->SetString("command_line_string",
      CommandLine::ForCurrentProcess()->command_line_string());
  FilePath dumps_path;
  PathService::Get(chrome::DIR_CRASH_DUMPS, &dumps_path);
  properties->SetString("DIR_CRASH_DUMPS", dumps_path.value());

  std::string branding;
#if defined(GOOGLE_CHROME_BUILD)
  branding = "Google Chrome";
#elif defined(CHROMIUM_BUILD)
  branding = "Chromium";
#else
  branding = "Unknown Branding";
#endif
  properties->SetString("branding", branding);

  scoped_ptr<DictionaryValue> return_value(new DictionaryValue);
  return_value->Set("properties", properties);

  return_value->SetInteger("browser_pid", base::GetCurrentProcId());
  // Add info about all windows in a list of dictionaries, one dictionary
  // item per window.
  ListValue* windows = new ListValue;
  int windex = 0;
  for (BrowserList::const_iterator it = BrowserList::begin();
       it != BrowserList::end();
       ++it, ++windex) {
    DictionaryValue* browser_item = new DictionaryValue;
    browser = *it;
    browser_item->SetInteger("index", windex);
    // Window properties
    gfx::Rect rect = browser->window()->GetRestoredBounds();
    browser_item->SetInteger("x", rect.x());
    browser_item->SetInteger("y", rect.y());
    browser_item->SetInteger("width", rect.width());
    browser_item->SetInteger("height", rect.height());
    browser_item->SetBoolean("fullscreen",
                             browser->window()->IsFullscreen());
    browser_item->SetInteger("selected_tab", browser->selected_index());
    browser_item->SetBoolean("incognito",
                             browser->profile()->IsOffTheRecord());
    // For each window, add info about all tabs in a list of dictionaries,
    // one dictionary item per tab.
    ListValue* tabs = new ListValue;
    for (int i = 0; i < browser->tab_count(); ++i) {
      TabContents* tc = browser->GetTabContentsAt(i);
      DictionaryValue* tab = new DictionaryValue;
      tab->SetInteger("index", i);
      tab->SetString("url", tc->GetURL().spec());
      tab->SetInteger("renderer_pid",
                      base::GetProcId(tc->GetRenderProcessHost()->GetHandle()));
      tab->Set("infobars", GetInfobarsInfo(tc));
      tabs->Append(tab);
    }
    browser_item->Set("tabs", tabs);

    windows->Append(browser_item);
  }
  return_value->Set("windows", windows);

  return_value->SetString("child_process_path",
                          ChildProcessHost::GetChildPath(true).value());
  // Child processes are the processes for plugins and other workers.
  // Add all child processes in a list of dictionaries, one dictionary item
  // per child process.
  ListValue* child_processes = new ListValue;
  base::WaitableEvent event(true   /* manual reset */,
                            false  /* not initially signaled */);
  CHECK(BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      new GetChildProcessHostInfoTask(&event, child_processes)));
  event.Wait();
  return_value->Set("child_processes", child_processes);

  // Add all extension processes in a list of dictionaries, one dictionary
  // item per extension process.
  ListValue* extension_processes = new ListValue;
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  for (ProfileManager::const_iterator it = profile_manager->begin();
       it != profile_manager->end(); ++it) {
    ExtensionProcessManager* process_manager =
        (*it)->GetExtensionProcessManager();
    ExtensionProcessManager::const_iterator jt;
    for (jt = process_manager->begin(); jt != process_manager->end(); ++jt) {
      ExtensionHost* ex_host = *jt;
      // Don't add dead extension processes.
      if (!ex_host->IsRenderViewLive())
        continue;
      DictionaryValue* item = new DictionaryValue;
      item->SetString("name", ex_host->extension()->name());
      item->SetInteger(
          "pid",
          base::GetProcId(ex_host->render_process_host()->GetHandle()));
      extension_processes->Append(item);
    }
  }
  return_value->Set("extension_processes", extension_processes);
  AutomationJSONReply(this, reply_message).SendSuccess(return_value.get());
}

// Sample json input: { "command": "GetNavigationInfo" }
// Refer to GetNavigationInfo() in chrome/test/pyautolib/pyauto.py for
// sample json output.
void TestingAutomationProvider::GetNavigationInfo(
    Browser* browser,
    DictionaryValue* args,
    IPC::Message* reply_message) {
  AutomationJSONReply reply(this, reply_message);
  int tab_index;
  TabContents* tab_contents = NULL;
  if (!args->GetInteger("tab_index", &tab_index) ||
      !(tab_contents = browser->GetTabContentsAt(tab_index))) {
    reply.SendError("tab_index missing or invalid.");
    return;
  }
  scoped_ptr<DictionaryValue> return_value(new DictionaryValue);
  const NavigationController& controller = tab_contents->controller();
  NavigationEntry* nav_entry = controller.GetActiveEntry();
  DCHECK(nav_entry);

  // Security info.
  DictionaryValue* ssl = new DictionaryValue;
  std::map<SecurityStyle, std::string> style_to_string;
  style_to_string[SECURITY_STYLE_UNKNOWN] = "SECURITY_STYLE_UNKNOWN";
  style_to_string[SECURITY_STYLE_UNAUTHENTICATED] =
      "SECURITY_STYLE_UNAUTHENTICATED";
  style_to_string[SECURITY_STYLE_AUTHENTICATION_BROKEN] =
      "SECURITY_STYLE_AUTHENTICATION_BROKEN";
  style_to_string[SECURITY_STYLE_AUTHENTICATED] =
      "SECURITY_STYLE_AUTHENTICATED";

  NavigationEntry::SSLStatus ssl_status = nav_entry->ssl();
  ssl->SetString("security_style",
                 style_to_string[ssl_status.security_style()]);
  ssl->SetBoolean("ran_insecure_content", ssl_status.ran_insecure_content());
  ssl->SetBoolean("displayed_insecure_content",
                  ssl_status.displayed_insecure_content());
  return_value->Set("ssl", ssl);

  // Page type.
  std::map<PageType, std::string> pagetype_to_string;
  pagetype_to_string[NORMAL_PAGE] = "NORMAL_PAGE";
  pagetype_to_string[ERROR_PAGE] = "ERROR_PAGE";
  pagetype_to_string[INTERSTITIAL_PAGE] = "INTERSTITIAL_PAGE";
  return_value->SetString("page_type",
                          pagetype_to_string[nav_entry->page_type()]);

  return_value->SetString("favicon_url", nav_entry->favicon().url().spec());
  reply.SendSuccess(return_value.get());
}

// Sample json input: { "command": "GetHistoryInfo",
//                      "search_text": "some text" }
// Refer chrome/test/pyautolib/history_info.py for sample json output.
void TestingAutomationProvider::GetHistoryInfo(Browser* browser,
                                               DictionaryValue* args,
                                               IPC::Message* reply_message) {
  consumer_.CancelAllRequests();

  string16 search_text;
  args->GetString("search_text", &search_text);

  // Fetch history.
  HistoryService* hs = profile_->GetHistoryService(Profile::EXPLICIT_ACCESS);
  history::QueryOptions options;
  // The observer owns itself.  It deletes itself after it fetches history.
  AutomationProviderHistoryObserver* history_observer =
      new AutomationProviderHistoryObserver(this, reply_message);
  hs->QueryHistory(
      search_text,
      options,
      &consumer_,
      NewCallback(history_observer,
                  &AutomationProviderHistoryObserver::HistoryQueryComplete));
}

// Sample json input: { "command": "AddHistoryItem",
//                      "item": { "URL": "http://www.google.com",
//                                "title": "Google",   # optional
//                                "time": 12345        # optional (time_t)
//                               } }
// Refer chrome/test/pyautolib/pyauto.py for details on input.
void TestingAutomationProvider::AddHistoryItem(Browser* browser,
                                               DictionaryValue* args,
                                               IPC::Message* reply_message) {
  DictionaryValue* item = NULL;
  args->GetDictionary("item", &item);
  string16 url_text;
  string16 title;
  base::Time time = base::Time::Now();
  AutomationJSONReply reply(this, reply_message);

  if (!item->GetString("url", &url_text)) {
    reply.SendError("bad args (no URL in dict?)");
    return;
  }
  GURL gurl(url_text);
  item->GetString("title", &title);  // Don't care if it fails.
  int it;
  double dt;
  if (item->GetInteger("time", &it))
    time = base::Time::FromTimeT(it);
  else if (item->GetDouble("time", &dt))
    time = base::Time::FromDoubleT(dt);

  // Ideas for "dummy" values (e.g. id_scope) came from
  // chrome/browser/autocomplete/history_contents_provider_unittest.cc
  HistoryService* hs = profile_->GetHistoryService(Profile::EXPLICIT_ACCESS);
  const void* id_scope = reinterpret_cast<void*>(1);
  hs->AddPage(gurl, time,
              id_scope,
              0,
              GURL(),
              PageTransition::LINK,
              history::RedirectList(),
              history::SOURCE_BROWSED,
              false);
  if (title.length())
    hs->SetPageTitle(gurl, title);
  reply.SendSuccess(NULL);
}

// Sample json input: { "command": "GetDownloadsInfo" }
// Refer chrome/test/pyautolib/download_info.py for sample json output.
void TestingAutomationProvider::GetDownloadsInfo(Browser* browser,
                                                 DictionaryValue* args,
                                                 IPC::Message* reply_message) {
  AutomationJSONReply reply(this, reply_message);

  if (!browser->profile()->HasCreatedDownloadManager()) {
      reply.SendError("no download manager");
      return;
  }

  scoped_ptr<DictionaryValue> return_value(new DictionaryValue);
  std::vector<DownloadItem*> downloads;
  browser->profile()->GetDownloadManager()->
      GetAllDownloads(FilePath(), &downloads);

  ListValue* list_of_downloads = new ListValue;
  for (std::vector<DownloadItem*>::iterator it = downloads.begin();
       it != downloads.end();
       it++) {  // Fill info about each download item.
    list_of_downloads->Append(GetDictionaryFromDownloadItem(*it));
  }
  return_value->Set("downloads", list_of_downloads);
  reply.SendSuccess(return_value.get());
  // All value objects allocated above are owned by |return_value|
  // and get freed by it.
}

void TestingAutomationProvider::WaitForDownloadsToComplete(
    Browser* browser,
    DictionaryValue* args,
    IPC::Message* reply_message) {

  // Look for a quick return.
  if (!browser->profile()->HasCreatedDownloadManager()) {
    // No download manager.
    AutomationJSONReply(this, reply_message).SendSuccess(NULL);
    return;
  }
  std::vector<DownloadItem*> downloads;
  browser->profile()->GetDownloadManager()->
      GetCurrentDownloads(FilePath(), &downloads);
  if (downloads.empty()) {
    AutomationJSONReply(this, reply_message).SendSuccess(NULL);
    return;
  }
  // The observer owns itself.  When the last observed item pings, it
  // deletes itself.
  AutomationProviderDownloadItemObserver* item_observer =
      new AutomationProviderDownloadItemObserver(
          this, reply_message, downloads.size());
  for (std::vector<DownloadItem*>::iterator i = downloads.begin();
       i != downloads.end();
       i++) {
    (*i)->AddObserver(item_observer);
  }
}

namespace {

DownloadItem* GetDownloadItemFromId(int id, DownloadManager* download_manager) {
  std::vector<DownloadItem*> downloads;
  download_manager->GetAllDownloads(FilePath(), &downloads);
  DownloadItem* selected_item = NULL;

  for (std::vector<DownloadItem*>::iterator it = downloads.begin();
       it != downloads.end();
       it++) {
    DownloadItem* curr_item = *it;
    if (curr_item->id() == id) {
      selected_item = curr_item;
      break;
    }
  }
  return selected_item;
}

}  // namespace

// See PerformActionOnDownload() in chrome/test/pyautolib/pyauto.py for sample
// json input and output.
void TestingAutomationProvider::PerformActionOnDownload(
    Browser* browser,
    DictionaryValue* args,
    IPC::Message* reply_message) {
  int id;
  std::string action;

  if (!browser->profile()->HasCreatedDownloadManager()) {
    AutomationJSONReply(this, reply_message).SendError("No download manager.");
    return;
  }
  if (!args->GetInteger("id", &id) || !args->GetString("action", &action)) {
    AutomationJSONReply(this, reply_message)
        .SendError("Must include int id and string action.");
    return;
  }

  DownloadManager* download_manager = browser->profile()->GetDownloadManager();
  DownloadItem* selected_item = GetDownloadItemFromId(id, download_manager);
  if (!selected_item) {
    AutomationJSONReply(this, reply_message)
        .SendError(StringPrintf("No download with an id of %d\n", id));
    return;
  }

  if (action == "open") {
    selected_item->AddObserver(
        new AutomationProviderDownloadUpdatedObserver(
            this, reply_message, true));
    selected_item->OpenDownload();
  } else if (action == "toggle_open_files_like_this") {
    selected_item->OpenFilesBasedOnExtension(
        !selected_item->ShouldOpenFileBasedOnExtension());
    AutomationJSONReply(this, reply_message).SendSuccess(NULL);
  } else if (action == "remove") {
    download_manager->AddObserver(
        new AutomationProviderDownloadModelChangedObserver(
            this, reply_message, download_manager));
    selected_item->Remove(false);
  } else if (action == "decline_dangerous_download") {
    // This is the same as removing the file with delete_file=true.
    download_manager->AddObserver(
        new AutomationProviderDownloadModelChangedObserver(
            this, reply_message, download_manager));
    selected_item->Remove(true);
  } else if (action == "save_dangerous_download") {
    selected_item->AddObserver(new AutomationProviderDownloadUpdatedObserver(
        this, reply_message, false));
    selected_item->DangerousDownloadValidated();
  } else if (action == "toggle_pause") {
    selected_item->AddObserver(new AutomationProviderDownloadUpdatedObserver(
        this, reply_message, false));
    // This will still return if download has already completed.
    selected_item->TogglePause();
  } else if (action == "cancel") {
    selected_item->AddObserver(new AutomationProviderDownloadUpdatedObserver(
        this, reply_message, false));
    selected_item->Cancel(true);
  } else {
    AutomationJSONReply(this, reply_message)
        .SendError(StringPrintf("Invalid action '%s' given.", action.c_str()));
  }
}

// Sample JSON input { "command": "LoadSearchEngineInfo" }
void TestingAutomationProvider::LoadSearchEngineInfo(
    Browser* browser,
    DictionaryValue* args,
    IPC::Message* reply_message) {
  TemplateURLModel* url_model(profile_->GetTemplateURLModel());
  if (url_model->loaded()) {
    AutomationJSONReply(this, reply_message).SendSuccess(NULL);
    return;
  }
  url_model->AddObserver(new AutomationProviderSearchEngineObserver(
      this, reply_message));
  url_model->Load();
}

// Sample JSON input { "command": "GetSearchEngineInfo" }
// Refer to pyauto.py for sample output.
void TestingAutomationProvider::GetSearchEngineInfo(
    Browser* browser,
    DictionaryValue* args,
    IPC::Message* reply_message) {
  TemplateURLModel* url_model(profile_->GetTemplateURLModel());
  scoped_ptr<DictionaryValue> return_value(new DictionaryValue);
  ListValue* search_engines = new ListValue;
  std::vector<const TemplateURL*> template_urls = url_model->GetTemplateURLs();
  for (std::vector<const TemplateURL*>::const_iterator it =
       template_urls.begin(); it != template_urls.end(); ++it) {
    DictionaryValue* search_engine = new DictionaryValue;
    search_engine->SetString("short_name", UTF16ToUTF8((*it)->short_name()));
    search_engine->SetString("description", UTF16ToUTF8((*it)->description()));
    search_engine->SetString("keyword", UTF16ToUTF8((*it)->keyword()));
    search_engine->SetBoolean("in_default_list", (*it)->ShowInDefaultList());
    search_engine->SetBoolean("is_default",
        (*it) == url_model->GetDefaultSearchProvider());
    search_engine->SetBoolean("is_valid", (*it)->url()->IsValid());
    search_engine->SetBoolean("supports_replacement",
                              (*it)->url()->SupportsReplacement());
    search_engine->SetString("url", (*it)->url()->url());
    search_engine->SetString("host", (*it)->url()->GetHost());
    search_engine->SetString("path", (*it)->url()->GetPath());
    search_engine->SetString("display_url",
                             UTF16ToUTF8((*it)->url()->DisplayURL()));
    search_engines->Append(search_engine);
  }
  return_value->Set("search_engines", search_engines);
  AutomationJSONReply(this, reply_message).SendSuccess(return_value.get());
}

// Refer to pyauto.py for sample JSON input.
void TestingAutomationProvider::AddOrEditSearchEngine(
    Browser* browser,
    DictionaryValue* args,
    IPC::Message* reply_message) {
  TemplateURLModel* url_model(profile_->GetTemplateURLModel());
  const TemplateURL* template_url;
  string16 new_title;
  string16 new_keyword;
  std::string new_url;
  std::string keyword;
  if (!args->GetString("new_title", &new_title) ||
      !args->GetString("new_keyword", &new_keyword) ||
      !args->GetString("new_url", &new_url)) {
    AutomationJSONReply(this, reply_message)
        .SendError("One or more inputs invalid");
    return;
  }
  std::string new_ref_url = TemplateURLRef::DisplayURLToURLRef(
      UTF8ToUTF16(new_url));
  scoped_ptr<KeywordEditorController> controller(
      new KeywordEditorController(profile_));
  if (args->GetString("keyword", &keyword)) {
    template_url = url_model->GetTemplateURLForKeyword(UTF8ToUTF16(keyword));
    if (template_url == NULL) {
      AutomationJSONReply(this, reply_message)
          .SendError(StringPrintf("No match for keyword: %s", keyword.c_str()));
      return;
    }
    url_model->AddObserver(new AutomationProviderSearchEngineObserver(
        this, reply_message));
    controller->ModifyTemplateURL(template_url, new_title, new_keyword,
                                  new_ref_url);
  } else {
    url_model->AddObserver(new AutomationProviderSearchEngineObserver(
        this, reply_message));
    controller->AddTemplateURL(new_title, new_keyword, new_ref_url);
  }
}

// Sample json input: { "command": "PerformActionOnSearchEngine",
//                      "keyword": keyword, "action": action }
void TestingAutomationProvider::PerformActionOnSearchEngine(
    Browser* browser,
    DictionaryValue* args,
    IPC::Message* reply_message) {
  TemplateURLModel* url_model(profile_->GetTemplateURLModel());
  std::string keyword;
  std::string action;
  if (!args->GetString("keyword", &keyword) ||
      !args->GetString("action", &action)) {
    AutomationJSONReply(this, reply_message).SendError(
        "One or more inputs invalid");
    return;
  }
  const TemplateURL* template_url(
      url_model->GetTemplateURLForKeyword(UTF8ToUTF16(keyword)));
  if (template_url == NULL) {
    AutomationJSONReply(this, reply_message)
        .SendError(StringPrintf("No match for keyword: %s", keyword.c_str()));
    return;
  }
  if (action == "delete") {
    url_model->AddObserver(new AutomationProviderSearchEngineObserver(
      this, reply_message));
    url_model->Remove(template_url);
  } else if (action == "default") {
    url_model->AddObserver(new AutomationProviderSearchEngineObserver(
      this, reply_message));
    url_model->SetDefaultSearchProvider(template_url);
  } else {
    AutomationJSONReply(this, reply_message)
        .SendError(StringPrintf("Invalid action: %s", action.c_str()));
  }
}

// Sample json input: { "command": "GetPrefsInfo" }
// Refer chrome/test/pyautolib/prefs_info.py for sample json output.
void TestingAutomationProvider::GetPrefsInfo(Browser* browser,
                                             DictionaryValue* args,
                                             IPC::Message* reply_message) {
  DictionaryValue* items = profile_->GetPrefs()->GetPreferenceValues();

  scoped_ptr<DictionaryValue> return_value(new DictionaryValue);
  return_value->Set("prefs", items);  // return_value owns items.
  AutomationJSONReply(this, reply_message).SendSuccess(return_value.get());
}

// Sample json input: { "command": "SetPrefs", "path": path, "value": value }
void TestingAutomationProvider::SetPrefs(Browser* browser,
                                         DictionaryValue* args,
                                         IPC::Message* reply_message) {
  std::string path;
  Value* val;
  AutomationJSONReply reply(this, reply_message);
  if (args->GetString("path", &path) && args->Get("value", &val)) {
    PrefService* pref_service = profile_->GetPrefs();
    const PrefService::Preference* pref =
        pref_service->FindPreference(path.c_str());
    if (!pref) {  // Not a registered pref.
      reply.SendError("pref not registered.");
      return;
    } else if (pref->IsManaged()) {  // Do not attempt to change a managed pref.
      reply.SendError("pref is managed. cannot be changed.");
      return;
    } else {  // Set the pref.
      pref_service->Set(path.c_str(), *val);
    }
  } else {
    reply.SendError("no pref path or value given.");
    return;
  }

  reply.SendSuccess(NULL);
}

// Sample json input: { "command": "GetOmniboxInfo" }
// Refer chrome/test/pyautolib/omnibox_info.py for sample json output.
void TestingAutomationProvider::GetOmniboxInfo(Browser* browser,
                                               DictionaryValue* args,
                                               IPC::Message* reply_message) {
  scoped_ptr<DictionaryValue> return_value(new DictionaryValue);

  LocationBar* loc_bar = browser->window()->GetLocationBar();
  AutocompleteEditView* edit_view = loc_bar->location_entry();
  AutocompleteEditModel* model = edit_view->model();

  // Fill up matches.
  ListValue* matches = new ListValue;
  const AutocompleteResult& result = model->result();
  for (AutocompleteResult::const_iterator i = result.begin();
       i != result.end(); ++i) {
    const AutocompleteMatch& match = *i;
    DictionaryValue* item = new DictionaryValue;  // owned by return_value
    item->SetString("type", AutocompleteMatch::TypeToString(match.type));
    item->SetBoolean("starred", match.starred);
    item->SetString("destination_url", match.destination_url.spec());
    item->SetString("contents", match.contents);
    item->SetString("description", match.description);
    matches->Append(item);
  }
  return_value->Set("matches", matches);

  // Fill up other properties.
  DictionaryValue* properties = new DictionaryValue;  // owned by return_value
  properties->SetBoolean("has_focus", model->has_focus());
  properties->SetBoolean("query_in_progress",
                         !model->autocomplete_controller()->done());
  properties->SetString("keyword", model->keyword());
  properties->SetString("text", edit_view->GetText());
  return_value->Set("properties", properties);

  AutomationJSONReply(this, reply_message).SendSuccess(return_value.get());
}

// Sample json input: { "command": "SetOmniboxText",
//                      "text": "goog" }
void TestingAutomationProvider::SetOmniboxText(Browser* browser,
                                               DictionaryValue* args,
                                               IPC::Message* reply_message) {
  string16 text;
  AutomationJSONReply reply(this, reply_message);
  if (!args->GetString("text", &text)) {
    reply.SendError("text missing");
    return;
  }
  browser->FocusLocationBar();
  LocationBar* loc_bar = browser->window()->GetLocationBar();
  AutocompleteEditView* edit_view = loc_bar->location_entry();
  edit_view->model()->OnSetFocus(false);
  edit_view->SetUserText(text);
  reply.SendSuccess(NULL);
}

// Sample json input: { "command": "OmniboxMovePopupSelection",
//                      "count": 1 }
// Negative count implies up, positive implies down. Count values will be
// capped by the size of the popup list.
void TestingAutomationProvider::OmniboxMovePopupSelection(
    Browser* browser,
    DictionaryValue* args,
    IPC::Message* reply_message) {
  int count;
  AutomationJSONReply reply(this, reply_message);
  if (!args->GetInteger("count", &count)) {
    reply.SendError("count missing");
    return;
  }
  LocationBar* loc_bar = browser->window()->GetLocationBar();
  AutocompleteEditModel* model = loc_bar->location_entry()->model();
  model->OnUpOrDownKeyPressed(count);
  reply.SendSuccess(NULL);
}

// Sample json input: { "command": "OmniboxAcceptInput" }
void TestingAutomationProvider::OmniboxAcceptInput(
    Browser* browser,
    DictionaryValue* args,
    IPC::Message* reply_message) {
  NavigationController& controller =
      browser->GetSelectedTabContents()->controller();
  new OmniboxAcceptNotificationObserver(&controller, this, reply_message);
  browser->window()->GetLocationBar()->AcceptInput();
}

// Sample json input: { "command": "GetInstantInfo" }
void TestingAutomationProvider::GetInstantInfo(Browser* browser,
                                               DictionaryValue* args,
                                               IPC::Message* reply_message) {
  DictionaryValue* info = new DictionaryValue;
  if (browser->instant()) {
    InstantController* instant = browser->instant();
    info->SetBoolean("enabled", true);
    info->SetBoolean("showing", instant->IsShowingInstant());
    info->SetBoolean("active", instant->is_active());
    info->SetBoolean("current", instant->IsCurrent());
    if (instant->GetPreviewContents() &&
        instant->GetPreviewContents()->tab_contents()) {
      TabContents* contents = instant->GetPreviewContents()->tab_contents();
      info->SetBoolean("loading", contents->is_loading());
      info->SetString("location", contents->GetURL().spec());
      info->SetString("title", contents->GetTitle());
    }
  } else {
    info->SetBoolean("enabled", false);
  }
  scoped_ptr<DictionaryValue> return_value(new DictionaryValue);
  return_value->Set("instant", info);
  AutomationJSONReply(this, reply_message).SendSuccess(return_value.get());
}

// Sample json input: { "command": "GetInitialLoadTimes" }
// Refer to InitialLoadObserver::GetTimingInformation() for sample output.
void TestingAutomationProvider::GetInitialLoadTimes(
    Browser*,
    DictionaryValue*,
    IPC::Message* reply_message) {
  scoped_ptr<DictionaryValue> return_value(
      initial_load_observer_->GetTimingInformation());

  std::string json_return;
  base::JSONWriter::Write(return_value.get(), false, &json_return);
  AutomationMsg_SendJSONRequest::WriteReplyParams(
      reply_message, json_return, true);
  Send(reply_message);
}

// Sample json input: { "command": "GetPluginsInfo" }
// Refer chrome/test/pyautolib/plugins_info.py for sample json output.
void TestingAutomationProvider::GetPluginsInfo(
    Browser* browser,
    DictionaryValue* args,
    IPC::Message* reply_message) {
  std::vector<webkit::npapi::WebPluginInfo> plugins;
  webkit::npapi::PluginList::Singleton()->GetPlugins(false, &plugins);
  ListValue* items = new ListValue;
  for (std::vector<webkit::npapi::WebPluginInfo>::const_iterator it =
           plugins.begin();
       it != plugins.end();
       ++it) {
    DictionaryValue* item = new DictionaryValue;
    item->SetString("name", it->name);
    item->SetString("path", it->path.value());
    item->SetString("version", it->version);
    item->SetString("desc", it->desc);
    item->SetBoolean("enabled", webkit::npapi::IsPluginEnabled(*it));
    // Add info about mime types.
    ListValue* mime_types = new ListValue();
    for (std::vector<webkit::npapi::WebPluginMimeType>::const_iterator type_it =
             it->mime_types.begin();
         type_it != it->mime_types.end();
         ++type_it) {
      DictionaryValue* mime_type = new DictionaryValue();
      mime_type->SetString("mimeType", type_it->mime_type);
      mime_type->SetString("description", type_it->description);

      ListValue* file_extensions = new ListValue();
      for (std::vector<std::string>::const_iterator ext_it =
               type_it->file_extensions.begin();
           ext_it != type_it->file_extensions.end();
           ++ext_it) {
        file_extensions->Append(new StringValue(*ext_it));
      }
      mime_type->Set("fileExtensions", file_extensions);

      mime_types->Append(mime_type);
    }
    item->Set("mimeTypes", mime_types);
    items->Append(item);
  }
  scoped_ptr<DictionaryValue> return_value(new DictionaryValue);
  return_value->Set("plugins", items);  // return_value owns items.

  AutomationJSONReply(this, reply_message).SendSuccess(return_value.get());
}

// Sample json input:
//    { "command": "EnablePlugin",
//      "path": "/Library/Internet Plug-Ins/Flash Player.plugin" }
void TestingAutomationProvider::EnablePlugin(Browser* browser,
                                             DictionaryValue* args,
                                             IPC::Message* reply_message) {
  FilePath::StringType path;
  AutomationJSONReply reply(this, reply_message);
  if (!args->GetString("path", &path)) {
    reply.SendError("path not specified.");
    return;
  } else if (!webkit::npapi::PluginList::Singleton()->EnablePlugin(
        FilePath(path))) {
    reply.SendError(StringPrintf("Could not enable plugin for path %s.",
                                 path.c_str()));
    return;
  }
  reply.SendSuccess(NULL);
}

// Sample json input:
//    { "command": "DisablePlugin",
//      "path": "/Library/Internet Plug-Ins/Flash Player.plugin" }
void TestingAutomationProvider::DisablePlugin(Browser* browser,
                                              DictionaryValue* args,
                                              IPC::Message* reply_message) {
  FilePath::StringType path;
  AutomationJSONReply reply(this, reply_message);
  if (!args->GetString("path", &path)) {
    reply.SendError("path not specified.");
    return;
  } else if (!webkit::npapi::PluginList::Singleton()->DisablePlugin(
        FilePath(path))) {
    reply.SendError(StringPrintf("Could not disable plugin for path %s.",
                                 path.c_str()));
    return;
  }
  reply.SendSuccess(NULL);
}

// Sample json input:
//    { "command": "SaveTabContents",
//      "tab_index": 0,
//      "filename": <a full pathname> }
// Sample json output:
//    {}
void TestingAutomationProvider::SaveTabContents(
    Browser* browser,
    DictionaryValue* args,
    IPC::Message* reply_message) {
  int tab_index = 0;
  FilePath::StringType filename;
  FilePath::StringType parent_directory;
  TabContents* tab_contents = NULL;

  if (!args->GetInteger("tab_index", &tab_index) ||
      !args->GetString("filename", &filename)) {
    AutomationJSONReply(this, reply_message)
        .SendError("tab_index or filename param missing");
    return;
  } else {
    tab_contents = browser->GetTabContentsAt(tab_index);
    if (!tab_contents) {
      AutomationJSONReply(this, reply_message).SendError("no tab at tab_index");
      return;
    }
  }
  // We're doing a SAVE_AS_ONLY_HTML so the the directory path isn't
  // used.  Nevertheless, SavePackage requires it be valid.  Sigh.
  parent_directory = FilePath(filename).DirName().value();
  if (!tab_contents->SavePage(FilePath(filename), FilePath(parent_directory),
                              SavePackage::SAVE_AS_ONLY_HTML)) {
    AutomationJSONReply(this, reply_message).SendError(
        "Could not initiate SavePage");
    return;
  }
  // The observer will delete itself when done.
  new SavePackageNotificationObserver(tab_contents->save_package(),
                                      this, reply_message);
}

// Refer to ImportSettings() in chrome/test/pyautolib/pyauto.py for sample
// json input.
// Sample json output: "{}"
void TestingAutomationProvider::ImportSettings(Browser* browser,
                                               DictionaryValue* args,
                                               IPC::Message* reply_message) {
  // Map from the json string passed over to the import item masks.
  std::map<std::string, ImportItem> string_to_import_item;
  string_to_import_item["HISTORY"] = importer::HISTORY;
  string_to_import_item["FAVORITES"] = importer::FAVORITES;
  string_to_import_item["COOKIES"] = importer::COOKIES;
  string_to_import_item["PASSWORDS"] = importer::PASSWORDS;
  string_to_import_item["SEARCH_ENGINES"] = importer::SEARCH_ENGINES;
  string_to_import_item["HOME_PAGE"] = importer::HOME_PAGE;
  string_to_import_item["ALL"] = importer::ALL;

  ListValue* import_items_list = NULL;
  if (!args->GetString("import_from", &import_settings_data_.browser_name) ||
      !args->GetBoolean("first_run", &import_settings_data_.first_run) ||
      !args->GetList("import_items", &import_items_list)) {
    AutomationJSONReply(this, reply_message)
        .SendError("Incorrect type for one or more of the arguments.");
    return;
  }

  import_settings_data_.import_items = 0;
  int num_items = import_items_list->GetSize();
  for (int i = 0; i < num_items; i++) {
    std::string item;
    import_items_list->GetString(i, &item);
    // If the provided string is not part of the map, error out.
    if (!ContainsKey(string_to_import_item, item)) {
      AutomationJSONReply(this, reply_message)
          .SendError("Invalid item string found in import_items.");
      return;
    }
    import_settings_data_.import_items |= string_to_import_item[item];
  }

  import_settings_data_.browser = browser;
  import_settings_data_.reply_message = reply_message;

  // The remaining functionality of importing settings is in
  // SourceProfilesLoaded(), which is called by |importer_list_| once the source
  // profiles are loaded.
  importer_list_ = new ImporterList;
  importer_list_->DetectSourceProfiles(this);
}

namespace {

// Translates a dictionary password to a PasswordForm struct.
webkit_glue::PasswordForm GetPasswordFormFromDict(
    const DictionaryValue& password_dict) {

  // If the time is specified, change time to the specified time.
  base::Time time = base::Time::Now();
  int it;
  double dt;
  if (password_dict.GetInteger("time", &it))
    time = base::Time::FromTimeT(it);
  else if (password_dict.GetDouble("time", &dt))
    time = base::Time::FromDoubleT(dt);

  std::string signon_realm;
  string16 username_value;
  string16 password_value;
  string16 origin_url_text;
  string16 username_element;
  string16 password_element;
  string16 submit_element;
  string16 action_target_text;
  bool blacklist = false;
  string16 old_password_element;
  string16 old_password_value;

  // We don't care if any of these fail - they are either optional or checked
  // before this function is called.
  password_dict.GetString("signon_realm", &signon_realm);
  password_dict.GetString("username_value", &username_value);
  password_dict.GetString("password_value", &password_value);
  password_dict.GetString("origin_url", &origin_url_text);
  password_dict.GetString("username_element", &username_element);
  password_dict.GetString("password_element", &password_element);
  password_dict.GetString("submit_element", &submit_element);
  password_dict.GetString("action_target", &action_target_text);
  password_dict.GetBoolean("blacklist", &blacklist);

  GURL origin_gurl(origin_url_text);
  GURL action_target(action_target_text);

  webkit_glue::PasswordForm password_form;
  password_form.signon_realm = signon_realm;
  password_form.username_value = username_value;
  password_form.password_value = password_value;
  password_form.origin = origin_gurl;
  password_form.username_element = username_element;
  password_form.password_element = password_element;
  password_form.submit_element = submit_element;
  password_form.action = action_target;
  password_form.blacklisted_by_user = blacklist;
  password_form.date_created = time;

  return password_form;
}

}  // namespace

// See AddSavedPassword() in chrome/test/functional/pyauto.py for sample json
// input.
// Sample json output: { "password_added": true }
void TestingAutomationProvider::AddSavedPassword(
    Browser* browser,
    DictionaryValue* args,
    IPC::Message* reply_message) {
  AutomationJSONReply reply(this, reply_message);
  DictionaryValue* password_dict = NULL;

  if (!args->GetDictionary("password", &password_dict)) {
    reply.SendError("Password must be a dictionary.");
    return;
  }

  // The signon realm is effectively the primary key and must be included.
  // Check here before calling GetPasswordFormFromDict.
  if (!password_dict->HasKey("signon_realm")) {
    reply.SendError("Password must include signon_realm.");
    return;
  }
  webkit_glue::PasswordForm new_password =
      GetPasswordFormFromDict(*password_dict);

  Profile* profile = browser->profile();
  // Use IMPLICIT_ACCESS since new passwords aren't added off the record.
  PasswordStore* password_store =
      profile->GetPasswordStore(Profile::IMPLICIT_ACCESS);

  // Set the return based on whether setting the password succeeded.
  scoped_ptr<DictionaryValue> return_value(new DictionaryValue);

  // It will be null if it's accessed in an incognito window.
  if (password_store != NULL) {
    password_store->AddLogin(new_password);
    return_value->SetBoolean("password_added", true);
  } else {
    return_value->SetBoolean("password_added", false);
  }

  reply.SendSuccess(return_value.get());
}

// See RemoveSavedPassword() in chrome/test/functional/pyauto.py for sample
// json input.
// Sample json output: {}
void TestingAutomationProvider::RemoveSavedPassword(
    Browser* browser,
    DictionaryValue* args,
    IPC::Message* reply_message) {
  AutomationJSONReply reply(this, reply_message);
  DictionaryValue* password_dict = NULL;

  if (!args->GetDictionary("password", &password_dict)) {
    reply.SendError("Password must be a dictionary.");
    return;
  }

  // The signon realm is effectively the primary key and must be included.
  // Check here before calling GetPasswordFormFromDict.
  if (!password_dict->HasKey("signon_realm")) {
    reply.SendError("Password must include signon_realm.");
    return;
  }
  webkit_glue::PasswordForm to_remove =
      GetPasswordFormFromDict(*password_dict);

  Profile* profile = browser->profile();
  // Use EXPLICIT_ACCESS since passwords can be removed off the record.
  PasswordStore* password_store =
      profile->GetPasswordStore(Profile::EXPLICIT_ACCESS);

  password_store->RemoveLogin(to_remove);
  reply.SendSuccess(NULL);
}

// Sample json input: { "command": "GetSavedPasswords" }
// Refer to GetSavedPasswords() in chrome/test/pyautolib/pyauto.py for sample
// json output.
void TestingAutomationProvider::GetSavedPasswords(
    Browser* browser,
    DictionaryValue* args,
    IPC::Message* reply_message) {
  Profile* profile = browser->profile();
  // Use EXPLICIT_ACCESS since saved passwords can be retreived off the record.
  PasswordStore* password_store =
      profile->GetPasswordStore(Profile::EXPLICIT_ACCESS);
  password_store->GetAutofillableLogins(
      new AutomationProviderGetPasswordsObserver(this, reply_message));
  // Observer deletes itself after returning.
}

// Refer to ClearBrowsingData() in chrome/test/pyautolib/pyauto.py for sample
// json input.
// Sample json output: {}
void TestingAutomationProvider::ClearBrowsingData(
    Browser* browser,
    DictionaryValue* args,
    IPC::Message* reply_message) {
  std::map<std::string, BrowsingDataRemover::TimePeriod> string_to_time_period;
  string_to_time_period["LAST_HOUR"] = BrowsingDataRemover::LAST_HOUR;
  string_to_time_period["LAST_DAY"] = BrowsingDataRemover::LAST_DAY;
  string_to_time_period["LAST_WEEK"] = BrowsingDataRemover::LAST_WEEK;
  string_to_time_period["FOUR_WEEKS"] = BrowsingDataRemover::FOUR_WEEKS;
  string_to_time_period["EVERYTHING"] = BrowsingDataRemover::EVERYTHING;

  std::map<std::string, int> string_to_mask_value;
  string_to_mask_value["HISTORY"] = BrowsingDataRemover::REMOVE_HISTORY;
  string_to_mask_value["DOWNLOADS"] = BrowsingDataRemover::REMOVE_DOWNLOADS;
  string_to_mask_value["COOKIES"] = BrowsingDataRemover::REMOVE_COOKIES;
  string_to_mask_value["PASSWORDS"] = BrowsingDataRemover::REMOVE_PASSWORDS;
  string_to_mask_value["FORM_DATA"] = BrowsingDataRemover::REMOVE_FORM_DATA;
  string_to_mask_value["CACHE"] = BrowsingDataRemover::REMOVE_CACHE;

  std::string time_period;
  ListValue* to_remove;
  if (!args->GetString("time_period", &time_period) ||
      !args->GetList("to_remove", &to_remove)) {
    AutomationJSONReply(this, reply_message)
        .SendError("time_period must be a string and to_remove a list.");
    return;
  }

  int remove_mask = 0;
  int num_removals = to_remove->GetSize();
  for (int i = 0; i < num_removals; i++) {
    std::string removal;
    to_remove->GetString(i, &removal);
    // If the provided string is not part of the map, then error out.
    if (!ContainsKey(string_to_mask_value, removal)) {
      AutomationJSONReply(this, reply_message)
          .SendError("Invalid browsing data string found in to_remove.");
      return;
    }
    remove_mask |= string_to_mask_value[removal];
  }

  if (!ContainsKey(string_to_time_period, time_period)) {
    AutomationJSONReply(this, reply_message)
        .SendError("Invalid string for time_period.");
    return;
  }

  BrowsingDataRemover* remover = new BrowsingDataRemover(
      profile(), string_to_time_period[time_period], base::Time());

  remover->AddObserver(
      new AutomationProviderBrowsingDataObserver(this, reply_message));
  remover->Remove(remove_mask);
  // BrowsingDataRemover deletes itself using DeleteTask.
  // The observer also deletes itself after sending the reply.
}

namespace {

  // Get the TabContents from a dictionary of arguments.
  TabContents* GetTabContentsFromDict(const Browser* browser,
                                      const DictionaryValue* args,
                                      std::string* error_message) {
    int tab_index;
    if (!args->GetInteger("tab_index", &tab_index)) {
      *error_message = "Must include tab_index.";
      return NULL;
    }

    TabContents* tab_contents = browser->GetTabContentsAt(tab_index);
    if (!tab_contents) {
      *error_message = StringPrintf("No tab at index %d.", tab_index);
      return NULL;
    }
    return tab_contents;
  }

  // Get the TranslateInfoBarDelegate from TabContents.
  TranslateInfoBarDelegate* GetTranslateInfoBarDelegate(
      TabContents* tab_contents) {
    for (size_t i = 0; i < tab_contents->infobar_count(); i++) {
      InfoBarDelegate* infobar = tab_contents->GetInfoBarDelegateAt(i);
      if (infobar->AsTranslateInfoBarDelegate())
        return infobar->AsTranslateInfoBarDelegate();
    }
    // No translate infobar.
    return NULL;
  }
}  // namespace

void TestingAutomationProvider::FindInPage(
    Browser* browser,
    DictionaryValue* args,
    IPC::Message* reply_message) {
  std::string error_message;
  TabContents* tab_contents = GetTabContentsFromDict(browser, args,
                                                     &error_message);
  if (!tab_contents) {
    AutomationJSONReply(this, reply_message).SendError(error_message);
    return;
  }
  string16 search_string;
  bool forward;
  bool match_case;
  bool find_next;
  if (!args->GetString("search_string", &search_string)) {
    AutomationJSONReply(this, reply_message).
        SendError("Must include search_string string.");
    return;
  }
  if (!args->GetBoolean("forward", &forward)) {
    AutomationJSONReply(this, reply_message).
        SendError("Must include forward boolean.");
    return;
  }
  if (!args->GetBoolean("match_case", &match_case)) {
    AutomationJSONReply(this, reply_message).
        SendError("Must include match_case boolean.");
    return;
  }
  if (!args->GetBoolean("find_next", &find_next)) {
    AutomationJSONReply(this, reply_message).
        SendError("Must include find_next boolean.");
    return;
  }
  SendFindRequest(tab_contents,
                  true,
                  search_string,
                  forward,
                  match_case,
                  find_next,
                  reply_message);
}

// See GetTranslateInfo() in chrome/test/pyautolib/pyauto.py for sample json
// input and output.
void TestingAutomationProvider::GetTranslateInfo(
    Browser* browser,
    DictionaryValue* args,
    IPC::Message* reply_message) {
  std::string error_message;
  TabContents* tab_contents = GetTabContentsFromDict(browser, args,
                                                     &error_message);
  if (!tab_contents) {
    AutomationJSONReply(this, reply_message).SendError(error_message);
    return;
  }

  // Get the translate bar if there is one and pass it to the observer.
  // The observer will check for null and populate the information accordingly.
  TranslateInfoBarDelegate* translate_bar =
      GetTranslateInfoBarDelegate(tab_contents);

  TabLanguageDeterminedObserver* observer = new TabLanguageDeterminedObserver(
      this, reply_message, tab_contents, translate_bar);
  // If the language for the page hasn't been loaded yet, then just make
  // the observer, otherwise call observe directly.
  std::string language = tab_contents->language_state().original_language();
  if (!language.empty()) {
    observer->Observe(NotificationType::TAB_LANGUAGE_DETERMINED,
                      Source<TabContents>(tab_contents),
                      Details<std::string>(&language));
  }
}

// See SelectTranslateOption() in chrome/test/pyautolib/pyauto.py for sample
// json input.
// Sample json output: {}
void TestingAutomationProvider::SelectTranslateOption(
    Browser* browser,
    DictionaryValue* args,
    IPC::Message* reply_message) {
  std::string option;
  std::string error_message;
  TabContents* tab_contents = GetTabContentsFromDict(browser, args,
                                                     &error_message);
  if (!tab_contents) {
    AutomationJSONReply(this, reply_message).SendError(error_message);
    return;
  }

  TranslateInfoBarDelegate* translate_bar =
      GetTranslateInfoBarDelegate(tab_contents);
  if (!translate_bar) {
    AutomationJSONReply(this, reply_message)
        .SendError("There is no translate bar open.");
    return;
  }

  if (!args->GetString("option", &option)) {
    AutomationJSONReply(this, reply_message).SendError("Must include option");
    return;
  }

  if (option == "translate_page") {
    // Make a new notification observer which will send the reply.
    new PageTranslatedObserver(this, reply_message, tab_contents);
    translate_bar->Translate();
    return;
  } else if (option == "set_target_language") {
    string16 target_language;
    if (!args->GetString("target_language", &target_language)) {
       AutomationJSONReply(this, reply_message)
           .SendError("Must include target_language string.");
      return;
    }
    // Get the target language index based off of the language name.
    size_t target_language_index = TranslateInfoBarDelegate::kNoIndex;
    for (size_t i = 0; i < translate_bar->GetLanguageCount(); i++) {
      if (translate_bar->GetLanguageDisplayableNameAt(i) == target_language) {
        target_language_index = i;
        break;
      }
    }
    if (target_language_index == TranslateInfoBarDelegate::kNoIndex) {
       AutomationJSONReply(this, reply_message)
           .SendError("Invalid target language string.");
       return;
    }
    // If the page has already been translated it will be translated again to
    // the new language. The observer will wait until the page has been
    // translated to reply.
    if (translate_bar->type() == TranslateInfoBarDelegate::AFTER_TRANSLATE) {
      new PageTranslatedObserver(this, reply_message, tab_contents);
      translate_bar->SetTargetLanguage(target_language_index);
      return;
    }
    // Otherwise just send the reply back immediately.
    translate_bar->SetTargetLanguage(target_language_index);
    scoped_ptr<DictionaryValue> return_value(new DictionaryValue);
    return_value->SetBoolean("translation_success", true);
    AutomationJSONReply(this, reply_message).SendSuccess(return_value.get());
    return;
  } else if (option == "click_always_translate_lang_button") {
    if (!translate_bar->ShouldShowAlwaysTranslateButton()) {
      AutomationJSONReply(this, reply_message)
          .SendError("Always translate button not showing.");
      return;
    }
    // Clicking 'Always Translate' triggers a translation. The observer will
    // wait until the translation is complete before sending the reply.
    new PageTranslatedObserver(this, reply_message, tab_contents);
    translate_bar->AlwaysTranslatePageLanguage();
    return;
  }

  AutomationJSONReply reply(this, reply_message);
  if (option == "never_translate_language") {
    if (translate_bar->IsLanguageBlacklisted()) {
      reply.SendError("The language was already blacklisted.");
      return;
    }
    translate_bar->ToggleLanguageBlacklist();
    reply.SendSuccess(NULL);
  } else if (option == "never_translate_site") {
    if (translate_bar->IsSiteBlacklisted()) {
      reply.SendError("The site was already blacklisted.");
      return;
    }
    translate_bar->ToggleSiteBlacklist();
    reply.SendSuccess(NULL);
  } else if (option == "toggle_always_translate") {
    translate_bar->ToggleAlwaysTranslate();
    reply.SendSuccess(NULL);
  } else if (option == "revert_translation") {
    translate_bar->RevertTranslation();
    reply.SendSuccess(NULL);
  } else if (option == "click_never_translate_lang_button") {
    if (!translate_bar->ShouldShowNeverTranslateButton()) {
      reply.SendError("Always translate button not showing.");
      return;
    }
    translate_bar->NeverTranslatePageLanguage();
    reply.SendSuccess(NULL);
  } else if (option == "decline_translation") {
    // This is the function called when an infobar is dismissed or when the
    // user clicks the 'Nope' translate button.
    translate_bar->TranslationDeclined();
    tab_contents->RemoveInfoBar(translate_bar);
    reply.SendSuccess(NULL);
  } else {
    reply.SendError("Invalid string found for option.");
  }
}

// Sample json input: { "command": "GetBlockedPopupsInfo",
//                      "tab_index": 1 }
// Refer GetBlockedPopupsInfo() in pyauto.py for sample output.
void TestingAutomationProvider::GetBlockedPopupsInfo(
    Browser* browser,
    DictionaryValue* args,
    IPC::Message* reply_message) {
  AutomationJSONReply reply(this, reply_message);
  std::string error_message;
  TabContents* tab_contents = GetTabContentsFromDict(
      browser, args, &error_message);
  if (!tab_contents) {
    reply.SendError(error_message);
    return;
  }
  scoped_ptr<DictionaryValue> return_value(new DictionaryValue);
  BlockedContentContainer* popup_container =
      tab_contents->blocked_content_container();
  ListValue* blocked_popups_list = new ListValue;
  if (popup_container) {
    std::vector<TabContents*> blocked_contents;
    popup_container->GetBlockedContents(&blocked_contents);
    for (std::vector<TabContents*>::const_iterator it =
             blocked_contents.begin(); it != blocked_contents.end(); ++it) {
      DictionaryValue* item = new DictionaryValue;
      item->SetString("url", (*it)->GetURL().spec());
      item->SetString("title", (*it)->GetTitle());
      blocked_popups_list->Append(item);
    }
  }
  return_value->Set("blocked_popups", blocked_popups_list);
  reply.SendSuccess(return_value.get());
}

// Refer UnblockAndLaunchBlockedPopup() in pyauto.py for sample input.
void TestingAutomationProvider::UnblockAndLaunchBlockedPopup(
    Browser* browser,
    DictionaryValue* args,
    IPC::Message* reply_message) {
  AutomationJSONReply reply(this, reply_message);
  std::string error_message;
  TabContents* tab_contents = GetTabContentsFromDict(
      browser, args, &error_message);
  if (!tab_contents) {
    reply.SendError(error_message);
    return;
  }
  int popup_index;
  if (!args->GetInteger("popup_index", &popup_index)) {
    reply.SendError("Need popup_index arg");
    return;
  }
  scoped_ptr<DictionaryValue> return_value(new DictionaryValue);
  BlockedContentContainer* content_container =
      tab_contents->blocked_content_container();
  if (!content_container ||
      popup_index >= (int)content_container->GetBlockedContentsCount()) {
    reply.SendError(StringPrintf("No popup at index %d", popup_index));
    return;
  }
  std::vector<TabContents*> blocked_contents;
  content_container->GetBlockedContents(&blocked_contents);
  content_container->LaunchForContents(blocked_contents[popup_index]);
  reply.SendSuccess(NULL);
}

// Sample json input: { "command": "GetThemeInfo" }
// Refer GetThemeInfo() in chrome/test/pyautolib/pyauto.py for sample output.
void TestingAutomationProvider::GetThemeInfo(
    Browser* browser,
    DictionaryValue* args,
    IPC::Message* reply_message) {
  scoped_ptr<DictionaryValue> return_value(new DictionaryValue);
  const Extension* theme = browser->profile()->GetTheme();
  if (theme) {
    return_value->SetString("name", theme->name());
    return_value->Set("images", theme->GetThemeImages()->DeepCopy());
    return_value->Set("colors", theme->GetThemeColors()->DeepCopy());
    return_value->Set("tints", theme->GetThemeTints()->DeepCopy());
  }
  AutomationJSONReply(this, reply_message).SendSuccess(return_value.get());
}

// Sample json input: { "command": "GetExtensionsInfo" }
// See GetExtensionsInfo() in chrome/test/pyautolib/pyauto.py for sample json
// output.
void TestingAutomationProvider::GetExtensionsInfo(
    Browser* browser,
    DictionaryValue* args,
    IPC::Message* reply_message) {
  AutomationJSONReply reply(this, reply_message);
  ExtensionService* service = profile()->GetExtensionService();
  if (!service) {
    reply.SendError("No extensions service.");
  }
  scoped_ptr<DictionaryValue> return_value(new DictionaryValue);
  ListValue* extensions_values = new ListValue;
  const ExtensionList* extensions = service->extensions();
  for (ExtensionList::const_iterator it = extensions->begin();
       it != extensions->end(); ++it) {
    const Extension* extension = *it;
    DictionaryValue* extension_value = new DictionaryValue;
    extension_value->SetString("id", extension->id());
    extension_value->SetString("version", extension->VersionString());
    extension_value->SetString("name", extension->name());
    extension_value->SetString("public_key", extension->public_key());
    extension_value->SetString("description", extension->description());
    extension_value->SetString("background_url",
                               extension->background_url().spec());
    extension_value->SetString("options_url",
                               extension->options_url().spec());
    extensions_values->Append(extension_value);
  }
  return_value->Set("extensions", extensions_values);
  reply.SendSuccess(return_value.get());
}

// See UninstallExtensionById() in chrome/test/pyautolib/pyauto.py for sample
// json input.
// Sample json output: {}
void TestingAutomationProvider::UninstallExtensionById(
    Browser* browser,
    DictionaryValue* args,
    IPC::Message* reply_message) {
  AutomationJSONReply reply(this, reply_message);
  std::string id;
  if (!args->GetString("id", &id)) {
    reply.SendError("Must include string id.");
    return;
  }
  ExtensionService* service = profile()->GetExtensionService();
  if (!service) {
    reply.SendError("No extensions service.");
    return;
  }
  ExtensionUnloadNotificationObserver observer;
  service->UninstallExtension(id, false);
  reply.SendSuccess(NULL);
}

// Sample json input:
//    { "command": "GetAutoFillProfile" }
// Refer to GetAutoFillProfile() in chrome/test/pyautolib/pyauto.py for sample
// json output.
void TestingAutomationProvider::GetAutoFillProfile(
    Browser* browser,
    DictionaryValue* args,
    IPC::Message* reply_message) {
  AutomationJSONReply reply(this, reply_message);
  // Get the AutoFillProfiles currently in the database.
  int tab_index = 0;
  if (!args->GetInteger("tab_index", &tab_index)) {
    reply.SendError("Invalid or missing tab_index integer value.");
    return;
  }

  TabContents* tab_contents = browser->GetTabContentsAt(tab_index);
  if (tab_contents) {
    PersonalDataManager* pdm = tab_contents->profile()->GetOriginalProfile()
        ->GetPersonalDataManager();
    if (pdm) {
      std::vector<AutoFillProfile*> autofill_profiles = pdm->profiles();
      std::vector<CreditCard*> credit_cards = pdm->credit_cards();

      ListValue* profiles = GetListFromAutoFillProfiles(autofill_profiles);
      ListValue* cards = GetListFromCreditCards(credit_cards);

      scoped_ptr<DictionaryValue> return_value(new DictionaryValue);

      return_value->Set("profiles", profiles);
      return_value->Set("credit_cards", cards);
      reply.SendSuccess(return_value.get());
    } else {
      reply.SendError("No PersonalDataManager.");
      return;
    }
  } else {
    reply.SendError("No tab at that index.");
    return;
  }
}

// Refer to FillAutoFillProfile() in chrome/test/pyautolib/pyauto.py for sample
// json input.
// Sample json output: {}
void TestingAutomationProvider::FillAutoFillProfile(
    Browser* browser,
    DictionaryValue* args,
    IPC::Message* reply_message) {
  AutomationJSONReply reply(this, reply_message);
  ListValue* profiles = NULL;
  ListValue* cards = NULL;

  // It's ok for profiles/credit_cards elements to be missing.
  args->GetList("profiles", &profiles);
  args->GetList("credit_cards", &cards);

  std::string error_mesg;

  std::vector<AutoFillProfile> autofill_profiles;
  std::vector<CreditCard> credit_cards;
  // Create an AutoFillProfile for each of the dictionary profiles.
  if (profiles) {
    autofill_profiles = GetAutoFillProfilesFromList(*profiles, &error_mesg);
  }
  // Create a CreditCard for each of the dictionary values.
  if (cards) {
    credit_cards = GetCreditCardsFromList(*cards, &error_mesg);
  }
  if (!error_mesg.empty()) {
    reply.SendError(error_mesg);
    return;
  }

  // Save the AutoFillProfiles.
  int tab_index = 0;
  if (!args->GetInteger("tab_index", &tab_index)) {
    reply.SendError("Invalid or missing tab_index integer");
    return;
  }

  TabContents* tab_contents = browser->GetTabContentsAt(tab_index);

  if (tab_contents) {
    PersonalDataManager* pdm = tab_contents->profile()
        ->GetPersonalDataManager();
    if (pdm) {
      pdm->OnAutoFillDialogApply(profiles? &autofill_profiles : NULL,
                                 cards? &credit_cards : NULL);
    } else {
      reply.SendError("No PersonalDataManager.");
      return;
    }
  } else {
    reply.SendError("No tab at that index.");
    return;
  }
  reply.SendSuccess(NULL);
}

// Sample json output: { "success": true }
void TestingAutomationProvider::SignInToSync(Browser* browser,
                                             DictionaryValue* args,
                                             IPC::Message* reply_message) {
  AutomationJSONReply reply(this, reply_message);
  std::string username;
  std::string password;
  if (!args->GetString("username", &username) ||
      !args->GetString("password", &password)) {
      reply.SendError("Invalid or missing args");
      return;
  }
  if (sync_waiter_.get() == NULL) {
    sync_waiter_.reset(new ProfileSyncServiceHarness(
        browser->profile(), username, password, 0));
  } else {
    sync_waiter_->SetCredentials(username, password);
  }
  if (sync_waiter_->SetupSync()) {
    DictionaryValue* return_value = new DictionaryValue;
    return_value->SetBoolean("success", true);
    reply.SendSuccess(return_value);
  } else {
    reply.SendError("Signing in to sync was unsuccessful");
  }
}

// Sample json output:
// {u'summary': u'SYNC DISABLED'}
//
// { u'authenticated': True,
//   u'last synced': u'Just now',
//   u'summary': u'READY',
//   u'sync url': u'clients4.google.com',
//   u'synced datatypes': [ u'Bookmarks',
//                          u'Preferences',
//                          u'Passwords',
//                          u'Autofill',
//                          u'Themes',
//                          u'Extensions',
//                          u'Apps']}
void TestingAutomationProvider::GetSyncInfo(Browser* browser,
                                            DictionaryValue* args,
                                            IPC::Message* reply_message) {
  AutomationJSONReply reply(this, reply_message);
  DictionaryValue* sync_info = new DictionaryValue;
  DictionaryValue* return_value = new DictionaryValue;
  if (sync_waiter_.get() == NULL) {
    sync_waiter_.reset(
        ProfileSyncServiceHarness::CreateAndAttach(browser->profile()));
  }
  if (!sync_waiter_->IsSyncAlreadySetup()) {
    sync_info->SetString("summary", "SYNC DISABLED");
  } else {
    ProfileSyncService* service = sync_waiter_->service();
    ProfileSyncService::Status status = sync_waiter_->GetStatus();
    sync_info->SetString("summary",
        ProfileSyncService::BuildSyncStatusSummaryText(status.summary));
    sync_info->SetString("sync url", service->sync_service_url().host());
    sync_info->SetBoolean("authenticated", status.authenticated);
    sync_info->SetString("last synced", service->GetLastSyncedTimeString());
    ListValue* synced_datatype_list = new ListValue;
    syncable::ModelTypeSet synced_datatypes;
    service->GetPreferredDataTypes(&synced_datatypes);
    for (syncable::ModelTypeSet::iterator it = synced_datatypes.begin();
         it != synced_datatypes.end(); ++it) {
      synced_datatype_list->Append(
          new StringValue(syncable::ModelTypeToString(*it)));
    }
    sync_info->Set("synced datatypes", synced_datatype_list);
  }
  return_value->Set("sync_info", sync_info);
  reply.SendSuccess(return_value);
}

// Sample json output: { "success": true }
void TestingAutomationProvider::AwaitSyncCycleCompletion(
    Browser* browser,
    DictionaryValue* args,
    IPC::Message* reply_message) {
  AutomationJSONReply reply(this, reply_message);
  if (sync_waiter_.get() == NULL) {
    sync_waiter_.reset(
        ProfileSyncServiceHarness::CreateAndAttach(browser->profile()));
  }
  if (!sync_waiter_->IsSyncAlreadySetup()) {
    reply.SendError("Not signed in to sync");
    return;
  }
  sync_waiter_->AwaitSyncCycleCompletion("Waiting for sync cycle");
  ProfileSyncService::Status status = sync_waiter_->GetStatus();
  if (status.summary == ProfileSyncService::Status::READY) {
    scoped_ptr<DictionaryValue> return_value(new DictionaryValue);
    return_value->SetBoolean("success", true);
    reply.SendSuccess(return_value.get());
  } else {
    reply.SendError("Wait for sync cycle was unsuccessful");
  }
}

// Refer to EnableSyncForDatatypes() in chrome/test/pyautolib/pyauto.py for
// sample json input. Sample json output: { "success": true }
void TestingAutomationProvider::EnableSyncForDatatypes(
    Browser* browser,
    DictionaryValue* args,
    IPC::Message* reply_message) {
  AutomationJSONReply reply(this, reply_message);
  if (sync_waiter_.get() == NULL) {
    sync_waiter_.reset(
        ProfileSyncServiceHarness::CreateAndAttach(browser->profile()));
  }
  if (!sync_waiter_->IsSyncAlreadySetup()) {
    reply.SendError("Not signed in to sync");
    return;
  }
  ListValue* datatypes = NULL;
  if (!args->GetList("datatypes", &datatypes)) {
    reply.SendError("Invalid or missing args");
    return;
  }
  std::string first_datatype;
  datatypes->GetString(0, &first_datatype);
  if (first_datatype == "All") {
    sync_waiter_->EnableSyncForAllDatatypes();
  } else {
    int num_datatypes = datatypes->GetSize();
    for (int i = 0; i < num_datatypes; ++i) {
      std::string datatype_string;
      datatypes->GetString(i, &datatype_string);
      syncable::ModelType datatype =
          syncable::ModelTypeFromString(datatype_string);
      if (datatype == syncable::UNSPECIFIED) {
        AutomationJSONReply(this, reply_message).SendError(StringPrintf(
            "Invalid datatype string: %s.", datatype_string.c_str()));
        return;
      }
      sync_waiter_->EnableSyncForDatatype(datatype);
      sync_waiter_->AwaitSyncCycleCompletion(StringPrintf(
          "Enabling datatype: %s", datatype_string.c_str()));
    }
  }
  ProfileSyncService::Status status = sync_waiter_->GetStatus();
  if (status.summary == ProfileSyncService::Status::READY ||
      status.summary == ProfileSyncService::Status::SYNCING) {
    DictionaryValue* return_value = new DictionaryValue;
    return_value->SetBoolean("success", true);
    reply.SendSuccess(return_value);
  } else {
    reply.SendError("Enabling sync for given datatypes was unsuccessful");
  }
}

// Refer to DisableSyncForDatatypes() in chrome/test/pyautolib/pyauto.py for
// sample json input. Sample json output: { "success": true }
void TestingAutomationProvider::DisableSyncForDatatypes(
    Browser* browser,
    DictionaryValue* args,
    IPC::Message* reply_message) {
  AutomationJSONReply reply(this, reply_message);
  if (sync_waiter_.get() == NULL) {
    sync_waiter_.reset(
        ProfileSyncServiceHarness::CreateAndAttach(browser->profile()));
  }
  if (!sync_waiter_->IsSyncAlreadySetup()) {
    reply.SendError("Not signed in to sync");
    return;
  }
  ListValue* datatypes = NULL;
  if (!args->GetList("datatypes", &datatypes)) {
    reply.SendError("Invalid or missing args");
    return;
  }
  std::string first_datatype;
  if (!datatypes->GetString(0, &first_datatype)) {
    reply.SendError("Invalid or missing string");
    return;
  }
  if (first_datatype == "All") {
    sync_waiter_->DisableSyncForAllDatatypes();
    ProfileSyncService::Status status = sync_waiter_->GetStatus();
    if (status.summary != ProfileSyncService::Status::READY &&
        status.summary != ProfileSyncService::Status::SYNCING) {
      DictionaryValue* return_value = new DictionaryValue;
      return_value->SetBoolean("success", true);
      reply.SendSuccess(return_value);
    } else {
      reply.SendError("Disabling all sync datatypes was unsuccessful");
    }
  } else {
    int num_datatypes = datatypes->GetSize();
    for (int i = 0; i < num_datatypes; i++) {
      std::string datatype_string;
      datatypes->GetString(i, &datatype_string);
      syncable::ModelType datatype =
          syncable::ModelTypeFromString(datatype_string);
      if (datatype == syncable::UNSPECIFIED) {
        AutomationJSONReply(this, reply_message).SendError(StringPrintf(
            "Invalid datatype string: %s.", datatype_string.c_str()));
        return;
      }
      sync_waiter_->DisableSyncForDatatype(datatype);
      sync_waiter_->AwaitSyncCycleCompletion(StringPrintf(
          "Disabling datatype: %s", datatype_string.c_str()));
    }
    ProfileSyncService::Status status = sync_waiter_->GetStatus();
    if (status.summary == ProfileSyncService::Status::READY ||
        status.summary == ProfileSyncService::Status::SYNCING) {
      DictionaryValue* return_value = new DictionaryValue;
      return_value->SetBoolean("success", true);
      reply.SendSuccess(return_value);
    } else {
      reply.SendError("Disabling sync for given datatypes was unsuccessful");
    }
  }
}

/* static */
ListValue* TestingAutomationProvider::GetListFromAutoFillProfiles(
    const std::vector<AutoFillProfile*>& autofill_profiles) {
  ListValue* profiles = new ListValue;

  std::map<AutoFillFieldType, std::string> autofill_type_to_string
      = GetAutoFillFieldToStringMap();

  // For each AutoFillProfile, transform it to a dictionary object to return.
  for (std::vector<AutoFillProfile*>::const_iterator it =
           autofill_profiles.begin();
       it != autofill_profiles.end(); ++it) {
    AutoFillProfile* profile = *it;
    DictionaryValue* profile_info = new DictionaryValue;
    // For each of the types, if it has a value, add it to the dictionary.
    for (std::map<AutoFillFieldType, std::string>::iterator
         type_it = autofill_type_to_string.begin();
         type_it != autofill_type_to_string.end(); ++type_it) {
      string16 value = profile->GetFieldText(AutoFillType(type_it->first));
      if (value.length()) {  // If there was something stored for that value.
        profile_info->SetString(type_it->second, value);
      }
    }
    profiles->Append(profile_info);
  }
  return profiles;
}

/* static */
ListValue* TestingAutomationProvider::GetListFromCreditCards(
    const std::vector<CreditCard*>& credit_cards) {
  ListValue* cards = new ListValue;

  std::map<AutoFillFieldType, std::string> credit_card_type_to_string =
      GetCreditCardFieldToStringMap();

  // For each AutoFillProfile, transform it to a dictionary object to return.
  for (std::vector<CreditCard*>::const_iterator it =
           credit_cards.begin();
       it != credit_cards.end(); ++it) {
    CreditCard* card = *it;
    DictionaryValue* card_info = new DictionaryValue;
    // For each of the types, if it has a value, add it to the dictionary.
    for (std::map<AutoFillFieldType, std::string>::iterator type_it =
        credit_card_type_to_string.begin();
        type_it != credit_card_type_to_string.end(); ++type_it) {
      string16 value = card->GetFieldText(AutoFillType(type_it->first));
      // If there was something stored for that value.
      if (value.length()) {
        card_info->SetString(type_it->second, value);
      }
    }
    cards->Append(card_info);
  }
  return cards;
}

/* static */
std::vector<AutoFillProfile>
TestingAutomationProvider::GetAutoFillProfilesFromList(
    const ListValue& profiles, std::string* error_message) {
  std::vector<AutoFillProfile> autofill_profiles;
  DictionaryValue* profile_info = NULL;
  string16 current_value;

  std::map<AutoFillFieldType, std::string> autofill_type_to_string =
      GetAutoFillFieldToStringMap();

  int num_profiles = profiles.GetSize();
  for (int i = 0; i < num_profiles; i++) {
    profiles.GetDictionary(i, &profile_info);
    AutoFillProfile profile;
    // Loop through the possible profile types and add those provided.
    for (std::map<AutoFillFieldType, std::string>::iterator type_it =
         autofill_type_to_string.begin();
         type_it != autofill_type_to_string.end(); ++type_it) {
      if (profile_info->HasKey(type_it->second)) {
        if (profile_info->GetString(type_it->second,
                                    &current_value)) {
          profile.SetInfo(AutoFillType(type_it->first), current_value);
        } else {
          *error_message= "All values must be strings";
          break;
        }
      }
    }
    autofill_profiles.push_back(profile);
  }
  return autofill_profiles;
}

/* static */
std::vector<CreditCard> TestingAutomationProvider::GetCreditCardsFromList(
    const ListValue& cards, std::string* error_message) {
  std::vector<CreditCard> credit_cards;
  DictionaryValue* card_info = NULL;
  string16 current_value;

  std::map<AutoFillFieldType, std::string> credit_card_type_to_string =
      GetCreditCardFieldToStringMap();

  int num_credit_cards = cards.GetSize();
  for (int i = 0; i < num_credit_cards; i++) {
    cards.GetDictionary(i, &card_info);
    CreditCard card;
    // Loop through the possible credit card fields and add those provided.
    for (std::map<AutoFillFieldType, std::string>::iterator type_it =
        credit_card_type_to_string.begin();
        type_it != credit_card_type_to_string.end(); ++type_it) {
      if (card_info->HasKey(type_it->second)) {
        if (card_info->GetString(type_it->second, &current_value)) {
          card.SetInfo(AutoFillType(type_it->first), current_value);
        } else {
          *error_message= "All values must be strings";
          break;
        }
      }
    }
    credit_cards.push_back(card);
  }
  return credit_cards;
}

/* static */
std::map<AutoFillFieldType, std::string>
    TestingAutomationProvider::GetAutoFillFieldToStringMap() {
  std::map<AutoFillFieldType, std::string> autofill_type_to_string;
  autofill_type_to_string[NAME_FIRST] = "NAME_FIRST";
  autofill_type_to_string[NAME_MIDDLE] = "NAME_MIDDLE";
  autofill_type_to_string[NAME_LAST] = "NAME_LAST";
  autofill_type_to_string[COMPANY_NAME] = "COMPANY_NAME";
  autofill_type_to_string[EMAIL_ADDRESS] = "EMAIL_ADDRESS";
  autofill_type_to_string[ADDRESS_HOME_LINE1] = "ADDRESS_HOME_LINE1";
  autofill_type_to_string[ADDRESS_HOME_LINE2] = "ADDRESS_HOME_LINE2";
  autofill_type_to_string[ADDRESS_HOME_CITY] = "ADDRESS_HOME_CITY";
  autofill_type_to_string[ADDRESS_HOME_STATE] = "ADDRESS_HOME_STATE";
  autofill_type_to_string[ADDRESS_HOME_ZIP] = "ADDRESS_HOME_ZIP";
  autofill_type_to_string[ADDRESS_HOME_COUNTRY] = "ADDRESS_HOME_COUNTRY";
  autofill_type_to_string[PHONE_HOME_WHOLE_NUMBER] =
      "PHONE_HOME_WHOLE_NUMBER";
  autofill_type_to_string[PHONE_FAX_WHOLE_NUMBER] = "PHONE_FAX_WHOLE_NUMBER";
  autofill_type_to_string[NAME_FIRST] = "NAME_FIRST";
  return autofill_type_to_string;
}

/* static */
std::map<AutoFillFieldType, std::string>
    TestingAutomationProvider::GetCreditCardFieldToStringMap() {
  std::map<AutoFillFieldType, std::string> credit_card_type_to_string;
  credit_card_type_to_string[CREDIT_CARD_NAME] = "CREDIT_CARD_NAME";
  credit_card_type_to_string[CREDIT_CARD_NUMBER] = "CREDIT_CARD_NUMBER";
  credit_card_type_to_string[CREDIT_CARD_EXP_MONTH] = "CREDIT_CARD_EXP_MONTH";
  credit_card_type_to_string[CREDIT_CARD_EXP_4_DIGIT_YEAR] =
      "CREDIT_CARD_EXP_4_DIGIT_YEAR";
  return credit_card_type_to_string;
}

// Refer to GetActiveNotifications() in chrome/test/pyautolib/pyauto.py for
// sample json input/output.
void TestingAutomationProvider::GetActiveNotifications(
    Browser* browser,
    DictionaryValue* args,
    IPC::Message* reply_message) {
  new GetActiveNotificationsObserver(this, reply_message);
}

// Refer to CloseNotification() in chrome/test/pyautolib/pyauto.py for
// sample json input.
// Returns empty json message.
void TestingAutomationProvider::CloseNotification(
    Browser* browser,
    DictionaryValue* args,
    IPC::Message* reply_message) {
  int index;
  if (!args->GetInteger("index", &index)) {
    AutomationJSONReply(this, reply_message)
        .SendError("'index' missing or invalid.");
    return;
  }
  NotificationUIManager* manager = g_browser_process->notification_ui_manager();
  BalloonCollection* collection = manager->balloon_collection();
  const BalloonCollection::Balloons& balloons = collection->GetActiveBalloons();
  int balloon_count = static_cast<int>(balloons.size());
  if (index < 0 || index >= balloon_count) {
    AutomationJSONReply(this, reply_message)
        .SendError(StringPrintf("No notification at index %d", index));
    return;
  }
  // This will delete itself when finished.
  new OnNotificationBalloonCountObserver(
      this, reply_message, collection, balloon_count - 1);
  manager->CancelById(balloons[index]->notification().notification_id());
}

// Refer to WaitForNotificationCount() in chrome/test/pyautolib/pyauto.py for
// sample json input.
// Returns empty json message.
void TestingAutomationProvider::WaitForNotificationCount(
    Browser* browser,
    DictionaryValue* args,
    IPC::Message* reply_message) {
  int count;
  if (!args->GetInteger("count", &count)) {
    AutomationJSONReply(this, reply_message)
        .SendError("'count' missing or invalid.");
    return;
  }
  NotificationUIManager* manager = g_browser_process->notification_ui_manager();
  BalloonCollection* collection = manager->balloon_collection();
  const BalloonCollection::Balloons& balloons = collection->GetActiveBalloons();
  if (static_cast<int>(balloons.size()) == count) {
    AutomationJSONReply(this, reply_message).SendSuccess(NULL);
    return;
  }
  // This will delete itself when finished.
  new OnNotificationBalloonCountObserver(
      this, reply_message, collection, count);
}

// Sample JSON input: { "command": "GetNTPInfo" }
// For output, refer to chrome/test/pyautolib/ntp_model.py.
void TestingAutomationProvider::GetNTPInfo(
    Browser* browser,
    DictionaryValue* args,
    IPC::Message* reply_message) {
  // This observer will delete itself.
  new NTPInfoObserver(this, reply_message, &consumer_);
}

void TestingAutomationProvider::MoveNTPMostVisitedThumbnail(
    Browser* browser,
    DictionaryValue* args,
    IPC::Message* reply_message) {
  AutomationJSONReply reply(this, reply_message);
  std::string url, error;
  int index, old_index;
  if (!args->GetString("url", &url)) {
    reply.SendError("Missing or invalid 'url' key.");
    return;
  }
  if (!args->GetInteger("index", &index)) {
    reply.SendError("Missing or invalid 'index' key.");
    return;
  }
  if (!args->GetInteger("old_index", &old_index)) {
    reply.SendError("Missing or invalid 'old_index' key");
    return;
  }
  history::TopSites* top_sites = browser->profile()->GetTopSites();
  if (!top_sites) {
    reply.SendError("TopSites service is not initialized.");
    return;
  }
  GURL swapped;
  // If thumbnail A is switching positions with a pinned thumbnail B, B
  // should be pinned in the old index of A.
  if (top_sites->GetPinnedURLAtIndex(index, &swapped)) {
    top_sites->AddPinnedURL(swapped, old_index);
  }
  top_sites->AddPinnedURL(GURL(url), index);
  reply.SendSuccess(NULL);
}

void TestingAutomationProvider::RemoveNTPMostVisitedThumbnail(
    Browser* browser,
    DictionaryValue* args,
    IPC::Message* reply_message) {
  AutomationJSONReply reply(this, reply_message);
  std::string url;
  if (!args->GetString("url", &url)) {
    reply.SendError("Missing or invalid 'url' key.");
    return;
  }
  history::TopSites* top_sites = browser->profile()->GetTopSites();
  if (!top_sites) {
    reply.SendError("TopSites service is not initialized.");
    return;
  }
  top_sites->AddBlacklistedURL(GURL(url));
  reply.SendSuccess(NULL);
}

void TestingAutomationProvider::UnpinNTPMostVisitedThumbnail(
    Browser* browser,
    DictionaryValue* args,
    IPC::Message* reply_message) {
  AutomationJSONReply reply(this, reply_message);
  std::string url;
  if (!args->GetString("url", &url)) {
    reply.SendError("Missing or invalid 'url' key.");
    return;
  }
  history::TopSites* top_sites = browser->profile()->GetTopSites();
  if (!top_sites) {
    reply.SendError("TopSites service is not initialized.");
    return;
  }
  top_sites->RemovePinnedURL(GURL(url));
  reply.SendSuccess(NULL);
}

void TestingAutomationProvider::RestoreAllNTPMostVisitedThumbnails(
    Browser* browser,
    DictionaryValue* args,
    IPC::Message* reply_message) {
  AutomationJSONReply reply(this, reply_message);
  history::TopSites* top_sites = browser->profile()->GetTopSites();
  if (!top_sites) {
    reply.SendError("TopSites service is not initialized.");
    return;
  }
  top_sites->ClearBlacklistedURLs();
  reply.SendSuccess(NULL);
}

void TestingAutomationProvider::KillRendererProcess(
    Browser* browser,
    DictionaryValue* args,
    IPC::Message* reply_message) {
  int pid;
  if (!args->GetInteger("pid", &pid)) {
    AutomationJSONReply(this, reply_message)
        .SendError("'pid' key missing or invalid.");
    return;
  }
  base::ProcessHandle process;
  if (!base::OpenProcessHandle(static_cast<base::ProcessId>(pid), &process)) {
    AutomationJSONReply(this, reply_message).SendError(base::StringPrintf(
        "Failed to open process handle for pid %d", pid));
    return;
  }
  new RendererProcessClosedObserver(this, reply_message);
  base::KillProcess(process, 0, false);
  base::CloseProcessHandle(process);
}

void TestingAutomationProvider::SendKeyEventToActiveTab(
    Browser* browser,
    DictionaryValue* args,
    IPC::Message* reply_message) {
  int type, modifiers;
  bool is_system_key;
  string16 unmodified_text, text;
  std::string key_identifier;
  NativeWebKeyboardEvent event;
  if (!args->GetInteger("type", &type)) {
    AutomationJSONReply reply(this, reply_message);
    reply.SendError("'type' missing or invalid.");
    return;
  }
  if (!args->GetBoolean("isSystemKey", &is_system_key)) {
    AutomationJSONReply reply(this, reply_message);
    reply.SendError("'isSystemKey' missing or invalid.");
    return;
  }
  if (!args->GetString("unmodifiedText", &unmodified_text)) {
    AutomationJSONReply reply(this, reply_message);
    reply.SendError("'unmodifiedText' missing or invalid.");
    return;
  }
  if (!args->GetString("text", &text)) {
    AutomationJSONReply reply(this, reply_message);
    reply.SendError("'text' missing or invalid.");
    return;
  }
  if (!args->GetInteger("nativeKeyCode", &event.nativeKeyCode)) {
    AutomationJSONReply reply(this, reply_message);
    reply.SendError("'nativeKeyCode' missing or invalid.");
    return;
  }
  if (!args->GetInteger("windowsKeyCode", &event.windowsKeyCode)) {
    AutomationJSONReply reply(this, reply_message);
    reply.SendError("'windowsKeyCode' missing or invalid.");
    return;
  }
  if (!args->GetInteger("modifiers", &modifiers)) {
    AutomationJSONReply reply(this, reply_message);
    reply.SendError("'modifiers' missing or invalid.");
    return;
  }
  if (args->GetString("keyIdentifier", &key_identifier)) {
    base::strlcpy(event.keyIdentifier,
                  key_identifier.c_str(),
                  WebKit::WebKeyboardEvent::keyIdentifierLengthCap);
  } else {
    event.setKeyIdentifierFromWindowsKeyCode();
  }

  if (type == automation::kRawKeyDownType) {
    event.type = WebKit::WebInputEvent::RawKeyDown;
  } else if (type == automation::kKeyDownType) {
    event.type = WebKit::WebInputEvent::KeyDown;
  } else if (type == automation::kKeyUpType) {
    event.type = WebKit::WebInputEvent::KeyUp;
  } else if (type == automation::kCharType) {
    event.type = WebKit::WebInputEvent::Char;
  } else {
    AutomationJSONReply reply(this, reply_message);
    reply.SendError("'type' refers to an unrecognized keyboard event type");
    return;
  }

  string16 unmodified_text_truncated = unmodified_text.substr(
      0, WebKit::WebKeyboardEvent::textLengthCap - 1);
  memcpy(event.unmodifiedText,
         unmodified_text_truncated.c_str(),
         unmodified_text_truncated.length() + 1);
  string16 text_truncated = text.substr(
      0, WebKit::WebKeyboardEvent::textLengthCap - 1);
  memcpy(event.text, text_truncated.c_str(), text_truncated.length() + 1);

  event.modifiers = 0;
  if (modifiers & automation::kShiftKeyMask)
    event.modifiers |= WebKit::WebInputEvent::ShiftKey;
  if (modifiers & automation::kControlKeyMask)
    event.modifiers |= WebKit::WebInputEvent::ControlKey;
  if (modifiers & automation::kAltKeyMask)
    event.modifiers |= WebKit::WebInputEvent::AltKey;
  if (modifiers & automation::kMetaKeyMask)
    event.modifiers |= WebKit::WebInputEvent::MetaKey;

  event.isSystemKey = is_system_key;
  event.timeStampSeconds = base::Time::Now().ToDoubleT();
  event.skip_in_browser = true;
  new InputEventAckNotificationObserver(this, reply_message, event.type);
  browser->GetSelectedTabContents()->render_view_host()->
    ForwardKeyboardEvent(event);
}

void TestingAutomationProvider::WaitForTabCountToBecome(
    int browser_handle,
    int target_tab_count,
    IPC::Message* reply_message) {
  if (!browser_tracker_->ContainsHandle(browser_handle)) {
    AutomationMsg_WaitForTabCountToBecome::WriteReplyParams(reply_message,
                                                            false);
    Send(reply_message);
    return;
  }

  Browser* browser = browser_tracker_->GetResource(browser_handle);

  // The observer will delete itself.
  new TabCountChangeObserver(this, browser, reply_message, target_tab_count);
}

void TestingAutomationProvider::WaitForInfoBarCount(
    int tab_handle,
    size_t target_count,
    IPC::Message* reply_message) {
  if (!tab_tracker_->ContainsHandle(tab_handle)) {
    AutomationMsg_WaitForInfoBarCount::WriteReplyParams(reply_message_, false);
    Send(reply_message_);
    return;
  }

  NavigationController* controller = tab_tracker_->GetResource(tab_handle);
  if (!controller) {
    AutomationMsg_WaitForInfoBarCount::WriteReplyParams(reply_message_, false);
    Send(reply_message_);
    return;
  }

  // The delegate will delete itself.
  new InfoBarCountObserver(this, reply_message, controller->tab_contents(),
                           target_count);
}

// Gets the current used encoding name of the page in the specified tab.
void TestingAutomationProvider::GetPageCurrentEncoding(
    int tab_handle, std::string* current_encoding) {
  if (tab_tracker_->ContainsHandle(tab_handle)) {
    NavigationController* nav = tab_tracker_->GetResource(tab_handle);
    Browser* browser = FindAndActivateTab(nav);
    DCHECK(browser);

    if (browser->command_updater()->IsCommandEnabled(IDC_ENCODING_MENU))
      *current_encoding = nav->tab_contents()->encoding();
  }
}

void TestingAutomationProvider::ShutdownSessionService(int handle,
                                                       bool* result) {
  if (browser_tracker_->ContainsHandle(handle)) {
    Browser* browser = browser_tracker_->GetResource(handle);
    browser->profile()->ShutdownSessionService();
    *result = true;
  } else {
    *result = false;
  }
}

void TestingAutomationProvider::SetContentSetting(
    int handle,
    const std::string& host,
    ContentSettingsType content_type,
    ContentSetting setting,
    bool* success) {
  *success = false;
  if (browser_tracker_->ContainsHandle(handle)) {
    Browser* browser = browser_tracker_->GetResource(handle);
    HostContentSettingsMap* map =
        browser->profile()->GetHostContentSettingsMap();
    if (host.empty()) {
      map->SetDefaultContentSetting(content_type, setting);
    } else {
      map->SetContentSetting(ContentSettingsPattern(host),
                             content_type, "", setting);
    }
    *success = true;
  }
}

void TestingAutomationProvider::LoadBlockedPlugins(int tab_handle,
                                                   bool* success) {
  *success = false;
  if (tab_tracker_->ContainsHandle(tab_handle)) {
    NavigationController* nav = tab_tracker_->GetResource(tab_handle);
    if (!nav)
      return;
    TabContents* contents = nav->tab_contents();
    if (!contents)
      return;
    contents->render_view_host()->LoadBlockedPlugins();
    *success = true;
  }
}

void TestingAutomationProvider::ResetToDefaultTheme() {
  profile_->ClearTheme();
}

void TestingAutomationProvider::WaitForProcessLauncherThreadToGoIdle(
    IPC::Message* reply_message) {
  new WaitForProcessLauncherThreadToGoIdleObserver(this, reply_message);
}

void TestingAutomationProvider::GetParentBrowserOfTab(int tab_handle,
                                                      int* browser_handle,
                                                      bool* success) {
  *success = false;
  if (tab_tracker_->ContainsHandle(tab_handle)) {
    NavigationController* controller = tab_tracker_->GetResource(tab_handle);
    int index;
    Browser* browser = Browser::GetBrowserForController(controller, &index);
    if (browser) {
      *browser_handle = browser_tracker_->Add(browser);
      *success = true;
    }
  }
}

// TODO(brettw) change this to accept GURLs when history supports it
void TestingAutomationProvider::OnRedirectQueryComplete(
    HistoryService::Handle request_handle,
    GURL from_url,
    bool success,
    history::RedirectList* redirects) {
  DCHECK_EQ(redirect_query_, request_handle);
  DCHECK(reply_message_ != NULL);

  std::vector<GURL> redirects_gurl;
  reply_message_->WriteBool(success);
  if (success) {
    for (size_t i = 0; i < redirects->size(); i++)
      redirects_gurl.push_back(redirects->at(i));
  }

  IPC::ParamTraits<std::vector<GURL> >::Write(reply_message_, redirects_gurl);

  Send(reply_message_);
  redirect_query_ = 0;
  reply_message_ = NULL;
}

void TestingAutomationProvider::OnBrowserAdded(const Browser* browser) {
}

void TestingAutomationProvider::OnBrowserRemoved(const Browser* browser) {
  // For backwards compatibility with the testing automation interface, we
  // want the automation provider (and hence the process) to go away when the
  // last browser goes away.
  if (BrowserList::empty() && !CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kKeepAliveForTest)) {
    // If you change this, update Observer for NotificationType::SESSION_END
    // below.
    MessageLoop::current()->PostTask(FROM_HERE,
        NewRunnableMethod(this, &TestingAutomationProvider::OnRemoveProvider));
  }
}

void TestingAutomationProvider::OnRemoveProvider() {
  AutomationProviderList::GetInstance()->RemoveProvider(this);
}
