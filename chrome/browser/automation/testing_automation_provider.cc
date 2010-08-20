// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/automation/testing_automation_provider.h"

#include "base/command_line.h"
#include "chrome/app/chrome_dll_resource.h"
#include "chrome/browser/automation/automation_browser_tracker.h"
#include "chrome/browser/automation/automation_provider_list.h"
#include "chrome/browser/automation/automation_provider_observers.h"
#include "chrome/browser/automation/automation_tab_tracker.h"
#include "chrome/browser/browser_window.h"
#include "chrome/browser/login_prompt.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/net/url_request_context_getter.h"
#include "chrome/common/notification_service.h"
#include "chrome/test/automation/automation_messages.h"
#include "net/url_request/url_request_context.h"

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

}  // namespace


TestingAutomationProvider::TestingAutomationProvider(Profile* profile)
    : AutomationProvider(profile),
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
