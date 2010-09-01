// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/automation/testing_automation_provider.h"

#include "app/message_box_flags.h"
#include "base/command_line.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "chrome/app/chrome_dll_resource.h"
#include "chrome/browser/app_modal_dialog.h"
#include "chrome/browser/app_modal_dialog_queue.h"
#include "chrome/browser/autocomplete/autocomplete_edit.h"
#include "chrome/browser/automation/automation_autocomplete_edit_tracker.h"
#include "chrome/browser/automation/automation_browser_tracker.h"
#include "chrome/browser/automation/automation_provider_list.h"
#include "chrome/browser/automation/automation_provider_observers.h"
#include "chrome/browser/automation/automation_tab_tracker.h"
#include "chrome/browser/automation/automation_window_tracker.h"
#include "chrome/browser/automation/ui_controls.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/bookmarks/bookmark_storage.h"
#include "chrome/browser/blocked_popup_container.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_window.h"
#include "chrome/browser/debugger/devtools_manager.h"
#include "chrome/browser/download/download_prefs.h"
#include "chrome/browser/download/download_shelf.h"
#include "chrome/browser/find_bar.h"
#include "chrome/browser/location_bar.h"
#include "chrome/browser/login_prompt.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/renderer_host/render_process_host.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/tab_contents/infobar_delegate.h"
#include "chrome/browser/tab_contents/interstitial_page.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/net/url_request_context_getter.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/automation/automation_messages.h"
#include "net/base/cookie_store.h"
#include "net/url_request/url_request_context.h"
#include "views/event.h"

namespace {

class GetCookiesTask : public Task {
 public:
  GetCookiesTask(const GURL& url,
                 URLRequestContextGetter* context_getter,
                 base::WaitableEvent* event,
                 std::string* cookies)
      : url_(url),
        context_getter_(context_getter),
        event_(event),
        cookies_(cookies) {}

  virtual void Run() {
    *cookies_ = context_getter_->GetCookieStore()->GetCookies(url_);
    event_->Signal();
  }

 private:
  const GURL& url_;
  URLRequestContextGetter* const context_getter_;
  base::WaitableEvent* const event_;
  std::string* const cookies_;

  DISALLOW_COPY_AND_ASSIGN(GetCookiesTask);
};

std::string GetCookiesForURL(
    const GURL& url,
    URLRequestContextGetter* context_getter) {
  std::string cookies;
  base::WaitableEvent event(true /* manual reset */,
                            false /* not initially signaled */);
  CHECK(ChromeThread::PostTask(
      ChromeThread::IO, FROM_HERE,
      new GetCookiesTask(url, context_getter, &event, &cookies)));
  event.Wait();
  return cookies;
}

class SetCookieTask : public Task {
 public:
  SetCookieTask(const GURL& url,
                const std::string& value,
                URLRequestContextGetter* context_getter,
                base::WaitableEvent* event,
                bool* rv)
      : url_(url),
        value_(value),
        context_getter_(context_getter),
        event_(event),
        rv_(rv) {}

  virtual void Run() {
    *rv_ = context_getter_->GetCookieStore()->SetCookie(url_, value_);
    event_->Signal();
  }

 private:
  const GURL& url_;
  const std::string& value_;
  URLRequestContextGetter* const context_getter_;
  base::WaitableEvent* const event_;
  bool* const rv_;

  DISALLOW_COPY_AND_ASSIGN(SetCookieTask);
};

bool SetCookieForURL(
    const GURL& url,
    const std::string& value,
    URLRequestContextGetter* context_getter) {
  base::WaitableEvent event(true /* manual reset */,
                            false /* not initially signaled */);
  bool rv = false;
  CHECK(ChromeThread::PostTask(
      ChromeThread::IO, FROM_HERE,
      new SetCookieTask(url, value, context_getter, &event, &rv)));
  event.Wait();
  return rv;
}

class DeleteCookieTask : public Task {
 public:
  DeleteCookieTask(const GURL& url,
                   const std::string& name,
                   const scoped_refptr<URLRequestContextGetter>& context_getter)
      : url_(url),
        name_(name),
        context_getter_(context_getter) {}

  virtual void Run() {
    net::CookieStore* cookie_store = context_getter_->GetCookieStore();
    cookie_store->DeleteCookie(url_, name_);
  }

 private:
  const GURL url_;
  const std::string name_;
  const scoped_refptr<URLRequestContextGetter> context_getter_;

  DISALLOW_COPY_AND_ASSIGN(DeleteCookieTask);
};

class ClickTask : public Task {
 public:
  explicit ClickTask(int flags) : flags_(flags) {}
  virtual ~ClickTask() {}

  virtual void Run() {
    ui_controls::MouseButton button = ui_controls::LEFT;
    if ((flags_ & views::Event::EF_LEFT_BUTTON_DOWN) ==
        views::Event::EF_LEFT_BUTTON_DOWN) {
      button = ui_controls::LEFT;
    } else if ((flags_ & views::Event::EF_RIGHT_BUTTON_DOWN) ==
        views::Event::EF_RIGHT_BUTTON_DOWN) {
      button = ui_controls::RIGHT;
    } else if ((flags_ & views::Event::EF_MIDDLE_BUTTON_DOWN) ==
        views::Event::EF_MIDDLE_BUTTON_DOWN) {
      button = ui_controls::MIDDLE;
    } else {
      NOTREACHED();
    }

    ui_controls::SendMouseClick(button);
  }

 private:
  int flags_;

  DISALLOW_COPY_AND_ASSIGN(ClickTask);
};

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
  std::string contents_;

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

void TestingAutomationProvider::OnMessageReceived(
    const IPC::Message& message) {
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
    IPC_MESSAGE_HANDLER_DELAY_REPLY(AutomationMsg_NavigateToURL, NavigateToURL)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(
        AutomationMsg_NavigateToURLBlockUntilNavigationsComplete,
        NavigateToURLBlockUntilNavigationsComplete)
    IPC_MESSAGE_HANDLER(AutomationMsg_NavigationAsync, NavigationAsync)
    IPC_MESSAGE_HANDLER(AutomationMsg_NavigationAsyncWithDisposition,
                        NavigationAsyncWithDisposition)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(AutomationMsg_GoBack, GoBack)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(AutomationMsg_GoForward, GoForward)
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
    IPC_MESSAGE_HANDLER(AutomationMsg_ApplyAccelerator, ApplyAccelerator)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(AutomationMsg_DomOperation,
                                    ExecuteJavascript)
    IPC_MESSAGE_HANDLER(AutomationMsg_ConstrainedWindowCount,
                        GetConstrainedWindowCount)
    IPC_MESSAGE_HANDLER(AutomationMsg_FindInPage, HandleFindInPageRequest)
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
    IPC_MESSAGE_HANDLER_DELAY_REPLY(AutomationMsg_OpenNewBrowserWindow,
                                    OpenNewBrowserWindow)
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

    IPC_MESSAGE_UNHANDLED(AutomationProvider::OnMessageReceived(message));
  IPC_END_MESSAGE_MAP()
}

void TestingAutomationProvider::OnChannelError() {
  BrowserList::CloseAllBrowsersAndExit();
  AutomationProvider::OnChannelError();
}

void TestingAutomationProvider::CloseBrowser(int browser_handle,
                                             IPC::Message* reply_message) {
  if (browser_tracker_->ContainsHandle(browser_handle)) {
    Browser* browser = browser_tracker_->GetResource(browser_handle);
    new BrowserClosedNotificationObserver(browser, this,
                                          reply_message);
    browser->window()->Close();
  } else {
    NOTREACHED();
  }
}

void TestingAutomationProvider::CloseBrowserAsync(int browser_handle) {
  if (browser_tracker_->ContainsHandle(browser_handle)) {
    Browser* browser = browser_tracker_->GetResource(browser_handle);
    browser->window()->Close();
  } else {
    NOTREACHED();
  }
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

void TestingAutomationProvider::AppendTab(int handle, const GURL& url,
                                          IPC::Message* reply_message) {
  int append_tab_response = -1;  // -1 is the error code
  NotificationObserver* observer = NULL;

  if (browser_tracker_->ContainsHandle(handle)) {
    Browser* browser = browser_tracker_->GetResource(handle);
    observer = AddTabStripObserver(browser, reply_message);
    TabContents* tab_contents = browser->AddTabWithURL(
        url, GURL(), PageTransition::TYPED, -1, TabStripModel::ADD_SELECTED,
        NULL, std::string(), &browser);
    if (tab_contents) {
      append_tab_response =
          GetIndexForNavigationController(&tab_contents->controller(), browser);
    }
  }

  if (append_tab_response < 0) {
    // The append tab failed. Remove the TabStripObserver
    if (observer) {
      RemoveTabStripObserver(observer);
      delete observer;
    }

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
    NavigationController* tab = tab_tracker_->GetResource(handle);

    // Since we are running on the UI thread don't call GetURLRequestContext().
    *value = GetCookiesForURL(url, tab->profile()->GetRequestContext());
    *value_size = static_cast<int>(value->size());
  }
}

void TestingAutomationProvider::SetCookie(const GURL& url,
                                          const std::string value,
                                          int handle,
                                          int* response_value) {
  *response_value = -1;

  if (url.is_valid() && tab_tracker_->ContainsHandle(handle)) {
    NavigationController* tab = tab_tracker_->GetResource(handle);

    if (SetCookieForURL(url, value, tab->profile()->GetRequestContext()))
      *response_value = 1;
  }
}

void TestingAutomationProvider::DeleteCookie(const GURL& url,
                                             const std::string& cookie_name,
                                             int handle, bool* success) {
  *success = false;
  if (url.is_valid() && tab_tracker_->ContainsHandle(handle)) {
    NavigationController* tab = tab_tracker_->GetResource(handle);
    ChromeThread::PostTask(
        ChromeThread::IO, FROM_HERE,
        new DeleteCookieTask(url, cookie_name,
                             tab->profile()->GetRequestContext()));
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

void TestingAutomationProvider::NavigateToURL(int handle,
                                              const GURL& url,
                                              IPC::Message* reply_message) {
  NavigateToURLBlockUntilNavigationsComplete(handle, url, 1, reply_message);
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
      AddNavigationStatusListener(tab, reply_message, number_of_navigations,
                                  false);

      // TODO(darin): avoid conversion to GURL
      browser->OpenURL(url, GURL(), CURRENT_TAB, PageTransition::TYPED);
      return;
    }
  }

  AutomationMsg_NavigateToURL::WriteReplyParams(
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

void TestingAutomationProvider::GoBack(int handle,
                                       IPC::Message* reply_message) {
  if (tab_tracker_->ContainsHandle(handle)) {
    NavigationController* tab = tab_tracker_->GetResource(handle);
    Browser* browser = FindAndActivateTab(tab);
    if (browser && browser->command_updater()->IsCommandEnabled(IDC_BACK)) {
      AddNavigationStatusListener(tab, reply_message, 1, false);
      browser->GoBack(CURRENT_TAB);
      return;
    }
  }

  AutomationMsg_GoBack::WriteReplyParams(
      reply_message, AUTOMATION_MSG_NAVIGATION_ERROR);
  Send(reply_message);
}

void TestingAutomationProvider::GoForward(int handle,
                                          IPC::Message* reply_message) {
  if (tab_tracker_->ContainsHandle(handle)) {
    NavigationController* tab = tab_tracker_->GetResource(handle);
    Browser* browser = FindAndActivateTab(tab);
    if (browser && browser->command_updater()->IsCommandEnabled(IDC_FORWARD)) {
      AddNavigationStatusListener(tab, reply_message, 1, false);
      browser->GoForward(CURRENT_TAB);
      return;
    }
  }

  AutomationMsg_GoForward::WriteReplyParams(
      reply_message, AUTOMATION_MSG_NAVIGATION_ERROR);
  Send(reply_message);
}

void TestingAutomationProvider::Reload(int handle,
                                       IPC::Message* reply_message) {
  if (tab_tracker_->ContainsHandle(handle)) {
    NavigationController* tab = tab_tracker_->GetResource(handle);
    Browser* browser = FindAndActivateTab(tab);
    if (browser && browser->command_updater()->IsCommandEnabled(IDC_RELOAD)) {
      AddNavigationStatusListener(tab, reply_message, 1, false);
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
      AddNavigationStatusListener(tab, reply_message, 1, false);
      handler->SetAuth(username, password);
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
      AddNavigationStatusListener(tab, reply_message, 1, false);
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
  DCHECK(!redirect_query_) << "Can only handle one redirect query at once.";
  if (tab_tracker_->ContainsHandle(tab_handle)) {
    NavigationController* tab = tab_tracker_->GetResource(tab_handle);
    HistoryService* history_service =
        tab->profile()->GetHistoryService(Profile::EXPLICIT_ACCESS);

    DCHECK(history_service) << "Tab " << tab_handle << "'s profile " <<
                               "has no history service";
    if (history_service) {
      DCHECK(reply_message_ == NULL);
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
  gfx::NativeWindow window =
      BrowserList::GetLastActive()->window()->GetNativeHandle();
  *handle = window_tracker_->Add(window);
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
  DCHECK(g_browser_process);
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
    ui_controls::SendMouseMoveNotifyWhenDone(click.x(), click.y(),
                                             new ClickTask(flags));
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
  ui_controls::SendKeyPress(window, static_cast<base::KeyboardCode>(key),
                           ((flags & views::Event::EF_CONTROL_DOWN) ==
                              views::Event::EF_CONTROL_DOWN),
                            ((flags & views::Event::EF_SHIFT_DOWN) ==
                              views::Event::EF_SHIFT_DOWN),
                            ((flags & views::Event::EF_ALT_DOWN) ==
                              views::Event::EF_ALT_DOWN),
                            ((flags & views::Event::EF_COMMAND_DOWN) ==
                              views::Event::EF_COMMAND_DOWN));
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
      TabContents* tab_contents =
          browser->GetTabContentsAt(tab_index);
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
    std::wstring* text) {
  *success = false;
  if (autocomplete_edit_tracker_->ContainsHandle(autocomplete_edit_handle)) {
    *text = autocomplete_edit_tracker_->GetResource(autocomplete_edit_handle)->
        GetText();
    *success = true;
  }
}

void TestingAutomationProvider::SetAutocompleteEditText(
    int autocomplete_edit_handle,
    const std::wstring& text,
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
    *query_in_progress = autocomplete_edit_tracker_->
        GetResource(autocomplete_edit_handle)->model()->query_in_progress();
    *success = true;
  }
}

void TestingAutomationProvider::ApplyAccelerator(int handle, int id) {
  LOG(ERROR) << "ApplyAccelerator has been deprecated. "
             << "Please use ExecuteBrowserCommandAsync instead.";
}

void TestingAutomationProvider::ExecuteJavascript(
    int handle,
    const std::wstring& frame_xpath,
    const std::wstring& script,
    IPC::Message* reply_message) {
  bool succeeded = false;
  TabContents* tab_contents = GetTabContentsForHandle(handle, NULL);
  if (tab_contents) {
    // Set the routing id of this message with the controller.
    // This routing id needs to be remembered for the reverse
    // communication while sending back the response of
    // this javascript execution.
    std::wstring set_automation_id;
    SStringPrintf(&set_automation_id,
      L"window.domAutomationController.setAutomationId(%d);",
      reply_message->routing_id());

    DCHECK(reply_message_ == NULL);
    reply_message_ = reply_message;

    tab_contents->render_view_host()->ExecuteJavascriptInWebFrame(
        frame_xpath, set_automation_id);
    tab_contents->render_view_host()->ExecuteJavascriptInWebFrame(
        frame_xpath, script);
    succeeded = true;
  }

  if (!succeeded) {
    AutomationMsg_DomOperation::WriteReplyParams(reply_message, std::string());
    Send(reply_message);
  }
}

void TestingAutomationProvider::GetConstrainedWindowCount(int handle,
                                                          int* count) {
  *count = -1;  // -1 is the error code
  if (tab_tracker_->ContainsHandle(handle)) {
      NavigationController* nav_controller = tab_tracker_->GetResource(handle);
      TabContents* tab_contents = nav_controller->tab_contents();
      if (tab_contents) {
        *count = static_cast<int>(tab_contents->child_windows_.size());
      }
  }
}

void TestingAutomationProvider::HandleFindInPageRequest(
    int handle,
    const std::wstring& find_request,
    int forward,
    int match_case,
    int* active_ordinal,
    int* matches_found) {
  LOG(ERROR) << "HandleFindInPageRequest has been deprecated."
             << "Please use HandleFindRequest instead.";
  *matches_found = -1;
}

void TestingAutomationProvider::HandleInspectElementRequest(
    int handle, int x, int y, IPC::Message* reply_message) {
  TabContents* tab_contents = GetTabContentsForHandle(handle, NULL);
  if (tab_contents) {
    DCHECK(reply_message_ == NULL);
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

void TestingAutomationProvider::OpenNewBrowserWindow(
    bool show, IPC::Message* reply_message) {
  OpenNewBrowserWindowOfType(static_cast<int>(Browser::TYPE_NORMAL), show,
                             reply_message);
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

    AddNavigationStatusListener(controller, reply_message, 1, false);
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
    NavigationEntry::PageType* page_type) {
  if (tab_tracker_->ContainsHandle(handle)) {
    NavigationController* tab = tab_tracker_->GetResource(handle);
    NavigationEntry* entry = tab->GetActiveEntry();
    *page_type = entry->page_type();
    *success = true;
    // In order to return the proper result when an interstitial is shown and
    // no navigation entry were created for it we need to ask the TabContents.
    if (*page_type == NavigationEntry::NORMAL_PAGE &&
        tab->tab_contents()->showing_interstitial_page())
      *page_type = NavigationEntry::INTERSTITIAL_PAGE;
  } else {
    *success = false;
    *page_type = NavigationEntry::NORMAL_PAGE;
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
    if (entry->page_type() == NavigationEntry::INTERSTITIAL_PAGE) {
      TabContents* tab_contents = tab->tab_contents();
      InterstitialPage* ssl_blocking_page =
          InterstitialPage::GetInterstitialPage(tab_contents);
      if (ssl_blocking_page) {
        if (proceed) {
          AddNavigationStatusListener(tab, reply_message, 1, false);
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
  if (browser_tracker_->ContainsHandle(browser_handle)) {
    Browser* browser = browser_tracker_->GetResource(browser_handle);
    browser->window()->Activate();
    *success = true;
  } else {
    *success = false;
  }
}

void TestingAutomationProvider::IsMenuCommandEnabled(int browser_handle,
                                                     int message_num,
                                                     bool* menu_item_enabled) {
  if (browser_tracker_->ContainsHandle(browser_handle)) {
    Browser* browser = browser_tracker_->GetResource(browser_handle);
    *menu_item_enabled =
        browser->command_updater()->IsCommandEnabled(message_num);
  } else {
    *menu_item_enabled = false;
  }
}

void TestingAutomationProvider::PrintNow(int tab_handle,
                                         IPC::Message* reply_message) {
  NavigationController* tab = NULL;
  TabContents* tab_contents = GetTabContentsForHandle(tab_handle, &tab);
  if (tab_contents) {
    FindAndActivateTab(tab);
    notification_observer_list_.AddObserver(
        new DocumentPrintedNotificationObserver(this, reply_message));
    if (tab_contents->PrintNow())
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
  if (!tab_tracker_->ContainsHandle(tab_handle)) {
    *success = false;
    return;
  }

  NavigationController* nav = tab_tracker_->GetResource(tab_handle);
  Browser* browser = FindAndActivateTab(nav);
  DCHECK(browser);
  if (!browser->command_updater()->IsCommandEnabled(IDC_SAVE_PAGE)) {
    *success = false;
    return;
  }

  SavePackage::SavePackageType save_type =
      static_cast<SavePackage::SavePackageType>(type);
  DCHECK(save_type >= SavePackage::SAVE_AS_ONLY_HTML &&
         save_type <= SavePackage::SAVE_AS_COMPLETE_HTML);
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
      scoped_refptr<BookmarkStorage> storage = new BookmarkStorage(
          browser->profile(),
          browser->profile()->GetBookmarkModel());
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

void TestingAutomationProvider::GetInfoBarCount(int handle, int* count) {
  *count = -1;  // -1 means error.
  if (tab_tracker_->ContainsHandle(handle)) {
    NavigationController* nav_controller = tab_tracker_->GetResource(handle);
    if (nav_controller)
      *count = nav_controller->tab_contents()->infobar_delegate_count();
  }
}

void TestingAutomationProvider::ClickInfoBarAccept(
    int handle,
    int info_bar_index,
    bool wait_for_navigation,
    IPC::Message* reply_message) {
  bool success = false;
  if (tab_tracker_->ContainsHandle(handle)) {
    NavigationController* nav_controller = tab_tracker_->GetResource(handle);
    if (nav_controller) {
      int count = nav_controller->tab_contents()->infobar_delegate_count();
      if (info_bar_index >= 0 && info_bar_index < count) {
        if (wait_for_navigation) {
          AddNavigationStatusListener(nav_controller, reply_message, 1, false);
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

  AddNavigationStatusListener(controller, reply_message, 1, true);
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
  AppModalDialog* dialog_delegate =
      Singleton<AppModalDialogQueue>()->active_dialog();
  *showing_dialog = (dialog_delegate != NULL);
  if (*showing_dialog)
    *dialog_button = dialog_delegate->GetDialogButtons();
  else
    *dialog_button = MessageBoxFlags::DIALOGBUTTON_NONE;
}

void TestingAutomationProvider::ClickAppModalDialogButton(int button,
                                                          bool* success) {
  *success = false;

  AppModalDialog* dialog_delegate =
      Singleton<AppModalDialogQueue>()->active_dialog();
  if (dialog_delegate &&
      (dialog_delegate->GetDialogButtons() & button) == button) {
    if ((button & MessageBoxFlags::DIALOGBUTTON_OK) ==
        MessageBoxFlags::DIALOGBUTTON_OK) {
      dialog_delegate->AcceptWindow();
      *success =  true;
    }
    if ((button & MessageBoxFlags::DIALOGBUTTON_CANCEL) ==
        MessageBoxFlags::DIALOGBUTTON_CANCEL) {
      DCHECK(!*success) << "invalid param, OK and CANCEL specified";
      dialog_delegate->CancelWindow();
      *success =  true;
    }
  }
}

void TestingAutomationProvider::WaitForBrowserWindowCountToBecome(
    int target_count, IPC::Message* reply_message) {
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
  if (Singleton<AppModalDialogQueue>()->HasActiveDialog()) {
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
      AddNavigationStatusListener(tab, reply_message, number_of_navigations,
                                  false);
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
      AddNavigationStatusListener(tab, reply_message, number_of_navigations,
                                  false);
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
        BlockedPopupContainer* container =
            tab_contents->blocked_popup_container();
        if (container) {
          *count = static_cast<int>(container->GetBlockedPopupCount());
        } else {
          // If we don't have a container, we don't have any blocked popups to
          // contain!
          *count = 0;
        }
      }
  }
}

// TODO(brettw) change this to accept GURLs when history supports it
void TestingAutomationProvider::OnRedirectQueryComplete(
    HistoryService::Handle request_handle,
    GURL from_url,
    bool success,
    history::RedirectList* redirects) {
  DCHECK(request_handle == redirect_query_);
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

void TestingAutomationProvider::OnBrowserRemoving(const Browser* browser) {
  // For backwards compatibility with the testing automation interface, we
  // want the automation provider (and hence the process) to go away when the
  // last browser goes away.
  if (BrowserList::size() == 1 && !CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kKeepAliveForTest)) {
    // If you change this, update Observer for NotificationType::SESSION_END
    // below.
    MessageLoop::current()->PostTask(FROM_HERE,
        NewRunnableMethod(this, &TestingAutomationProvider::OnRemoveProvider));
  }
}

void TestingAutomationProvider::Observe(NotificationType type,
                                        const NotificationSource& source,
                                        const NotificationDetails& details) {
  DCHECK(type == NotificationType::SESSION_END);
  // OnBrowserRemoving does a ReleaseLater. When session end is received we exit
  // before the task runs resulting in this object not being deleted. This
  // Release balance out the Release scheduled by OnBrowserRemoving.
  Release();
}

void TestingAutomationProvider::OnRemoveProvider() {
  AutomationProviderList::GetInstance()->RemoveProvider(this);
}
